// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "ReferenceSkeleton.h"

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
	/**
	 * Wave 17: a weapon drawn in its own player's view without its animation (FWeaponLook::bDrawAnimation, which only the
	 * one-handed pistol and the two guns keep: Lyra's equip swung the arms about) comes up from below in code alone, over
	 * this long (or its draw time, if shorter), easing in and out.
	 */
	constexpr float RaiseSeconds = 0.35f;
	/** How far still to come up (1 to 0: DrawDrop and DrawTip for 1) `SinceDrawn` seconds after a weapon whose draw takes `DrawTime` was drawn. */
	float RaiseLowered(float SinceDrawn, float DrawTime);
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

	/**
	 * A hand's fist, in its hand bone's space (the wrist at the origin): where the middle of a handle it closes round lies
	 * (across the palm, under the fingers' roots, the fingers curled round it), which way the handle runs on out of the
	 * fist on the thumb's side (from the little finger's side to the index's, leaning toward the fingers' tips, as a handle
	 * lies across the palm in a hammer grip), and which way the fingers point from the wrist. Found from the hand's own
	 * bones (FistOfSkeleton), so that a knife or a stick sits in any hero's fist as it closes (FWeaponLook::bInFist).
	 */
	struct FFist
	{
		FVector Centre = FVector::ZeroVector;
		FVector Handle = FVector::RightVector;
		FVector Fingers = FVector::ForwardVector;
	};
	/** How far the handle leans from straight across the palm toward the fingers' tips (degrees). */
	constexpr float FistSlant = 20.0f;
	/** Where the handle's middle lies from the middle of the fingers' roots: toward the palm, and back toward the wrist (shares of the hand's length from the wrist to them). */
	constexpr float FistInset = 0.3f;
	constexpr float FistBack = 0.12f;
	/** How far each joint of a finger (from its root) bends to close the fist round a handle (degrees). */
	constexpr float FistBends[3] = {60.0f, 80.0f, 45.0f};
	/** The fist of a hand whose fingers' roots (index, middle, ring, little: the _01 bones) are at `Knuckles` in its own space; `bRight` for a right hand. */
	FFist FistOf(const FVector (&Knuckles)[4], bool bRight);
	/** The fist of `Skeleton`'s right hand (or left) in its reference pose; false if it hasn't the bones. */
	bool FistOfSkeleton(const FReferenceSkeleton& Skeleton, bool bRight, FFist& Out);
	/**
	 * Where a thing held in the fist sits in the hand bone's space: its own `Along` (a blade's tip, a stick's head) along the
	 * handle out of the thumb's side, its own `Up` (a knife's spine) facing back from the fingers (the edge the way they
	 * point), its own origin (the middle of its grip) in the fist, scaled `Scale`; or held `GripAlong` further along it
	 * (cm in its own space, toward its business end).
	 */
	FTransform HeldInFist(const FFist& Fist, const FVector& Along, const FVector& Up, const FVector& Scale, float GripAlong = 0.0f);
	/**
	 * The turn of a hand whose fist holds a handle running `Handle` out of its thumb's side, its fingers pointing `Away` as
	 * far as the handle lets them (the forearm's way on, so that the wrist is straight), in whatever frame those are in.
	 */
	FQuat FistTurn(const FFist& Fist, const FVector& Handle, const FVector& Away);
	/** Holding a knife or a stick, the forearm runs to the hand from a point this far under the shoulder (cm): the elbow hangs. */
	constexpr float FistElbowDrop = 25.0f;

	/** The knife's swipe: seconds it takes from the stroke to the knife back on guard. */
	constexpr float SwipeSeconds = 0.45f;
	/** On guard the knife's blade stands up out of the fist this many degrees from level, turned this many in toward the middle. */
	constexpr float GuardPitch = 55.0f;
	constexpr float GuardYaw = -10.0f;
	/** In its own player's view the knife's blade is tipped up this many degrees more than on the body. */
	constexpr float ViewKnifeTip = 5.0f;
	/** In its own player's view the knife is turned this many degrees about its blade (wave 17), so that its flat and its holes face the eye rather than its edge. */
	constexpr float ViewKnifeTurn = 35.0f;
	/** Where the knife hand is at `Phase` (0 to 1) of a swipe, from where it is on guard (cm: forward, right, up), and which way the blade points and its spine faces (unit vectors in the same axes), for a frame scaled by `Size` (1 for the body, smaller for the view). */
	struct FKnifeAt
	{
		FVector Hand = FVector::ZeroVector;
		FVector Blade = FVector::ForwardVector;
		FVector Spine = FVector::UpVector;
	};
	/** On guard: the blade standing up out of the top of the fist, leaning forward and a little in (GuardPitch, GuardYaw: a hammer grip), its spine back; tipped up `PitchUp` degrees more. */
	FKnifeAt KnifeGuard(float PitchUp = 0.0f);
	/** At `Phase` of a swipe from the right shoulder down and across to the left, the edge leading: raised, swept, back on guard (tipped up `PitchUp` more, as KnifeGuard). */
	FKnifeAt KnifeSwipe(float Phase, float Size = 1.0f, float PitchUp = 0.0f);
	/**
	 * The phase of a swipe at which the blade (the Cheese Stick's head) comes round to straight ahead, sweeping across from
	 * the right: where a blow lands on someone before the swinger (CheeseStrikeWeapons::ContactDelay). The swipe's keys,
	 * and the sledgehammer swing's (SledgeSwing), have it pointing straight ahead there.
	 */
	constexpr float SwipeContactPhase = 0.38f;

	/**
	 * The Cheese Stick, held as a sledgehammer (FWeaponLook::bSledge): both hands on its handle, the right near its end and
	 * the left higher up, its head over the right shoulder at rest; swung from there, wound back over the shoulder, down
	 * and across to straight ahead (at SwipeContactPhase), on round to the lower left and back. Where the right hand is
	 * from where it rests (cm: ahead, right, up) and which way the stick runs from it to its head.
	 */
	struct FSledgeAt
	{
		FVector Hand = FVector::ZeroVector;
		FVector Along = FVector::UpVector;
	};
	FSledgeAt SledgeRest();
	FSledgeAt SledgeSwing(float Phase, float Size = 1.0f);
	/** Where the right hand rests holding it, in its own player's view (cm from the eye: ahead, right, up), so that the stick takes the lower right of the view; and on the body others see, from the chest. */
	inline const FVector SledgeViewHand(30.0f, 15.0f, -21.0f);
	inline const FVector SledgeBodyHand(22.0f, 6.0f, -12.0f);
	/** A heavy shot's kick in view: the weapon in hand tipped up this many degrees for a whole kick (FWeaponStats::KickPitch moves the view; this the weapon in it). */
	constexpr float ViewKickTip = 9.0f;

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
	/** How wide the swipe is (1 the knife's; the Cheese Stick's wider), and whether it is held and swung as a sledgehammer (the Cheese Stick). */
	float SwipeSize = 1.0f;
	bool bSledge = false;
	/** How much a montage moves the arms (1 - the slot's source weight). */
	float Montage = 0.0f;
	/** A heavy shot's kick (0 to 1): the weapon thrown back KickBack cm toward the eye and tipped up (CheeseStrikeArms::ViewKickTip for a whole one). */
	float Kick = 0.0f;
	float KickBack = 0.0f;

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
	/** The right hand's fist (CheeseStrikeArms::FistOfSkeleton), for a knife or a stick held in it. */
	CheeseStrikeArms::FFist Fist;
	float DeltaSeconds = 0.0f;
	/** The animation's hand in view, followed steadily and settling (FCheeseStrikeViewHold's rates). */
	FVector SteadyAt = FVector::ZeroVector;
	FQuat SteadyTurn = FQuat::Identity;
	FVector SettledAt = FVector::ZeroVector;
	FQuat SettledTurn = FQuat::Identity;
	bool bFollowing = false;
};

/**
 * The knife (or the Cheese Stick) in hand, on either body: the fist closed
 * round its handle (every finger curled, CheeseStrikeArms::FistBends, the
 * thumb over them), which sits in the fist's grip as the hand's own bones
 * have it (CheeseStrikeArms::FFist, FWeaponLook::bInFist); with the stick,
 * the left fist too. For the body others see, the knife hand on guard low on
 * the right ahead of the hip, the blade standing up out of the top of the
 * fist, the wrist straight (the fingers pointing on from the forearm,
 * CheeseStrikeArms::FistTurn), and its swipe (CheeseStrikeArms::KnifeSwipe);
 * or the stick held as a sledgehammer, its head over the right shoulder, and
 * swung (SledgeRest, SledgeSwing); in the frame of the way the body faces and
 * aims. (The first-person body's hand is placed in view by
 * FCheeseStrikeViewHold, after it.)
 */
struct FCheeseStrikeKnifeHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much it is held as a knife (0 to 1). */
	float Alpha = 0.0f;
	/** Whether to place the hand (not for the first-person body). */
	bool bPlaceHand = true;
	/** Only the right fist closed round a handle, the hand left where the hold has it (FWeaponLook::bFistGrip: the BabyBomb). */
	bool bFingersOnly = false;
	/** The way the body faces in component space, and its aim's pitch (degrees). */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	float AimPitch = 0.0f;
	/** The swipe's phase (none below 0), how wide it is (1 the knife's), and whether it is the Cheese Stick held as a sledgehammer in both hands. */
	float SwipePhase = -1.0f;
	float SwipeSize = 1.0f;
	bool bSledge = false;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	FBoneReference Chest;
	FBoneReference UpperArm;
	FBoneReference LowerArm;
	FBoneReference Hand;
	FBoneReference OtherHand;
	/** Each hand's (right, left) fingers' three joints, index to little, and its thumb's. */
	FBoneReference Fingers[2][4][3];
	FBoneReference Thumb[2][3];
	bool bBones = false;
	bool bFingers[2] = {false, false};
	CheeseStrikeArms::FFist Fist;
};

/**
 * Closes `Hand`'s fingers (index to little, three joints each) into a fist round a handle, the thumb over them
 * (CheeseStrikeArms::FistBends), whatever the pose had them doing; `bRight` for a right hand. On a gun's grip
 * (bTrigger), the index finger only part closed, on the trigger, and the thumb left as it was.
 */
void CheeseStrikeCloseFist(FCSPose<FCompactPose>& Pose, const FBoneReference& Hand, const FBoneReference (&Fingers)[4][3], const FBoneReference (&Thumb)[3], bool bRight, bool bTrigger = false);

/**
 * A weapon held in the right hand alone (the Curd Pistol), on either body:
 * the left arm let go of it and lowered by the side, the hand turned on the
 * forearm as the skeleton's own pose has it (as a grenade's throw lowers it),
 * blended in by Alpha, bone by bone.
 */
struct FCheeseStrikeFreeArm : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	float Alpha = 0.0f;
	/** The way the body faces in component space. */
	FVector Forward = FVector(0.0, 1.0, 0.0);

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	FBoneReference UpperArm;
	FBoneReference LowerArm;
	FBoneReference Hand;
	bool bBones = false;
};

/** Lowers the arm (`Upper`, `Lower`, `End`) by the side of a body facing `Ahead` (flat), its hand a little ahead and out toward `Out` (the arm's own side), the elbow back, turned on the forearm as the skeleton's own pose has it. */
void CheeseStrikeLowerArm(FCSPose<FCompactPose>& Pose, FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex End, const FVector& Ahead, const FVector& Out);

/**
 * A grenade in hand, on either body (CheeseStrikeGrenades::HeldHand,
 * ThrowHand): the right hand up with the grenade in its palm, placed from
 * the eyes in the frame of the aim (its own player's view, or the way the
 * body faces tipped by its aim), its fingers and palm turned as the throw has
 * them at its phase, the elbow out and down, its fingers closed round the
 * grenade; and the left arm let go of the
 * weapon and lowered by the side. Blended in by Alpha (the switch to the
 * grenade, or a throw under way), bone by bone, over the hold of the weapon,
 * which is put away meanwhile.
 */
struct FCheeseStrikeGrenadeHold : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How much the arms hold the grenade (0 to 1). */
	float Alpha = 0.0f;
	/** The throw's phase (none below 0: held). */
	float ThrowPhase = -1.0f;
	/** Its own player's body: placed in the view's frame (View, the camera at CameraAtHead in the head bone's space); else in the body's. */
	bool bInView = false;
	FQuat View = FQuat::Identity;
	FVector CameraAtHead = FVector::ZeroVector;
	/** The way the body faces in component space, and its aim's pitch (degrees). */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	float AimPitch = 0.0f;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	void PoseArms(FCSPose<FCompactPose>& Pose) const;
	FBoneReference Head;
	FBoneReference UpperArm;
	FBoneReference LowerArm;
	FBoneReference Hand;
	FBoneReference Index;
	FBoneReference Middle;
	FBoneReference Pinky;
	/** Each finger's three joints, index to little, closed round the grenade. */
	FBoneReference Curl[4][3];
	bool bCurl = false;
	FBoneReference OtherUpperArm;
	FBoneReference OtherLowerArm;
	FBoneReference OtherHand;
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
