// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A player down in Execution's last stand lies on their back, propped on the
 * left elbow, head and shoulders up, the pistol held out past their feet. The
 * rules of it, plain arithmetic tested without a world
 * (CheeseStrike.Rules.Downed*): how far they can aim (only a cone toward the
 * feet), how the body turns round after the view (slowly), how they scoot on
 * their back (slowly, along the body), and where the body lies, which the pose
 * (FCheeseStrikeDownedPose), the shots (FHitHistory) and the bots follow.
 *
 * Lengths are centimetres, angles degrees, times seconds. The body's frame is
 * the capsule's axis at the floor, X ahead toward the feet, Y to the right, Z up.
 */
namespace CheeseStrikeDowned
{
	/** Going down (falling back onto the elbow) and getting up again take this long, eased at both ends. */
	constexpr float Seconds = 0.8f;
	constexpr float RiseSeconds = 1.0f;
	/** Breaths a minute, laboured. */
	constexpr float BreathsPerMinute = 26.0f;
	/** A laboured breath at `Phase` (radians): -1 out to 1 in, drawn in quickly and let out slowly. */
	float Breath(float Phase);

	/** The aim: up to ConeYaw either side of the way the body faces, and between LowestPitch and HighestPitch. */
	constexpr float ConeYaw = 45.0f;
	constexpr float LowestPitch = -12.0f;
	constexpr float HighestPitch = 30.0f;
	/** `View` kept to the cone of a body facing `BodyYaw` (its roll left out). */
	FRotator ClampView(const FRotator& View, float BodyYaw);
	/** Whether `View` is within that cone, give or take `Give`. */
	bool InCone(const FRotator& View, float BodyYaw, float Give = 0.5f);

	/**
	 * The body turns round after the view: not while the view is within
	 * TurnFree of its facing, and otherwise toward it at TurnRate, until it is
	 * within TurnFree again; so pushing the view to the cone's edge scoots the
	 * body round, slowly. Getting up, it turns straight to the view at RiseTurnRate.
	 */
	constexpr float TurnFree = 15.0f;
	constexpr float TurnRate = 45.0f;
	constexpr float RiseTurnRate = 180.0f;
	/** The body's yaw `DeltaSeconds` on, facing `BodyYaw` with the view at `ViewYaw`. */
	float TurnBody(float BodyYaw, float ViewYaw, float DeltaSeconds);
	/** The same getting up: straight toward the view. */
	float TurnRising(float BodyYaw, float ViewYaw, float DeltaSeconds);

	/**
	 * Moving: a scoot on the back, pushing with the legs, toward the feet or
	 * the head only (the sideways keys turn the body instead, at TurnRate),
	 * at ScootSpeed, well under a crouched walk. Toward the feet it stops once
	 * the legs have less than LegRoomToScoot of room ahead.
	 */
	constexpr float ScootSpeed = 60.0f;
	constexpr float LegRoomToScoot = 55.0f;
	/** How far one push of a leg moves the body, for the pose's cycle. */
	constexpr float ScootStride = 25.0f;
	/** What becomes of the movement input `Input` of a body facing `Forward` (flat) with `LegRoom` of room for its legs. */
	FVector ScootInput(const FVector& Input, const FVector& Forward, float LegRoom);

	/**
	 * Where the body lies, in its frame: the hips HipsAhead in front of the
	 * capsule's axis and PelvisHeight up, the legs out ahead of them along the
	 * floor (LegLength, or bent up into the room there is), the torso back
	 * and up to the shoulders, propped on the elbow, and the head raised over
	 * the capsule's axis, about HeadAt; the eyes (where shots take the head to
	 * be, and the bots see from) are there.
	 */
	constexpr float HipsAhead = 25.0f;
	constexpr float PelvisHeight = 12.0f;
	constexpr float LegLength = 95.0f;
	/** The least the legs reach along the floor, knees right up (the feet then may touch what is in the way). */
	constexpr float LegReachLeast = 40.0f;
	/** The ankles kept this far from anything in front of the feet: a foot's length, the toes lying ahead of them, and a little clear. */
	constexpr float ToeGap = 22.0f;
	/** Where the head and the shoulders are in the body's frame. */
	FVector HeadAt();
	FVector ShouldersAt();
	/** The radius of the torso and of the legs, for shots. */
	constexpr float TorsoRadius = 18.0f;
	/** The legs are two, out from the hip joints either side of the body's line with the knees a little up, so one segment along that line is as wide as the pair. */
	constexpr float LegRadius = 19.0f;
	/** How far along the floor the legs reach from the hips with `Room` there (from the hips to whatever is ahead). */
	float LegReach(float Room);

	/** The body lying in the world: the head, the hips, the shoulders and the feet. */
	struct FShape
	{
		FVector Head = FVector::ZeroVector;
		FVector Hips = FVector::ZeroVector;
		FVector Shoulders = FVector::ZeroVector;
		FVector Feet = FVector::ZeroVector;
	};
	/** The body of a player down whose capsule's axis meets the floor at `Floor`, facing `Yaw`, with `LegRoom` ahead of its hips. */
	FShape Shape(const FVector& Floor, float Yaw, float LegRoom);
	/** Its middle (the chest, or between the chest and the hips), for aiming at it. */
	FVector Middle(const FShape& Body);

	/**
	 * The floor round a body lying down, as the character measures it (ACheeseStrikeCharacter::MeasureDownFloor): how high
	 * it is over the capsule's floor (cm, under it negative) at the points of a grid in the body's frame, FloorColumns
	 * from FloorBehind behind the capsule's axis to past the feet, FloorRows across from the left to the right, FloorStep
	 * apart; so that on a slope or a step the pose lays every part of the body on the floor under it, not on a level one
	 * (FCheeseStrikeDownedPose). Flat (all 0) until measured.
	 */
	constexpr int32 FloorColumns = 10;
	constexpr int32 FloorRows = 5;
	constexpr float FloorStep = 25.0f;
	constexpr float FloorBehind = -75.0f;
	/** A floor point measured this far up or down from the capsule's floor at most (cm): what is further is no floor of the body's. */
	constexpr float FloorReach = 60.0f;
	/** Nothing of the body lying down goes nearer the floor under it than this (cm): the pose raises it whole if anything would. */
	constexpr float FloorClear = 1.0f;
	/** The bones of the heroes' own guns (hidden with their sections, or parked far from the body by Paragon's animators, as tools/art/ragdolls.py knows them), which with what hangs from them are no part of the body lying on the floor. */
	inline const TCHAR* const OwnGunBones[] = {TEXT("gun"), TEXT("shotgun_base"), TEXT("sniper_can_arm_01"), TEXT("trap_handle"), TEXT("weapon"), TEXT("weapon_l"), TEXT("pistol"), TEXT("ult_weapon_l"), TEXT("ult_weapon_r"), TEXT("grenade")};
	struct FFloor
	{
		float Heights[FloorColumns][FloorRows] = {};
		/** The point of the grid at `Column`, `Row`, in the body's frame (X ahead, Y right). */
		static FVector2D Point(int32 Column, int32 Row);
		/** How high the floor is at `X` ahead and `Y` to the right of the capsule's axis: between the four points of the grid round it, bilinearly (the edge's beyond it). */
		float At(float X, float Y) const;
		bool IsFlat() const;
	};
}
