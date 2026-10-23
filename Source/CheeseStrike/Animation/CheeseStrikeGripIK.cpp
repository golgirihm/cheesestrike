// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGripIK.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "CheeseStrike.h"

void FCheeseStrikeGripIK::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_TwoBoneIK::CacheBones_AnyThread(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	AnchorBone.Initialize(Bones);
	// The palm's bones on the hand's own side.
	const FString Name = IKBone.BoneName.ToString();
	const FString Side = Name.Len() > 2 && Name[Name.Len() - 2] == TEXT('_') ? Name.Right(2) : FString(TEXT("_l"));
	for (int32 Each = 0; Each < UE_ARRAY_COUNT(CheeseStrikeGrip::PalmBones); ++Each)
	{
		PalmReferences[Each] = FBoneReference(FName(FString(CheeseStrikeGrip::PalmBones[Each]) + Side));
		PalmReferences[Each].Initialize(Bones);
	}
	// The bones may have changed with the mesh: measured afresh.
	bGripMeasured = false;
}

bool FCheeseStrikeGripIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return FAnimNode_TwoBoneIK::IsValidToEvaluate(Skeleton, RequiredBones) && AnchorBone.IsValidToEvaluate(RequiredBones);
}

void FCheeseStrikeGripIK::MeasureGrip(FCSPose<FCompactPose>& Pose)
{
	const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
	const FTransform Hand = Pose.GetComponentSpaceTransform(IKBone.GetCompactPoseIndex(Bones));
	const FTransform Anchor = Pose.GetComponentSpaceTransform(AnchorBone.GetCompactPoseIndex(Bones));
	Grip = Hand.GetRelativeTransform(Anchor);
	Grip.SetScale3D(FVector::OneVector);
	// The middle of the closed fingers, in the hand's space; a hand without them closes about 8 cm out from the wrist.
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const FBoneReference& Bone : PalmReferences)
	{
		if (Bone.IsValidToEvaluate(Bones))
		{
			Sum += Pose.GetComponentSpaceTransform(Bone.GetCompactPoseIndex(Bones)).GetLocation();
			++Count;
		}
	}
	Palm = Count == UE_ARRAY_COUNT(PalmReferences) ? Hand.InverseTransformPosition(Sum / Count) : FVector::ZeroVector;
	bGripMeasured = true;
	UE_LOG(LogCheeseStrike, Verbose, TEXT("Grip: %s sits at (%.1f, %.1f, %.1f) from %s, turned (%.0f, %.0f, %.0f), its fingers closing at (%.1f, %.1f, %.1f)"), *IKBone.BoneName.ToString(),
		Grip.GetTranslation().X, Grip.GetTranslation().Y, Grip.GetTranslation().Z, *AnchorBone.BoneName.ToString(), Grip.Rotator().Pitch, Grip.Rotator().Yaw, Grip.Rotator().Roll, Palm.X, Palm.Y, Palm.Z);
}

FTransform FCheeseStrikeGripIK::HandOnAnchor() const
{
	FTransform On = Grip;
	if (bHasHold)
	{
		// The hand turned as the idle has it, put so that its fingers close round the hold.
		On.SetTranslation(Hold - Grip.GetRotation().RotateVector(Palm));
	}
	return On;
}

void FCheeseStrikeGripIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	if (!bGripMeasured)
	{
		if (GripPose != nullptr && GripPose->GetSkeleton() == Output.AnimInstanceProxy->GetSkeleton())
		{
			// The idle's first frame, whatever the body is doing now.
			FPoseContext Idle(Output.AnimInstanceProxy);
			FAnimationPoseData Data(Idle);
			GripPose->GetAnimationPose(Data, FAnimExtractContext(0.0));
			FCSPose<FCompactPose> Measured;
			Measured.InitPose(Idle.Pose);
			MeasureGrip(Measured);
		}
		else
		{
			MeasureGrip(Output.Pose);
		}
	}
	const FCompactPoseBoneIndex HandIndex = IKBone.GetCompactPoseIndex(Bones);
	const FTransform Anchor = Output.Pose.GetComponentSpaceTransform(AnchorBone.GetCompactPoseIndex(Bones));
	const FTransform Hand = Output.Pose.GetComponentSpaceTransform(HandIndex);
	// The target, where the grip is on the anchor as it stands now.
	const FTransform Target = HandOnAnchor() * Anchor;
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
	bAllowStretching = true;
	StartStretchRatio = CheeseStrikeGrip::StretchFrom;
	MaxStretchScale = CheeseStrikeGrip::MostStretch;

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
