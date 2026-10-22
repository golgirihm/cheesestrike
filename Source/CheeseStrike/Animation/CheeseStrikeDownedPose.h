// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "Rules/CheeseStrikeDowned.h"

/** The numbers the pose of a player down (Execution's last stand) is made by; the rest are CheeseStrikeDowned's rules. */
namespace CheeseStrikeDowned
{
	/** How far back the hips lie from upright, in degrees, before the torso is propped up on the elbow (which sets it in the end). */
	constexpr float PelvisRecline = 70.0f;
	/** How far forward each of the three spine bones curls, lifting the shoulders off the floor. */
	constexpr float SpineCurl[3] = {4.0f, 8.0f, 14.0f};
	/** How high the ankles of legs out along the floor are. */
	constexpr float AnkleHeight = 9.0f;
	/** How much of the way out the legs reach lying still: the right one out straight, the left bent, its knee up. */
	constexpr float RightLegOut = 0.985f;
	constexpr float LeftLegOut = 0.95f;
	/** Scooting, each leg pushes between these shares of its reach, a foot lifted this much as it comes back for the next push. */
	constexpr float PushFrom = 0.7f;
	constexpr float PushTo = 0.97f;
	constexpr float FootLift = 5.0f;
	/** How much the hips shuffle from side to side and lift with each push (cm), and turn (degrees). */
	constexpr float HipShuffle = 2.0f;
	constexpr float HipLift = 1.5f;
	constexpr float HipTwist = 5.0f;
	/** Turning round on the spot, the legs push this much of a full scoot. */
	constexpr float TurnPush = 0.5f;
	/** How fast the scoot comes in and out of the legs (per second). */
	constexpr float ScootBlendRate = 5.0f;
	/** How high the left elbow's joint is, the forearm flat on the floor: the shoulder is an upper arm over it. */
	constexpr float ElbowHeight = 12.0f;
	/** How much of the aim to the side the chest turns with (the rest is the head's and the arm's). */
	constexpr float ChestTurn = 0.8f;
	/** How much of the head's turn to look along the aim is taken in the neck. */
	constexpr float NeckShare = 0.7f;
	/** The pistol hand: so much of the arm's length out along the aim from the shoulder, and so far up and in from that (cm). */
	constexpr float PistolReach = 0.86f;
	constexpr float PistolUp = 5.0f;
	constexpr float PistolIn = 4.0f;
	/** In its own player's view (the first-person body), the pistol is held where it shows from the eyes, whichever way they look: so far out along the aim from the head, to the right of it and under it (cm). */
	constexpr float FirstPersonPistolOut = 44.0f;
	constexpr float FirstPersonPistolRight = 13.0f;
	constexpr float FirstPersonPistolUnder = 13.0f;
	/** How much a breath bends the chest (degrees). */
	constexpr float BreathBend = 2.0f;

	/** The bones the pose moves (the UE4 mannequin's names), which every body must have (FCheeseStrikeDownedPose::CacheBones_AnyThread finds them by these names). */
	inline const TCHAR* const PoseBones[] = {TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")};
}

/**
 * The pose of a player down: on their back, the hips on the floor, the legs
 * out along it ahead (the right straight, the left knee a little up), the
 * spine curled up so that the shoulders and the head are raised, propped on
 * the left elbow, and the pistol held out in the right hand past the feet,
 * where the player aims, breathing hard. Made in component space from
 * whatever pose comes in (the crouched pistol set), by bone names every hero
 * shares (the UE4 mannequin's), so it fits every body: the pelvis laid back
 * on the floor where CheeseStrikeDowned's rules have the hips, the spine
 * curled until the left shoulder is an upper arm over the floor, the head
 * turned to look along the aim, two-bone IK for the arms (the left elbow on
 * the floor, the right hand out along the aim, the pistol turned with it) and
 * for the legs (out along the floor as far as there is room, scooting: each
 * pushing in turn, the hips shuffling). The whole is blended in by Alpha,
 * bone by bone in local space, so that going down and getting up are a
 * movement, not a snap.
 */
struct FCheeseStrikeDownedPose : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How far down, 0 (not at all) to 1. */
	float Alpha = 0.0f;
	/** The breath now, -1 to 1 (CheeseStrikeDowned::Breath). */
	float Breath = 0.0f;
	/** How much the arms are the pose's rather than what comes in (a montage, the reload, takes them). */
	float ArmsOwn = 1.0f;
	/** The way the body faces, in component space (Paragon's and the mannequin's +Y). */
	FVector Forward = FVector(0.0, 1.0, 0.0);
	/** Where the capsule's axis meets the floor, in component space. */
	FVector Origin = FVector::ZeroVector;
	/** Where the player aims, from the way the body faces: degrees to the right and up. */
	float AimYaw = 0.0f;
	float AimPitch = 0.0f;
	/** How far the legs reach along the floor from the hips at most (CheeseStrikeDowned::LegReach). */
	float LegReach = CheeseStrikeDowned::LegLength;
	/** Scooting: how much (0 still, 1 pushing), and where the legs are in their pushes (radians). */
	float Scoot = 0.0f;
	float ScootPhase = 0.0f;
	/** Whether this is the body its own player sees from inside. */
	bool bFirstPerson = false;

	/** Whether it has every bone it needs (logged once if not). */
	bool HasBones() const { return bBones; }

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) override;

private:
	/** Puts the pose all the way down (Alpha aside) into `Pose`. */
	void GoDown(FCSPose<FCompactPose>& Pose) const;

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
	bool bToldMissing = false;
};
