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
	/** The toes kept this far from anything in front of the feet. */
	constexpr float ToeGap = 10.0f;
	/** Where the head and the shoulders are in the body's frame. */
	FVector HeadAt();
	FVector ShouldersAt();
	/** The radius of the torso and of the legs, for shots. */
	constexpr float TorsoRadius = 18.0f;
	constexpr float LegRadius = 11.0f;
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
}
