// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeDownedPose.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"
#include "CheeseStrike.h"

namespace
{
	/** The turn that takes `From` `Degrees` of the way towards `To`. */
	FQuat DownedToward(const FVector& From, const FVector& To, float Degrees)
	{
		const FVector Axis = FVector::CrossProduct(From, To).GetSafeNormal();
		return Axis.IsNearlyZero() ? FQuat::Identity : FQuat(Axis, FMath::DegreesToRadians(Degrees));
	}

	/** Turns a bone about itself, its children with it. */
	void DownedTurn(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FQuat& Turn)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetRotation((Turn * Now.GetRotation()).GetNormalized());
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	/** Sets a bone's turn where it is, its children with it. */
	void DownedSetTurn(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Bone, const FQuat& Turn)
	{
		FTransform Now = Pose.GetComponentSpaceTransform(Bone);
		Now.SetRotation(Turn.GetNormalized());
		const FBoneTransform Set(Bone, Now);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	/** Puts a limb (upper, lower, end) so that its end reaches `Effector`, bending towards `JointTarget`; the end turns with the lower bone if `bEndFollows`, else keeps its turn. */
	void DownedReach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FVector& Effector, const FVector& JointTarget, bool bEndFollows)
	{
		FTransform UpperNow = Pose.GetComponentSpaceTransform(Upper);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(Lower);
		FTransform EndNow = Pose.GetComponentSpaceTransform(End);
		const FQuat LowerWas = LowerNow.GetRotation();
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, JointTarget, Effector, false, 1.0, 1.0);
		if (bEndFollows)
		{
			EndNow.SetRotation((LowerNow.GetRotation() * LowerWas.Inverse() * EndNow.GetRotation()).GetNormalized());
		}
		const FBoneTransform Set[] = {FBoneTransform(Upper, UpperNow), FBoneTransform(Lower, LowerNow), FBoneTransform(End, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
	}
}

void FCheeseStrikeDownedPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeDownedPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	const auto Bone = [&Bones](FBoneReference& Reference, const TCHAR* Name)
	{
		Reference = FBoneReference(FName(Name));
		Reference.Initialize(Bones);
		return Reference.IsValidToEvaluate(Bones);
	};
	bool bAll = Bone(Pelvis, TEXT("pelvis"));
	bAll &= Bone(Spine[0], TEXT("spine_01"));
	bAll &= Bone(Spine[1], TEXT("spine_02"));
	bAll &= Bone(Spine[2], TEXT("spine_03"));
	bAll &= Bone(Neck, TEXT("neck_01"));
	bAll &= Bone(Head, TEXT("head"));
	bAll &= Bone(ThighL, TEXT("thigh_l"));
	bAll &= Bone(CalfL, TEXT("calf_l"));
	bAll &= Bone(FootL, TEXT("foot_l"));
	bAll &= Bone(ThighR, TEXT("thigh_r"));
	bAll &= Bone(CalfR, TEXT("calf_r"));
	bAll &= Bone(FootR, TEXT("foot_r"));
	bAll &= Bone(UpperArmL, TEXT("upperarm_l"));
	bAll &= Bone(LowerArmL, TEXT("lowerarm_l"));
	bAll &= Bone(HandL, TEXT("hand_l"));
	bAll &= Bone(UpperArmR, TEXT("upperarm_r"));
	bAll &= Bone(LowerArmR, TEXT("lowerarm_r"));
	bAll &= Bone(HandR, TEXT("hand_r"));
	// A body cut down to fewer bones (at a distance) does without the pose rather than half of it.
	bBones = bAll;
	if (!bAll && !bToldMissing && Bones.GetCompactPoseNumBones() > 40)
	{
		bToldMissing = true;
		UE_LOG(LogCheeseStrike, Warning, TEXT("The pose for a player down is missing bones of the mannequin's on this body"));
	}
}

void FCheeseStrikeDownedPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeDownedPose::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	ComponentPose.EvaluateComponentSpace(Output);
	if (Alpha <= 0.001f || !bBones)
	{
		return;
	}
	if (Alpha >= 0.999f)
	{
		GoDown(Output.Pose);
		return;
	}
	// Part of the way: the pose coming in and the one all the way down, blended bone by bone in local space.
	// (The conversion fills in only the bones in component space: the rest are local already.)
	FCompactPose Up = Output.Pose.GetPose();
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Output.Pose, Up);
	GoDown(Output.Pose);
	FCompactPose Down = Output.Pose.GetPose();
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Output.Pose, Down);
	for (const FCompactPoseBoneIndex Index : Down.ForEachBoneIndex())
	{
		FTransform Blended;
		Blended.Blend(Up[Index], Down[Index], Alpha);
		Down[Index] = Blended;
	}
	Output.Pose.InitPose(MoveTemp(Down));
}

void FCheeseStrikeDownedPose::GoDown(FCSPose<FCompactPose>& Pose) const
{
	using namespace CheeseStrikeDowned;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const auto Index = [&Bones](const FBoneReference& Reference) { return Reference.GetCompactPoseIndex(Bones); };
	const auto At = [&Pose](FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetLocation(); };
	const auto TurnOf = [&Pose](FCompactPoseBoneIndex Bone) { return Pose.GetComponentSpaceTransform(Bone).GetRotation(); };
	const FVector Up = FVector::UpVector;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	// The right side, from the hips (whichever way the axes turn).
	FVector Right = At(Index(ThighR)) - At(Index(ThighL));
	Right = (Right - Ahead * FVector::DotProduct(Right, Ahead) - Up * Right.Z).GetSafeNormal();
	if (Ahead.IsNearlyZero() || Right.IsNearlyZero())
	{
		return;
	}

	// What comes in: how long the limbs are, and how the head and the pistol hand are turned with the aim straight ahead.
	const float LegFull = FVector::Dist(At(Index(ThighR)), At(Index(CalfR))) + FVector::Dist(At(Index(CalfR)), At(Index(FootR)));
	const float UpperArmLength = FVector::Dist(At(Index(UpperArmL)), At(Index(LowerArmL)));
	const float ForearmLength = FVector::Dist(At(Index(LowerArmL)), At(Index(HandL)));
	const float ArmR = FVector::Dist(At(Index(UpperArmR)), At(Index(LowerArmR))) + FVector::Dist(At(Index(LowerArmR)), At(Index(HandR)));
	const FQuat HeadWas = TurnOf(Index(Head));
	const FQuat HandWas = TurnOf(Index(HandR));
	// Where the legs are in their pushes, -1 to 1, as much as they are pushing.
	const float Push = FMath::Sin(ScootPhase) * Scoot;

	// The chest squared to the hips first (the pistol stance turns it), so that lying back doesn't roll it onto a side.
	{
		FVector Shoulders = At(Index(UpperArmR)) - At(Index(UpperArmL));
		Shoulders = FVector(Shoulders.X, Shoulders.Y, 0.0).GetSafeNormal();
		if (!Shoulders.IsNearlyZero())
		{
			const float Off = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Shoulders, Right), -1.0f, 1.0f)));
			DownedTurn(Pose, Index(Spine[0]), DownedToward(Shoulders, Right, Off));
		}
	}
	// The pelvis laid back on the floor where the hips lie, shuffling and twisting a little with each push.
	{
		FTransform Hips = Pose.GetComponentSpaceTransform(Index(Pelvis));
		const FQuat Lain = DownedToward(Ahead, Right, HipTwist * Push) * DownedToward(Up, -Ahead, PelvisRecline);
		Hips.SetRotation((Lain * Hips.GetRotation()).GetNormalized());
		Hips.SetLocation(Origin + Ahead * HipsAhead + Up * (PelvisHeight + HipLift * FMath::Abs(Push)) + Right * HipShuffle * Push);
		const FBoneTransform Set(Index(Pelvis), Hips);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}
	// The spine curled up off the floor, the chest heaving.
	for (int32 Each = 0; Each < 3; ++Each)
	{
		DownedTurn(Pose, Index(Spine[Each]), DownedToward(Up, Ahead, SpineCurl[Each] + (Each > 0 ? BreathBend * 0.5f * Breath : 0.0f)));
	}
	// The shoulders level, the left (on its elbow) a little higher.
	{
		const FVector Shoulders = (At(Index(UpperArmR)) - At(Index(UpperArmL))).GetSafeNormal();
		const FVector Level = (FVector(Shoulders.X, Shoulders.Y, 0.0).GetSafeNormal() - Up * 0.08f).GetSafeNormal();
		if (!Shoulders.IsNearlyZero() && !Level.IsNearlyZero())
		{
			DownedTurn(Pose, Index(Spine[1]), FQuat::FindBetweenNormals(Shoulders, Level));
		}
	}
	// Propped on the left elbow: the torso turned about the hips until the left shoulder is an upper arm (and an elbow) off the floor.
	{
		const FVector Hips = At(Index(Pelvis));
		FVector Torso = At(Index(UpperArmL)) - Hips;
		Torso -= Right * FVector::DotProduct(Torso, Right);
		const float Length = Torso.Size();
		if (Length > 1.0f)
		{
			const float Now = FMath::Atan2(Torso.Z, -FVector::DotProduct(Torso, Ahead));
			const float Wanted = FMath::Asin(FMath::Clamp((UpperArmLength * 0.95f + ElbowHeight - Hips.Z) / Length, -0.2f, 0.95f));
			DownedTurn(Pose, Index(Pelvis), DownedToward(Up, Ahead, FMath::RadiansToDegrees(Wanted - Now)));
		}
	}

	// The chest turns toward the aim, a share of the way.
	DownedTurn(Pose, Index(Spine[2]), DownedToward(Ahead, Right, AimYaw * ChestTurn));
	// The aim, from the way the body faces: so far to the right, then so far up.
	const FQuat Yawed = DownedToward(Ahead, Right, AimYaw);
	const FQuat AimTurn = DownedToward(Yawed.RotateVector(Ahead), Up, AimPitch) * Yawed;
	const FVector Aim = AimTurn.RotateVector(Ahead);
	// The head looks along it, much of the turn in the neck.
	{
		const FQuat Wanted = (AimTurn * HeadWas).GetNormalized();
		const FQuat Still = Wanted * TurnOf(Index(Head)).Inverse();
		DownedTurn(Pose, Index(Neck), FQuat::Slerp(FQuat::Identity, Still, NeckShare));
		DownedSetTurn(Pose, Index(Head), Wanted);
	}

	// The arms, as far as they are the pose's.
	if (ArmsOwn > 0.001f)
	{
		// The left elbow on the floor under the shoulder, a little out and back, the forearm along the floor ahead.
		const FVector Shoulder = At(Index(UpperArmL));
		const FVector Elbow = FVector(Shoulder.X, Shoulder.Y, 4.0) - Right * 7.0f - Ahead * 4.0f;
		FVector Hand = Elbow + (Ahead - Right * 0.25f).GetSafeNormal() * ForearmLength * 0.95f;
		Hand.Z = 3.0f;
		DownedReach(Pose, Index(UpperArmL), Index(LowerArmL), Index(HandL), FMath::Lerp(At(Index(HandL)), Hand, ArmsOwn), Elbow - Up * 10.0f - Right * 5.0f, true);
	}
	if (ArmsOwn > 0.001f)
	{
		// The pistol held out along the aim from the right shoulder, the elbow a little bent, down and out.
		const FVector Shoulder = At(Index(UpperArmR));
		const float Reach = ArmR * PistolReach;
		FVector Hand = Shoulder + Aim * Reach + Up * PistolUp - Right * PistolIn;
		// Its own player sees it from the eyes, the arm coming in from the right shoulder under the view, never across it.
		if (bFirstPerson)
		{
			const FVector AimRight = FVector::CrossProduct(Up, Aim).GetSafeNormal();
			const FVector AimUp = FVector::CrossProduct(Aim, AimRight).GetSafeNormal();
			Hand = At(Index(Head)) + Aim * FirstPersonPistolOut + AimRight * FirstPersonPistolRight * (FVector::DotProduct(AimRight, Right) >= 0.0f ? 1.0f : -1.0f) - AimUp * FirstPersonPistolUnder;
		}
		DownedReach(Pose, Index(UpperArmR), Index(LowerArmR), Index(HandR), FMath::Lerp(At(Index(HandR)), Hand, ArmsOwn), Shoulder + Aim * Reach * 0.5f + Right * 20.0f - Up * 15.0f, false);
		DownedSetTurn(Pose, Index(HandR), FQuat::Slerp(TurnOf(Index(HandR)), AimTurn * HandWas, ArmsOwn));
	}

	// The legs out along the floor ahead, as far as there is room: the right straight, the left knee up a little;
	// scooting, each pushes in turn, the foot coming back lifted.
	const struct
	{
		const FBoneReference& Thigh;
		const FBoneReference& Calf;
		const FBoneReference& Foot;
		float Side;
		float Out;
		float Phase;
	} Legs[] = {{ThighR, CalfR, FootR, 1.0f, RightLegOut, 0.0f}, {ThighL, CalfL, FootL, -1.0f, LeftLegOut, UE_PI}};
	for (const auto& Leg : Legs)
	{
		const FVector Hip = At(Index(Leg.Thigh));
		const float Drop = FMath::Max(0.0f, Hip.Z - AnkleHeight);
		const float Flat = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(LegFull * 0.99f) - Drop * Drop));
		const float Phase = ScootPhase + Leg.Phase;
		const float Share = FMath::Lerp(Leg.Out, FMath::Lerp(PushFrom, PushTo, 0.5f + 0.5f * FMath::Sin(Phase)), Scoot);
		const float Lift = FootLift * Scoot * FMath::Max(0.0f, -FMath::Cos(Phase));
		const float Along = FMath::Max(FMath::Min(Flat * Share, LegReach), LegFull * 0.35f);
		const FVector Foot = FVector(Hip.X, Hip.Y, AnkleHeight + Lift) + Ahead * Along + Right * Leg.Side * 4.0f;
		DownedReach(Pose, Index(Leg.Thigh), Index(Leg.Calf), Index(Leg.Foot), Foot, Hip + Ahead * Along * 0.5f + Up * 60.0f + Right * Leg.Side * 10.0f, true);
	}
}
