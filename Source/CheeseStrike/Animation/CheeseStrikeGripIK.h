// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_TwoBoneIK.h"
#include "CheeseStrikeGripIK.generated.h"

class UAnimSequence;

/** The numbers the hands are held on a weapon by. */
namespace CheeseStrikeGrip
{
	/** The bones of a hand whose middle is where it closes round what it holds: the middle finger's three and the thumb's middle joint (with the hand's side, "_l" or "_r", after them). */
	inline const TCHAR* const PalmBones[] = {TEXT("middle_01"), TEXT("middle_02"), TEXT("middle_03"), TEXT("thumb_02")};
	/** How much longer than it is an arm may stretch to keep its hand on the weapon (a share: 1.06 is six per cent), and from what share of its length it begins to. */
	constexpr float MostStretch = 1.06f;
	constexpr float StretchFrom = 0.97f;
}

/**
 * Keeps a hand on the weapon: two-bone IK whose target is a fixed place and
 * turn relative to the anchor bone that the weapon rides (hand_r). The
 * hand's turn on the weapon is the animator's own: the one the set's idle
 * (`GripPose`, at its first frame) gives it relative to the anchor,
 * measured once for each set, whatever the body happens to be doing when
 * the set changes (running, falling, guarding with the knife). Where it
 * holds is the weapon's (`Hold`: the point, in the anchor's space, its hand
 * closes round, on the fore-end of a long gun, round the other hand on a
 * pistol), the hand placed so that the middle of its closed fingers
 * (CheeseStrikeGrip::PalmBones, measured in the same pose) is there:
 * Paragon's heroes are built differently, and their retargeted animations
 * put the free hand anywhere from the fore-end to past the muzzle. Without a
 * hold, the idle's own place.
 *
 * The animations' ik_hand bones aren't used: the aim offsets turn the
 * spine, and the hands with it, but not those bones. The elbow bends the
 * way the animation has it in the pose coming in, and the arm stretches a
 * little (CheeseStrikeGrip::MostStretch) rather than let go. Nothing in it
 * is particular to the left hand or a long gun.
 */
USTRUCT()
struct FCheeseStrikeGripIK : public FAnimNode_TwoBoneIK
{
	GENERATED_BODY()

	/** The bone the weapon rides, which the hand follows. */
	UPROPERTY()
	FBoneReference AnchorBone;
	/** The set's idle, which the grip is measured from. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> GripPose;

	/** Where the hand closes round the weapon, in the anchor's space (cm), if the weapon says. */
	bool bHasHold = false;
	FVector Hold = FVector::ZeroVector;

	/** The hand's place and turn relative to the anchor in the idle, and where the middle of its closed fingers is in the hand's own space, once measured. */
	FTransform Grip = FTransform::Identity;
	FVector Palm = FVector::ZeroVector;
	bool bGripMeasured = false;
	/** How far beyond the elbow the bend is aimed, in cm. */
	float ElbowReach = 50.0f;

	/** Measures the grip in `Pose` (component space): the hand relative to the anchor, and the middle of its fingers in the hand's space. */
	void MeasureGrip(FCSPose<FCompactPose>& Pose);
	/** Where the hand goes relative to the anchor: the measured grip, moved to the weapon's hold if it has one. */
	FTransform HandOnAnchor() const;

	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;

private:
	FBoneReference PalmReferences[UE_ARRAY_COUNT(CheeseStrikeGrip::PalmBones)];
};
