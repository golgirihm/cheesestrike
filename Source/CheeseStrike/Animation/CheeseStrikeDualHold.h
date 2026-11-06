// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"

/** The numbers two guns are held by, one in each hand (the Dual Burratas, the Munster Twin SMGs). */
namespace CheeseStrikeDual
{
	/** On the body others see, how far (cm) each hand is from the body's middle at the hip: the two about a shoulder's width apart, each gun ahead of its own shoulder. */
	constexpr float HipHalfApart = 19.0f;
	/** The elbow of each straight wrist, a forearm behind the hand along its gun: this much further out across the body and down (cm). */
	constexpr float ElbowOut = 6.0f;
	constexpr float ElbowDown = 10.0f;
	/**
	 * Aimed: on the body others see, how far out each hand goes from where the aimed stance has the weapon hand (the two
	 * side by side at eye level); and in its own player's view, half of how far apart the two guns sit either side of the
	 * eye, which the look's Sight gives (FWeaponLook::Sight, its X), so that the view's hold needs no push at all: far
	 * enough apart that neither hand comes over the middle of the view, where the crosshair is.
	 */
	constexpr float AimSpread = 6.0f;
	constexpr float AimHalfApart = 11.0f;
	/** Aimed, each gun is turned in toward the middle by this many degrees (the right to the left, the left to the right). */
	constexpr float Converge = 3.0f;

	/**
	 * `Held` (in the body's space) mirrored across the plane through `Origin`
	 * whose normal is `Across`: the left hand's place and turn for the right's.
	 * A mirror on both sides of a turn is a turn again, so a hand's frame stays
	 * a hand's frame; `Correction` then turns the mirrored right hand's frame
	 * into the left hand's own (their bones' axes need not be mirrors).
	 */
	FTransform Mirrored(const FTransform& Held, const FVector& Origin, const FVector& Across);
	/**
	 * `Hand` (in the body's space) turned about its own place so that the gun it holds, whose business end points
	 * `AlongInHand` in the hand's space, points the way `Forward` does turned `InDegrees` in toward the body's
	 * left (the right gun converged on the middle), its pitch kept as it was: the aimed stance's own yaw, which the
	 * animation may have a little out, is put right by it.
	 */
	FTransform Aligned(const FTransform& Hand, const FVector& AlongInHand, const FVector& Forward, const FVector& Up, float InDegrees);
}

/**
 * Two guns, one in each hand, on either body: the hand holding the weapon
 * moved out across the body from where the stance has it (on the body
 * others see, to CheeseStrikeDual::HipHalfApart of the middle, the two about
 * a shoulder's width apart), its gun laid along the body's forward (Aligned,
 * whatever yaw the stance gave) and its wrist straight (the elbow bent
 * toward a point a forearm behind the hand along the gun, ElbowOut and
 * ElbowDown), and the other hand put where it would be held the mirror way
 * round, across the body's middle, its elbow the mirror of the right's, so
 * that the second gun (which it holds as the right holds the first,
 * ACheeseStrikeWeapon::MirroredGrip) points the same way, the two side by
 * side and alike. Aimed (Aim), the spread is the aimed one (in its own
 * player's view none: the look's Sight has put the right gun beside the eye
 * already) and the right gun is turned in a little (Converge) before the
 * mirror, so that the two converge. In its own player's view the mirror is
 * the view's own middle (the upright plane through the eye along the view,
 * bAboutEye), so that the pair sits evenly either side of the crosshair
 * whatever the head's lean. Each hand's fingers close round its own gun's
 * grip, by its own bones (CheeseStrikeCloseFist). After the first-person
 * hold, so that it moves the hand placed in view; the left hand's grip
 * (FCheeseStrikeGripIK) has nothing to do then, the weapon being held in one
 * hand each.
 */
struct FCheeseStrikeDualHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much it is held so (0 to 1). */
	float Alpha = 0.0f;
	/** How far out the right hand goes (cm) at the hip (from the body's middle if bFromMiddle, else from where the stance has it), and aimed from where the stance has it; and how far aimed it is (0 to 1). */
	float Spread = CheeseStrikeDual::HipHalfApart;
	bool bFromMiddle = true;
	float AimSpread = CheeseStrikeDual::AimSpread;
	float Aim = 0.0f;
	/** The weapon's model in the hand bone's space, and which way its muzzle points in its own space, for laying it along the aim. */
	FTransform ModelInHand = FTransform::Identity;
	FVector ModelAlong = FVector(0.0, 1.0, 0.0);
	/** The way the body faces in component space, and where the capsule's axis meets the floor. */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Origin = FVector::ZeroVector;
	/** Mirrored across the view's middle (its own player's body): the view's turn in component space, and the camera's place in the head bone's space. */
	bool bAboutEye = false;
	FQuat View = FQuat::Identity;
	FVector CameraAtHead = FVector::ZeroVector;

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
	FBoneReference Head;
	bool bBones = false;
	/** The left hand's frame against the right's mirrored, from the reference pose. */
	FTransform Correction = FTransform::Identity;
	/** Each finger's joints (thumb, index, middle, ring, little; three each) on both hands (right, left). */
	FBoneReference Fingers[2][5][3];
	bool bFingers = false;
};
