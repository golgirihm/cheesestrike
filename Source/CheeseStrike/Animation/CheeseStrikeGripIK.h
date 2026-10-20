// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"
#include "CheeseStrikeGripIK.generated.h"

/**
 * Keeps a hand on the weapon: two-bone IK whose target is a fixed place
 * and turn relative to the anchor bone that the weapon rides (weapon_r,
 * in the right hand), measured from the first pose evaluated: the animator's
 * own grip. The animations' ik_hand bones aren't used: the aim offsets
 * turn the spine, and the hands with it, but not those bones, so reaching
 * for them twists the arm more the further the aim is from level. The
 * elbow bends the way the animation has it in the pose coming in, so the
 * arm keeps its shape; nothing comes from an earlier frame.
 */
USTRUCT()
struct FCheeseStrikeGripIK : public FAnimNode_TwoBoneIK
{
	GENERATED_BODY()

	/** The bone the weapon rides, which the hand follows. */
	UPROPERTY()
	FBoneReference AnchorBone;

	/** Where the hand is relative to the anchor, once measured. */
	FTransform Grip = FTransform::Identity;
	bool bGripMeasured = false;
	/** How far beyond the elbow the bend is aimed, in cm. */
	float ElbowReach = 50.0f;

	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
};
