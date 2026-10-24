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
}

namespace
{
	bool DualBone(FBoneReference& Reference, const TCHAR* Name, const FBoneContainer& Bones)
	{
		Reference = FBoneReference(FName(Name));
		Reference.Initialize(Bones);
		return Reference.IsValidToEvaluate(Bones);
	}

	/** Puts an arm so that its hand is at `Target`, turned as it says, bending as it was. */
	void DualReach(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FTransform& Target)
	{
		FTransform UpperNow = Pose.GetComponentSpaceTransform(Upper);
		FTransform LowerNow = Pose.GetComponentSpaceTransform(Lower);
		FTransform EndNow = Pose.GetComponentSpaceTransform(End);
		const FVector Joint = LowerNow.GetLocation();
		const FVector Pole = Joint + (Joint - (UpperNow.GetLocation() + EndNow.GetLocation()) * 0.5).GetSafeNormal() * 50.0;
		AnimationCore::SolveTwoBoneIK(UpperNow, LowerNow, EndNow, Pole, Target.GetLocation(), true, 0.97, 1.08);
		EndNow.SetRotation(Target.GetRotation());
		const FBoneTransform Set[] = {FBoneTransform(Upper, UpperNow), FBoneTransform(Lower, LowerNow), FBoneTransform(End, EndNow)};
		Pose.SafeSetCSBoneTransforms(Set);
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
	// The right hand out to the right; the left where it would be held the mirror way.
	FTransform RightTarget = RightNow;
	RightTarget.AddToTranslation(Across * Spread * Alpha);
	const FTransform Mirror = Correction * CheeseStrikeDual::Mirrored(RightTarget, Origin, Across);
	FTransform LeftTarget = LeftNow;
	LeftTarget.SetLocation(FMath::Lerp(LeftNow.GetLocation(), Mirror.GetLocation(), static_cast<double>(Alpha)));
	LeftTarget.SetRotation(FQuat::Slerp(LeftNow.GetRotation(), Mirror.GetRotation(), Alpha).GetNormalized());
	DualReach(Pose, UpperR.GetCompactPoseIndex(Bones), LowerR.GetCompactPoseIndex(Bones), RightHand, RightTarget);
	DualReach(Pose, UpperL.GetCompactPoseIndex(Bones), LowerL.GetCompactPoseIndex(Bones), LeftHand, LeftTarget);
}
