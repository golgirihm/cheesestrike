// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Footsteps: when a body on the ground sounds a step, and how loudly. Every
 * machine works it out for every character it shows, from how fast that
 * character is going, so that nothing is sent: a player's steps are heard by
 * the others from where they are, fading with distance. A step is sounded
 * each stride of ground covered, so a sprint (faster over longer strides)
 * steps faster than a walk, and louder; a crouched walk is soft and slow;
 * nothing is heard in the air, in a slide, standing still (crouched too) or
 * dead. Plain functions, tested without a world (CheeseStrike.Rules.Footsteps*).
 */
namespace CheeseStrikeFootsteps
{
	enum class EGait : uint8
	{
		/** No step: in the air, sliding, still or dead. */
		None,
		Crouch,
		Walk,
		Sprint,
	};

	/** Slower than this across the ground (cm/s) is standing still, a crouch-still included. */
	constexpr float MinSpeed = 40.0f;
	/** The ground covered between one step and the next (cm), by gait: shorter as the walk slowed, so that the steps keep the animations' pace. */
	constexpr float CrouchStride = 88.0f;
	constexpr float WalkStride = 152.0f;
	constexpr float SprintStride = 184.0f;
	/** How loud a step is, by gait, as a share of the sound's full volume. */
	constexpr float CrouchVolume = 0.2f;
	constexpr float WalkVolume = 0.5f;
	constexpr float SprintVolume = 0.7f;
	/** A step is heard in full within this (cm) and fades to nothing FalloffDistance beyond it. */
	constexpr float FullWithin = 150.0f;
	constexpr float FalloffDistance = 1800.0f;
	/** Where in a stride (0 to 1) a body that starts moving is, so that its first step comes soon, not a whole stride later. */
	constexpr float StartProgress = 0.6f;

	/** How a body is moving: its gait from whether it is alive and on the ground, sliding, crouched or sprinting, and its speed across the ground (cm/s). */
	EGait GaitFor(bool bAlive, bool bOnGround, bool bSliding, bool bCrouched, bool bSprinting, float GroundSpeed);
	/** The ground covered between steps in a gait (cm); 0 for none. */
	float Stride(EGait Gait);
	/** The volume of a step in a gait; 0 for none. */
	float Volume(EGait Gait);
	/**
	 * Moves `Progress` (the share of a stride walked since the last step) on by `Distance` cm in `Gait` and
	 * says whether a step is due, taking it off. With no gait, the body is at rest: the progress is reset to
	 * StartProgress, ready for the first step of its next walk. Never more than one step comes of one call.
	 */
	bool Advance(float& Progress, EGait Gait, float Distance);

	/**
	 * The steps are recordings (OpenGameArt's "Footsteps" by GboxMikeFozzy,
	 * CC0: shoes on concrete, imported by tools/art/wave14_sounds.py), six
	 * of each: a walk's, and a sprint's (the same steps quicker and harder).
	 * The sound of step `Variant` (any number) in `Gait`: a crouch's is a
	 * walk's, heard more softly (Volume).
	 */
	constexpr int32 Variants = 6;
	FString SoundPath(EGait Gait, int32 Variant);
}
