// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGripIK.h"
#include "Animation/AnimInstanceProxy.h"
#include "CheeseStrike.h"

void FCheeseStrikeGripIK::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_TwoBoneIK::CacheBones_AnyThread(Context);
	AnchorBone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
}

bool FCheeseStrikeGripIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return FAnimNode_TwoBoneIK::IsValidToEvaluate(Skeleton, RequiredBones) && AnchorBone.IsValidToEvaluate(RequiredBones);
}

void FCheeseStrikeGripIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex HandIndex = IKBone.GetCompactPoseIndex(Bones);
	const FTransform Anchor = Output.Pose.GetComponentSpaceTransform(AnchorBone.GetCompactPoseIndex(Bones));
	const FTransform Hand = Output.Pose.GetComponentSpaceTransform(HandIndex);
	if (!bGripMeasured)
	{
		Grip = Hand.GetRelativeTransform(Anchor);
		bGripMeasured = true;
		UE_LOG(LogCheeseStrike, Display, TEXT("Grip: %s sits at (%.1f, %.1f, %.1f) from %s, turned (%.0f, %.0f, %.0f)"), *IKBone.BoneName.ToString(), Grip.GetTranslation().X, Grip.GetTranslation().Y, Grip.GetTranslation().Z, *AnchorBone.BoneName.ToString(), Grip.Rotator().Pitch, Grip.Rotator().Yaw, Grip.Rotator().Roll);
	}
	// The target, where the grip is on the anchor as it stands now.
	const FTransform Target = Grip * Anchor;
	EffectorLocationSpace = BCS_ComponentSpace;
	EffectorLocation = Target.GetTranslation();
	// The elbow bends as the animation has it: a point beyond the elbow, away from the line from shoulder to hand.
	const FVector Shoulder = Output.Pose.GetComponentSpaceTransform(CachedUpperLimbIndex).GetTranslation();
	const FVector Elbow = Output.Pose.GetComponentSpaceTransform(CachedLowerLimbIndex).GetTranslation();
	const FVector Out = (Elbow - (Shoulder + Hand.GetTranslation()) * 0.5).GetSafeNormal();
	JointTargetLocationSpace = BCS_ComponentSpace;
	JointTargetLocation = Elbow + Out * ElbowReach;
	bTakeRotationFromEffectorSpace = false;
	bMaintainEffectorRelRot = false;

	FAnimNode_TwoBoneIK::EvaluateSkeletalControl_AnyThread(Output, OutBoneTransforms);

	// And the hand turns with the anchor.
	for (FBoneTransform& Bone : OutBoneTransforms)
	{
		if (Bone.BoneIndex == HandIndex)
		{
			Bone.Transform.SetRotation(Target.GetRotation());
		}
	}
}
