// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"

/** The numbers the pose of a player down (Execution's last stand) is made by. */
namespace CheeseStrikeDowned
{
	/** How long going down and getting up again take, in seconds (eased at both ends). */
	constexpr float Seconds = 0.7f;
	/** How fast the legs go from kneeling to creeping with the crouched walk and back, per second (eased). */
	constexpr float KneelRate = 4.0f;
	/** Breaths a minute, laboured. */
	constexpr float BreathsPerMinute = 26.0f;
	/** How far forward the down thigh leans from upright, in degrees: the hips sunk back towards the heel of the knee on the ground, lower than a crouch. */
	constexpr float ThighLean = 65.0f;
	/** How far forward each of the three spine bones bends, in degrees, and how far the neck and head come back up to look ahead. */
	constexpr float SpineBend[3] = {12.0f, 12.0f, 6.0f};
	constexpr float NeckBack = 14.0f;
	constexpr float HeadBack = 10.0f;
	/** How far the body leans to the side of the knee that is down, in degrees. */
	constexpr float SideLean = 5.0f;
	/** How much a breath bends the chest (degrees) and lifts the hips (cm). */
	constexpr float BreathBend = 1.5f;
	constexpr float BreathLift = 0.5f;

	/** A laboured breath at `Phase` (radians): -1 out to 1 in, drawn in quickly and let out slowly. */
	float Breath(float Phase);

	/** The bones the pose moves (the UE4 mannequin's names), which every body must have (FCheeseStrikeDownedPose::CacheBones_AnyThread finds them by these names). */
	inline const TCHAR* const PoseBones[] = {TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("neck_01"), TEXT("head"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), TEXT("upperarm_r")};
}

/**
 * The pose of a player down: on one knee (the right, the shooting hand's),
 * the other foot planted ahead, hunched forward over the hurt, the left hand
 * pressed to the belly and the sidearm held out in the right, breathing
 * hard. Made in component space from whatever pose comes in (the crouched
 * pistol set), by bone names every hero shares (the UE4 mannequin's), so it
 * fits every body: the hips go down until the right knee touches the ground,
 * two-bone IK puts the legs where kneeling has them (or, moving, keeps the
 * crouched walk's feet on the ground), the spine bends forward and the neck
 * back up, the right arm comes up again by what the spine took down so that
 * the pistol still points where the player aims, and the left arm reaches
 * for the belly. The whole is blended in by Alpha, bone by bone in local
 * space, so that going down and getting up are a movement, not a snap.
 */
struct FCheeseStrikeDownedPose : public FAnimNode_Base
{
	FComponentSpacePoseLink ComponentPose;

	/** How far down, 0 (not at all) to 1. */
	float Alpha = 0.0f;
	/** How much the legs kneel, against following the crouched walk's feet (1 still, 0 moving). */
	float Kneel = 1.0f;
	/** The breath now, -1 to 1 (CheeseStrikeDowned::Breath). */
	float Breath = 0.0f;
	/** How much the left hand is on the belly rather than where the pose has it (a montage takes it, as the grip lets it go). */
	float HandOnWound = 1.0f;
	/** The way the body faces, in component space (Paragon's and the mannequin's +Y). */
	FVector Forward = FVector(0.0, 1.0, 0.0);

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
	FBoneReference BallR;
	FBoneReference UpperArmL;
	FBoneReference LowerArmL;
	FBoneReference HandL;
	FBoneReference UpperArmR;
	bool bBones = false;
	bool bToldMissing = false;
};
