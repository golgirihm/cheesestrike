// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeDownedPose.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"
#include "CheeseStrike.h"

namespace CheeseStrikeDowned
{
	float Breath(float Phase)
	{
		// A third of the breath drawing in, two thirds letting it out.
		const float Cycle = FMath::Fmod(Phase / UE_TWO_PI, 1.0f);
		const float At = Cycle < 0.0f ? Cycle + 1.0f : Cycle;
		const float Shaped = At < 0.35f ? 0.5f * At / 0.35f : 0.5f + 0.5f * (At - 0.35f) / 0.65f;
		return -FMath::Cos(Shaped * UE_TWO_PI);
	}
}

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
	// The ball of the foot only says how long the foot is.
	Bone(BallR, TEXT("ball_r"));
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
	const FVector Up = FVector::UpVector;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	// The right side, from the hips (whichever way the axes turn).
	FVector Right = At(Index(ThighR)) - At(Index(ThighL));
	Right = (Right - Ahead * FVector::DotProduct(Right, Ahead) - Up * Right.Z).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return;
	}

	// The legs as they come in: where the feet stand (the floor is at 0, the root's), and how long the bones are.
	const FVector FootLWas = At(Index(FootL));
	const FVector FootRWas = At(Index(FootR));
	const FVector KneeLWas = At(Index(CalfL));
	const FVector KneeRWas = At(Index(CalfR));
	const float Thigh = FVector::Dist(At(Index(ThighR)), KneeRWas);
	const float Shin = FVector::Dist(KneeRWas, FootRWas);
	const float Ankle = FMath::Clamp(FMath::Min(FootLWas.Z, FootRWas.Z), 4.0f, 20.0f);
	const float Foot = BallR.IsValidToEvaluate(Bones) ? FVector::Dist(FootRWas, At(Index(BallR))) : Shin * 0.3f;
	// The knee on the ground is about as high as the ankle is.
	const float KneeHeight = Ankle * 0.9f;

	// The hips go down until the right thigh, leaning forward a little, stands on its knee on the ground; moving, a little less.
	{
		const float HipWanted = KneeHeight + Thigh * FMath::Cos(FMath::DegreesToRadians(ThighLean));
		const float Drop = FMath::Max(0.0f, At(Index(ThighR)).Z - HipWanted) * FMath::Lerp(0.75f, 1.0f, Kneel);
		FTransform Hips = Pose.GetComponentSpaceTransform(Index(Pelvis));
		Hips.AddToTranslation(Up * (BreathLift * Breath - Drop));
		const FBoneTransform Set(Index(Pelvis), Hips);
		Pose.SafeSetCSBoneTransforms(MakeArrayView(&Set, 1));
	}

	// The right knee on the ground ahead of its hip, the shin back along the ground, the toes down.
	{
		const FVector Hip = At(Index(ThighR));
		const float Rise = Hip.Z - KneeHeight;
		const FVector Knee = FVector(Hip.X, Hip.Y, KneeHeight) + Ahead * FMath::Sqrt(FMath::Max(0.0f, Thigh * Thigh - Rise * Rise)) + Right * 3.0f;
		const float AnkleUp = FMath::Clamp((Foot * 0.75f - KneeHeight) / FMath::Max(Shin, 1.0f), 0.0f, 0.5f);
		const FVector Kneeling = Knee + (-Ahead * FMath::Sqrt(1.0f - AnkleUp * AnkleUp) + Up * AnkleUp) * Shin;
		const FVector Effector = FMath::Lerp(FootRWas, Kneeling, Kneel);
		const FVector Bend = FMath::Lerp(KneeRWas + Ahead * 40.0f, Knee + Ahead * 20.0f, Kneel);
		DownedReach(Pose, Index(ThighR), Index(CalfR), Index(FootR), Effector, Bend, Kneel > 0.5f);
	}
	// The left foot flat on the ground ahead, its knee up.
	{
		const FVector Hip = At(Index(ThighL));
		const FVector Planted = FVector(Hip.X, Hip.Y, FootLWas.Z) + Ahead * Thigh * 0.8f - Right * 6.0f;
		const FVector Effector = FMath::Lerp(FootLWas, Planted, Kneel);
		const FVector Bend = FMath::Lerp(KneeLWas + Ahead * 40.0f, Planted + Up * Shin + Ahead * 30.0f, Kneel);
		DownedReach(Pose, Index(ThighL), Index(CalfL), Index(FootL), Effector, Bend, false);
	}

	// Hunched forward and a little to the hurt side, the chest heaving; the neck and head come back up to look ahead.
	float Bent = 0.0f;
	for (int32 Each = 0; Each < 3; ++Each)
	{
		const float Bend = SpineBend[Each] - (Each > 0 ? BreathBend * 0.5f * Breath : 0.0f);
		Bent += Bend;
		DownedTurn(Pose, Index(Spine[Each]), DownedToward(Up, Ahead, Bend) * DownedToward(Up, Right, Each < 2 ? SideLean * 0.5f : 0.0f));
	}
	DownedTurn(Pose, Index(Neck), DownedToward(Up, Ahead, -NeckBack));
	DownedTurn(Pose, Index(Head), DownedToward(Up, Ahead, -HeadBack));
	// The pistol arm comes up again by what the spine took down, so that it points where it did.
	DownedTurn(Pose, Index(UpperArmR), DownedToward(Up, Ahead, -Bent));

	// The left hand on the belly, a little to its side, the elbow out.
	if (HandOnWound > 0.001f)
	{
		const FVector Belly = At(Index(Spine[0]));
		const FVector Wound = Belly + Ahead * 16.0f - Right * 5.0f + Up * 2.0f;
		const FVector Hand = At(Index(HandL));
		const FVector Elbow = At(Index(LowerArmL));
		DownedReach(Pose, Index(UpperArmL), Index(LowerArmL), Index(HandL), FMath::Lerp(Hand, Wound, HandOnWound),
			FMath::Lerp(Elbow + (Elbow - Hand).GetSafeNormal() * 30.0f, Wound - Right * 30.0f - Ahead * 5.0f, HandOnWound), true);
	}
}
