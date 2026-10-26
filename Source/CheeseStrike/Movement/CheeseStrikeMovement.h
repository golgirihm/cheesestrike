// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * How players move: the numbers, and the rules that are only arithmetic, kept
 * apart from the engine so that they can be tested alone. The movement
 * component (CheeseStrikeMovementComponent) applies them.
 *
 * The numbers come from the Godot game (docs/unreal/design.md, "The player"),
 * which was three times life size: its ratios are kept, and the raw speeds
 * are a shooter's at life size. Lengths are centimetres, times seconds.
 */
namespace CheeseStrikeMovement
{
	/** Walking, in cm/s (a fifth slower than it was at 450; the sprint follows). */
	constexpr float WalkSpeed = 360.0f;
	/** Sprinting, with the key held, is this much faster. */
	constexpr float SprintFactor = 1.5f;
	/** Aiming down the sights slows a player to this share of walking. */
	constexpr float AimFactor = 0.6f;
	/** So does crouching, to this share, and it rules out sprinting. */
	constexpr float CrouchFactor = 0.5f;
	/**
	 * Off the ground a player can only steer: their speed across the ground
	 * changes by no more than this each second, in cm/s, whether they push a
	 * way or let go.
	 */
	constexpr float AirSteer = 467.0f;
	/** Ground acceleration, the engine's usual. */
	constexpr float Acceleration = 2048.0f;
	/** Upward speed of a jump, in cm/s: at the engine's gravity, about half a metre up, quickly. */
	constexpr float JumpSpeed = 330.0f;
	constexpr float Gravity = 980.0f;
	/** The tallest step walked up without jumping. */
	constexpr float StepHeight = 20.0f;
	/** The capsule: the mannequin's, and how far it drops when crouched. */
	constexpr float CapsuleRadius = 34.0f;
	constexpr float CapsuleHalfHeight = 96.0f;
	constexpr float CrouchedHalfHeight = 44.0f;
	/** Crouches and stands in about an eighth of a second. */
	constexpr float CrouchRate = 8.0f;
	/** The eyes, above the capsule's centre: the mannequin's head when standing, and the same above a crouched capsule, so they drop by what the capsule does. */
	constexpr float EyeHeight = 64.0f;
	constexpr float CrouchedEyeHeightAboveCentre = 64.0f;
	/**
	 * Crouching at a sprint slides: the player keeps going the way they were,
	 * a little faster at first, easing down to a crouched walk over SlideTime
	 * seconds, without steering. They must be going at least SlideFrom of
	 * walking speed to start one, and wait SlideRest after one for the next.
	 */
	constexpr float SlideTime = 0.9f;
	constexpr float SlideBoost = 1.15f;
	constexpr float SlideFrom = 1.2f;
	constexpr float SlideRest = 0.4f;

	/** What a heavy weapon in hand (the Munster Machine Gun) slows its carrier to. */
	constexpr float HeavyFactor = 0.85f;
	/** The top speed `Speed` becomes with a heavy weapon. */
	float WithWeapon(float Speed, bool bHeavy);

	/** How fast a player moves across the ground. */
	float Speed(bool bSprinting, bool bAiming, bool bCrouching);

	/** Whether a player going at `GroundSpeed` can begin a slide. */
	bool CanSlide(float GroundSpeed);

	/** How fast a player `Elapsed` seconds into a slide begun at `From` cm/s is going. */
	float SlideSpeed(float From, float Elapsed);

	/** How high a jump at `JumpSpeed` under `Gravity` goes, in cm. */
	float JumpHeight(float InJumpSpeed = JumpSpeed, float InGravity = Gravity);

	/** The share of acceleration that steers in the air: AirSteer of Acceleration. */
	float AirControl();
}
