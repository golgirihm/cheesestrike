// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"

/**
 * The numbers the arms and the body are posed by in code, over the
 * animations: the first-person hold (FCheeseStrikeViewHold), the knife's
 * stance and swipe (FCheeseStrikeKnifeHold) and the slide
 * (FCheeseStrikeSlidePose). Plain functions, tested without a world.
 */
namespace CheeseStrikeArms
{
	/**
	 * How fast the first-person hold follows the animation's hand, per second:
	 * the steady follow takes out the animations' own tremble (their
	 * compression's error, a tenth of a degree or two at each joint, which the
	 * arm adds up and a gun's length shows), the settling one the lasting lean
	 * of crouching and the cycles, which the hold undoes, so that only their
	 * bob shows.
	 */
	constexpr float SteadyRate = 15.0f;
	constexpr float SettleRate = 4.0f;
	/** The most of the cycles' bob the hold keeps (cm). */
	constexpr float MostBob = 8.0f;
	/** Aimed, how much of the bob and sway is kept. */
	constexpr float AimedSway = 0.35f;
	/** A weapon just drawn comes up from this far below (cm), tipped down this much (degrees). */
	constexpr float DrawDrop = 25.0f;
	constexpr float DrawTip = 30.0f;
	/** Sprinting, the weapon is carried across the body, its muzzle turned in and up and its top canted in (a turn about the hand: pitch, yaw, roll in degrees; and a shift in cm: forward, right, up). */
	inline const FRotator SprintTurn(8.0f, -10.0f, -22.0f);
	inline const FVector SprintShift(-1.0f, 0.0f, -2.0f);
	/** Sliding, canted in and a little low. */
	inline const FRotator SlideTurn(3.0f, -6.0f, -20.0f);
	inline const FVector SlideShift(-1.0f, -1.0f, -2.0f);
	/** How fast the sprint and slide holds come and go, per second. */
	constexpr float HoldBlendRate = 9.0f;
	/** An arm reaching for a place in view may stretch this much rather than fall short. */
	constexpr float MostStretch = 1.08f;

	/** The share of the way a first-order follow at `Rate` per second goes in `DeltaSeconds`, whatever the frame rate. */
	float Follow(float Rate, float DeltaSeconds);

	/** The knife's swipe: seconds it takes from the stroke to the knife back on guard. */
	constexpr float SwipeSeconds = 0.45f;
	/** Where the knife hand is at `Phase` (0 to 1) of a swipe, from where it is on guard (cm: forward, right, up), and which way the blade points and its spine faces (unit vectors in the same axes), for a frame scaled by `Size` (1 for the body, smaller for the view). */
	struct FKnifeAt
	{
		FVector Hand = FVector::ZeroVector;
		FVector Blade = FVector::ForwardVector;
		FVector Spine = FVector::UpVector;
	};
	/** On guard: the blade forward, up and a little in, its spine up. */
	FKnifeAt KnifeGuard();
	/** At `Phase` of a swipe from the right shoulder down and across to the left, the edge leading: raised, swept, back on guard. */
	FKnifeAt KnifeSwipe(float Phase, float Size = 1.0f);

	/** Whether someone crouched and going at `Speed` (cm/s) is sliding, as any machine can tell: faster than a crouched walk can go. */
	bool LooksSliding(bool bCrouched, float Speed);
	/** How far down and back the slide lays the body, 0 to 1, from how fast it goes: all the way at a sprint, nothing at a crouched walk. */
	float SlideDepth(float Speed);
	/** The view's roll while sliding (degrees), as far into the slide as `Depth`. */
	constexpr float SlideTilt = -6.0f;
}

/**
 * Its own player's arms (the first-person body only), placed in view: the
 * hand holding the weapon is brought by two-bone IK to where it holds the
 * weapon down the sights, its sight line running down the view through the
 * eye, or at the hip, that moved and turned in view as its hold has it
 * (CheeseStrikeWeaponLooks::HipFromAimed), so that every weapon sits alike
 * whatever its shape; with the animations' bob but without their lasting
 * lean or tremble; low, coming up, while it is drawn; low and turned in at a
 * sprint; a
 * little low and canted in a slide; and the knife on guard and swiped
 * across the view (CheeseStrikeArms::KnifeSwipe). The other hand follows on
 * its grip (FCheeseStrikeGripIK, after it), and while a montage has it (a
 * reload), goes along with the weapon hand where the animation has it
 * relative to that hand. Only the arms move: the body and
 * the head (which the camera rides) stay where the animation has them, so
 * that nothing of the body is pushed into the view. A montage in the slot
 * (firing, reloading, drawing) moves the hand through it.
 */
struct FCheeseStrikeViewHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much the arm is placed (0 where the animation has it). */
	float Alpha = 0.0f;
	/** The view's turn in component space, and the camera's place in the head bone's space. */
	FQuat View = FQuat::Identity;
	FVector CameraAtHead = FVector::ZeroVector;
	/** The weapon's model in the hand bone's space (with its size), which way its business end points in its own space, and where the eye goes to aim down its sights, in its own space. */
	FTransform ModelInHand = FTransform::Identity;
	FVector ModelAlong = FVector(0.0, 1.0, 0.0);
	FVector Sight = FVector::ZeroVector;
	bool bHasSight = false;
	/** At the hip, the hand as it is aimed, moved and turned in view (cm: forward, right, up; about the eye); and the knife's hand on guard. */
	FTransform HipFromAimed = FTransform::Identity;
	FVector KnifeHand = FVector::ZeroVector;
	/** How far down the sights (0 to 1), how far still to come up as it is drawn (0 to 1), how much sprinting and sliding. */
	float Aimed = 0.0f;
	float Lowered = 0.0f;
	float Sprint = 0.0f;
	float Slide = 0.0f;
	/** Held as a knife (on guard, swiped), and the swipe's phase (none below 0). */
	bool bKnife = false;
	float SwipePhase = -1.0f;
	/** How much a montage moves the arms (1 - the slot's source weight). */
	float Montage = 0.0f;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	FBoneReference Head;
	FBoneReference UpperArm;
	FBoneReference LowerArm;
	FBoneReference Hand;
	/** The other arm, which goes along with the weapon hand where the animation has it on the weapon (a reload's). */
	FBoneReference OtherUpperArm;
	FBoneReference OtherLowerArm;
	FBoneReference OtherHand;
	bool bBones = false;
	float DeltaSeconds = 0.0f;
	/** The animation's hand in view, followed steadily and settling (FCheeseStrikeViewHold's rates). */
	FVector SteadyAt = FVector::ZeroVector;
	FQuat SteadyTurn = FQuat::Identity;
	FVector SettledAt = FVector::ZeroVector;
	FQuat SettledTurn = FQuat::Identity;
	bool bFollowing = false;
};

/**
 * The knife in hand, on either body: the fist closed round its handle (the
 * index finger curled as the middle one is, where the pistol's stance has
 * it along the frame); for the body others see, the knife hand drawn in on
 * guard ahead of the chest, and its swipe (CheeseStrikeArms::KnifeSwipe), in
 * the frame of the way the body faces and aims. (The first-person body's
 * hand is placed in view by FCheeseStrikeViewHold, after it.)
 */
struct FCheeseStrikeKnifeHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much it is held as a knife (0 to 1). */
	float Alpha = 0.0f;
	/** Whether to place the hand (not for the first-person body). */
	bool bPlaceHand = true;
	/** The way the body faces in component space, and its aim's pitch (degrees). */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	float AimPitch = 0.0f;
	/** The knife's model in the hand bone's space, which way its blade points and its spine faces in its own space. */
	FTransform ModelInHand = FTransform::Identity;
	FVector ModelAlong = FVector(0.0, -1.0, 0.0);
	FVector ModelSpine = FVector(0.0, 0.0, 1.0);
	/** The swipe's phase (none below 0). */
	float SwipePhase = -1.0f;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	FBoneReference Chest;
	FBoneReference UpperArm;
	FBoneReference LowerArm;
	FBoneReference Hand;
	FBoneReference Index[3];
	FBoneReference Middle[3];
	bool bBones = false;
	bool bFingers = false;
};

/**
 * A slide (crouching at a sprint), on either body: the hips dropped near the
 * floor and laid back, the chest coming up over them, the right leg out
 * ahead along the floor and the left folded under it, knee up; blended in
 * by Alpha (CheeseStrikeArms::SlideDepth), bone by bone, over whatever comes
 * in (the crouched cycles). The arms keep the weapon: the hands are placed
 * after it.
 */
struct FCheeseStrikeSlidePose : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	float Alpha = 0.0f;
	/** The way the body faces in component space, and where the capsule's axis meets the floor there. */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	FVector Origin = FVector::ZeroVector;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	void Slide(FCSPose<FCompactPose>& Pose) const;
	FBoneReference Pelvis;
	FBoneReference Spine[3];
	FBoneReference ThighL;
	FBoneReference CalfL;
	FBoneReference FootL;
	FBoneReference ThighR;
	FBoneReference CalfR;
	FBoneReference FootR;
	bool bBones = false;
};
