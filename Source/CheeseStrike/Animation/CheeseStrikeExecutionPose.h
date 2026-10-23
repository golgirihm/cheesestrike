// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "Rules/CheeseStrikeExecution.h"

/**
 * The pose of an execution (CheeseStrikeExecution), on both bodies in it,
 * made in component space from whatever pose comes in (the executor
 * standing with the weapon in hand; the one down, lying as
 * FCheeseStrikeDownedPose has them), by the bone names every hero shares,
 * so that it fits every body. Keyframed in code: each limb's target moves
 * between a few places at set times, eased, and two-bone IK reaches it.
 *
 * The executor's stab: down on the right knee beside the chest, the left
 * foot planted ahead, leaning over; the left hand pins the near shoulder,
 * the right raises the knife overhead and drives it down into the chest,
 * twice, then pulls it out and stands. Their stomp: stood at the head, the
 * right knee comes up high, the boot over the face, and slams down on it,
 * the weight following through, grinds, and steps back.
 *
 * The one down: at each blow the chest jerks and the head is thrown back,
 * the pistol arm falling to the floor at the first; under the boot, the
 * pistol arm comes up to shield the face, too late.
 */
struct FCheeseStrikeExecutionPose : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** Which execution, if any, whether this body deals it, and how far in it is (seconds). */
	CheeseStrikeExecution::EKind Kind = CheeseStrikeExecution::EKind::None;
	bool bExecutor = false;
	float Time = 0.0f;
	/** The way the body faces, in component space, and where its capsule's axis meets the floor there. */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Origin = FVector::ZeroVector;
	/** The executor: where the blow lands (the chest, or the head), and the near shoulder of the one down, in component space. */
	FVector Target = FVector::ZeroVector;
	FVector TargetShoulder = FVector::ZeroVector;
	/** The one down: which side of them the executor is (1 right, -1 left). */
	float Side = 1.0f;

	bool HasBones() const { return bBones; }

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

	/**
	 * How much the pose has the body now (0 to 1): the executor's comes in
	 * over the first moments and goes out as they stand again; the one
	 * down's stays (they die in it).
	 */
	static float Weight(CheeseStrikeExecution::EKind Kind, bool bExecutor, float Time);

private:
	void Stab(FCSPose<FCompactPose>& Pose) const;
	void Stomp(FCSPose<FCompactPose>& Pose) const;
	void Struck(FCSPose<FCompactPose>& Pose) const;

	FBoneReference Pelvis;
	FBoneReference Spine[3];
	FBoneReference Neck;
	FBoneReference Head;
	FBoneReference ThighL;
	FBoneReference CalfL;
	FBoneReference FootL;
	FBoneReference ThighR;
	FBoneReference CalfR;
	FBoneReference FootR;
	FBoneReference UpperArmL;
	FBoneReference LowerArmL;
	FBoneReference HandL;
	FBoneReference UpperArmR;
	FBoneReference LowerArmR;
	FBoneReference HandR;
	bool bBones = false;
};
