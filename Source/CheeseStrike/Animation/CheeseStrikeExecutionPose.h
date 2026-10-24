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
 * foot planted ahead, leaning over; the left hand pins the near shoulder
 * while the right draws the knife back, its point on the chest, holds a
 * beat, and thrusts it in once; the left hand comes onto the fist to push
 * it home, both leaning on it a moment; then the left hand braces on the
 * chest and the right pulls the knife out, and they stand. The knife's
 * point is placed, not the hand: the hand is put where the knife it holds
 * (ModelInHand, ModelAlong, as FCheeseStrikeKnifeHold has it) has its
 * point where it should be and its blade along the thrust. Their stomp:
 * stood at the head, the weight onto the left foot, the right knee comes
 * up high, the boot hangs over the face a moment, and slams down on it,
 * the weight following through, grinds, and steps back.
 *
 * The one down: at the thrust the chest jerks and the head is thrown back,
 * the pistol hand grabbing at the executor's wrist, and at the pull the
 * body jolts and goes limp, the arm falling to the floor; under the boot,
 * the pistol arm comes up to shield the face, too late, and the body is
 * driven flat.
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
	/** The executor's knife, once it is in hand: its model as hand_r holds it, and which way its point is in the model. */
	bool bKnifeInHand = false;
	FTransform ModelInHand = FTransform::Identity;
	FVector ModelAlong = FVector(0.0, 1.0, 0.0);
	/** How far the knife's point is from its model's origin, the middle of the grip (cm, tools/art/weapon_models.py). */
	static constexpr float KnifePoint = 21.0f;

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
