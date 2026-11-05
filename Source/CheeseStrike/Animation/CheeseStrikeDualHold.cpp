// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeDualHold.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"

namespace CheeseStrikeDual
{
	FTransform Mirrored(const FTransform& Held, const FVector& Origin, const FVector& Across)
	{
		const FVector Normal = Across.GetSafeNormal();
		const FVector At = Held.GetLocation();
		const FVector MirroredAt = At - 2.0 * FVector::DotProduct(At - Origin, Normal) * Normal;
		// The reflection, I - 2 n n, on both sides of the turn.
		FMatrix Reflect = FMatrix::Identity;
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				Reflect.M[Row][Column] -= 2.0 * Normal[Row] * Normal[Column];
			}
		}
		const FMatrix Turn = Reflect * FRotationMatrix::Make(Held.GetRotation()) * Reflect;
		return FTransform(Turn.ToQuat().GetNormalized(), MirroredAt, Held.GetScale3D());
	}

	FTransform Aligned(const FTransform& Hand, const FVector& AlongInHand, const FVector& Forward, const FVector& Up, float InDegrees)
	{
		const FVector UpNow = Up.GetSafeNormal();
		const FVector AlongNow = Hand.GetRotation().RotateVector(AlongInHand).GetSafeNormal();
		const FVector Flat = (Forward - UpNow * FVector::DotProduct(Forward, UpNow)).GetSafeNormal();
		if (AlongNow.IsNearlyZero() || Flat.IsNearlyZero())
		{
			return Hand;
		}
		// A turn about up by a positive angle takes forward toward the right: in toward the left is the other way. The
		// pitch stays the gun's own (the aim offsets' up and down).
		const FVector InYaw = Flat.RotateAngleAxis(-InDegrees, UpNow).GetSafeNormal();
		const float Rise = FMath::Clamp(static_cast<float>(FVector::DotProduct(AlongNow, UpNow)), -1.0f, 1.0f);
		const FVector Wanted = (InYaw * FMath::Sqrt(FMath::Max(1.0f - Rise * Rise, 0.0f)) + UpNow * Rise).GetSafeNormal();
		FTransform Turned = Hand;
		Turned.SetRotation((FQuat::FindBetweenNormals(AlongNow, Wanted) * Hand.GetRotation()).GetNormalized());
		return Turned;
	}
}

namespace
{
	bool DualBone(FBoneReference& Reference, const TCHAR* Name, const FBoneContainer& Bones)
	{
		Reference = FBoneReference(FName(Name));
		Reference.Initialize(Bones);
		return Reference.IsValidToEvaluate(Bones);
	}

	/** Puts an arm so that its hand is at `Target`, turned as it says, its elbow bent toward `Pole`; returns where the elbow went. */
	FVector DualReach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FTransform& Target, const FVector& Pole)
	{
		FTransform UpperNow = Pose.GetComponentSpaceTransform(Upper);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(Lower);
		FTransform EndNow = Pose.GetComponentSpaceTransform(End);
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, Pole, Target.GetLocation(), true, 0.97, 1.08);
		EndNow.SetRotation(Target.GetRotation());
		const FBoneTransform Set[] = {FBoneTransform(Upper, UpperNow), FBoneTransform(Lower, LowerNow), FBoneTransform(End, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
		return LowerNow.GetLocation();
	}

	/** `At` mirrored across the plane through `Origin` whose normal is `Across`. */
	FVector DualMirrored(const FVector& At, const FVector& Origin, const FVector& Across)
	{
		return At - 2.0 * FVector::DotProduct(At - Origin, Across) * Across;
	}
}

void FCheeseStrikeDualHold::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	ComponentPose.Initialize(Context);
}

void FCheeseStrikeDualHold::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ComponentPose.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	bBones = DualBone(UpperR, TEXT("upperarm_r"), Bones) && DualBone(LowerR, TEXT("lowerarm_r"), Bones) && DualBone(HandR, TEXT("hand_r"), Bones)
		&& DualBone(UpperL, TEXT("upperarm_l"), Bones) && DualBone(LowerL, TEXT("lowerarm_l"), Bones) && DualBone(HandL, TEXT("hand_l"), Bones);
	Correction = FTransform::Identity;
	if (!bBones)
	{
		return;
	}
	// In the reference pose, which stands the same both sides about the body's middle (its X across): the left hand's
	// frame against the right hand's mirrored.
	const FReferenceSkeleton& Skeleton = Bones.GetReferenceSkeleton();
	const int32 Right = Skeleton.FindBoneIndex(HandR.BoneName);
	const int32 Left = Skeleton.FindBoneIndex(HandL.BoneName);
	if (Right != INDEX_NONE && Left != INDEX_NONE)
	{
		const FTransform RightInBody = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Right);
		const FTransform LeftInBody = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Left);
		const FTransform MirroredRight = CheeseStrikeDual::Mirrored(RightInBody, FVector::ZeroVector, FVector::XAxisVector);
		Correction = LeftInBody.GetRelativeTransform(MirroredRight);
		Correction.SetScale3D(FVector::OneVector);
	}
}

void FCheeseStrikeDualHold::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FCheeseStrikeDualHold::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	ComponentPose.EvaluateComponentSpace(Output);
	if (!bBones || Alpha <= 0.001f)
	{
		return;
	}
	FCSPose<FCompactPose>& Pose = Output.Pose;
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex RightHand = HandR.GetCompactPoseIndex(Bones);
	const FCompactPoseBoneIndex LeftHand = HandL.GetCompactPoseIndex(Bones);
	// Across the body, toward its right: the body faces Forward, up is up.
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FTransform RightNow = Pose.GetComponentSpaceTransform(RightHand);
	const FTransform LeftNow = Pose.GetComponentSpaceTransform(LeftHand);
	// The right gun laid along the body's forward (the stance's own yaw may be a little out), turned in a little toward
	// the middle when aimed; the right hand out to the right: on the body others see, to HipHalfApart of the middle at
	// the hip (the two about a shoulder's width apart) and AimSpread out from where the aimed stance has it (the two side
	// by side before the eyes); in its own player's view, pushed out no further (Spread and AimSpread are 0 there).
	const float Aimed = FMath::Clamp(Aim, 0.0f, 1.0f);
	const FVector AlongInHand = ModelInHand.GetRotation().RotateVector(ModelAlong);
	const FTransform Laid = CheeseStrikeDual::Aligned(RightNow, AlongInHand, Forward, FVector::UpVector, CheeseStrikeDual::Converge * Aimed);
	FTransform RightTarget = RightNow;
	RightTarget.SetRotation(FQuat::Slerp(RightNow.GetRotation(), Laid.GetRotation(), Alpha).GetNormalized());
	const float HipOut = bFromMiddle ? FMath::Max(0.0f, Spread - static_cast<float>(FVector::DotProduct(RightNow.GetLocation() - Origin, Across))) : Spread;
	RightTarget.AddToTranslation(Across * FMath::Lerp(HipOut, AimSpread, Aimed) * Alpha);
	// The left hand where the right would be held the mirror way round.
	const FTransform Mirror = Correction * CheeseStrikeDual::Mirrored(RightTarget, Origin, Across);
	FTransform LeftTarget = LeftNow;
	LeftTarget.SetLocation(FMath::Lerp(LeftNow.GetLocation(), Mirror.GetLocation(), static_cast<double>(Alpha)));
	LeftTarget.SetRotation(FQuat::Slerp(LeftNow.GetRotation(), Mirror.GetRotation(), Alpha).GetNormalized());
	// The right wrist straight: its elbow bent toward a point a forearm behind the hand along its gun, a little out and
	// down (the stance's bend, which brought the elbow in under a two-handed hold, otherwise); and the left arm the mirror
	// of the right, elbow and all, so that the two are held alike.
	const FCompactPoseBoneIndex LowerRight = LowerR.GetCompactPoseIndex(Bones);
	const FVector ElbowNow = Pose.GetComponentSpaceTransform(LowerRight).GetLocation();
	const FVector Bore = RightTarget.GetRotation().RotateVector(AlongInHand).GetSafeNormal();
	const float Forearm = FVector::Dist(ElbowNow, RightNow.GetLocation());
	const FVector StancePole = ElbowNow + (ElbowNow - (Pose.GetComponentSpaceTransform(UpperR.GetCompactPoseIndex(Bones)).GetLocation() + RightNow.GetLocation()) * 0.5).GetSafeNormal() * 50.0;
	const FVector StraightPole = RightTarget.GetLocation() - Bore * Forearm + Across * CheeseStrikeDual::ElbowOut - FVector::UpVector * CheeseStrikeDual::ElbowDown;
	const FVector RightPole = FMath::Lerp(StancePole, StraightPole, static_cast<double>(Alpha));
	const FVector RightElbow = DualReach(Pose, UpperR.GetCompactPoseIndex(Bones), LowerRight, RightHand, RightTarget, RightPole);
	const FVector LeftElbowNow = Pose.GetComponentSpaceTransform(LowerL.GetCompactPoseIndex(Bones)).GetLocation();
	const FVector LeftStancePole = LeftElbowNow + (LeftElbowNow - (Pose.GetComponentSpaceTransform(UpperL.GetCompactPoseIndex(Bones)).GetLocation() + LeftNow.GetLocation()) * 0.5).GetSafeNormal() * 50.0;
	const FVector LeftPole = FMath::Lerp(LeftStancePole, DualMirrored(RightElbow, Origin, Across), static_cast<double>(Alpha));
	DualReach(Pose, UpperL.GetCompactPoseIndex(Bones), LowerL.GetCompactPoseIndex(Bones), LeftHand, LeftTarget, LeftPole);
}
