// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"

/** The numbers two guns are held by, one in each hand (the Dual Burratas). */
namespace CheeseStrikeDual
{
	/** How far out (cm) each hand is moved from where the stance has the weapon hand, across the body: the guns apart. */
	constexpr float Spread = 9.0f;
	/** The same in its own player's view, where the guns are seen up close. */
	constexpr float ViewSpread = 7.0f;

	/**
	 * `Held` (in the body's space) mirrored across the plane through `Origin`
	 * whose normal is `Across`: the left hand's place and turn for the right's.
	 * A mirror on both sides of a turn is a turn again, so a hand's frame stays
	 * a hand's frame; `Correction` then turns the mirrored right hand's frame
	 * into the left hand's own (their bones' axes need not be mirrors).
	 */
	FTransform Mirrored(const FTransform& Held, const FVector& Origin, const FVector& Across);
}

/**
 * Two guns, one in each hand, on either body: the hand holding the weapon
 * moved out across the body (CheeseStrikeDual::Spread) from where the stance
 * has it, and the other hand put where it would be held the mirror way
 * round, across the body's middle, so that the second gun (which it holds
 * as the right holds the first, ACheeseStrikeWeapon::MirroredGrip) points
 * the same way, the two side by side. After the first-person hold, so that
 * it moves the hand placed in view; the left hand's grip (FCheeseStrikeGripIK)
 * has nothing to do then, the weapon being held in one hand each.
 */
struct FCheeseStrikeDualHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much it is held so (0 to 1). */
	float Alpha = 0.0f;
	/** How far out each hand goes (cm). */
	float Spread = CheeseStrikeDual::Spread;
	/** The way the body faces in component space, and where the capsule's axis meets the floor. */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Origin = FVector::ZeroVector;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	FBoneReference UpperR;
	FBoneReference LowerR;
	FBoneReference HandR;
	FBoneReference UpperL;
	FBoneReference LowerL;
	FBoneReference HandL;
	bool bBones = false;
	/** The left hand's frame against the right's mirrored, from the reference pose. */
	FTransform Correction = FTransform::Identity;
};
