// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ACheeseStrikeCharacter;

/**
 * A light aim assist for the look stick, as shooters give gamepads: the view
 * turns slower while the crosshair is on or near an enemy (slowdown), and,
 * while the player is turning, is drawn a little toward them (a pull).
 * Only the look stick gets it (ACheeseStrikeCharacter::StickLookInput); the
 * mouse and the bots never do. The rules are plain functions, tested without
 * a world; ForCharacter finds the enemies in view. The numbers are console
 * variables (CheeseStrike.AimAssist.*), and the options turn it off.
 */
namespace CheeseStrikeAimAssist
{
	/** An enemy as the assist sees it: the line up the middle of its body, from the hips to the head. */
	struct FTarget
	{
		FVector Low = FVector::ZeroVector;
		FVector High = FVector::ZeroVector;
	};

	struct FTuning
	{
		/** How far off an enemy is helped at all, in cm. */
		float Range = 4000.0f;
		/** How far either side of the body's middle line the crosshair gets the whole of it, in cm, and at least so many degrees, however far off the enemy is. */
		float Radius = 45.0f;
		float MinDegrees = 1.0f;
		/** Out to this many times that it fades to nothing. */
		float Falloff = 2.5f;
		/** The turn's speed on an enemy: a share of what the stick asks, from the hip and down the sights. */
		float Slowdown = 0.6f;
		float AimedSlowdown = 0.5f;
		/** The pull toward an enemy while turning, degrees a second, from the hip and down the sights. */
		float Pull = 3.0f;
		float AimedPull = 5.0f;
	};

	/** The tuning as the console variables have it now. */
	FTuning Tuning();
	/** Whether the assist is on: the console variable and the player's options. */
	bool IsOn();

	struct FAssist
	{
		/** What the stick's turn is multiplied by. */
		float TurnScale = 1.0f;
		/** Degrees to turn the view by this frame toward the enemy, across (yaw) and up (pitch). */
		float PullYaw = 0.0f;
		float PullPitch = 0.0f;
		/** How strongly the enemy chosen holds the crosshair, 0 to 1; 0 with none. */
		float Strength = 0.0f;
		/** Which of the targets it is, or INDEX_NONE. */
		int32 Target = INDEX_NONE;
	};

	/** How strongly a target `Degrees` off the crosshair holds it, when its radius is `RadiusDegrees`: 1 inside it, fading to 0 at Falloff times it. */
	float Strength(float Degrees, float RadiusDegrees, const FTuning& Tuning);

	/**
	 * The help for a view from `Eye` along `View` with `Targets` in sight:
	 * the strongest's slowdown, and, while `bTurning` (the stick is pushed),
	 * its pull over `DeltaSeconds`, never past it.
	 */
	FAssist Assist(const FVector& Eye, const FRotator& View, const TArray<FTarget>& Targets, bool bAimed, bool bTurning, float DeltaSeconds, const FTuning& Tuning);

	/** The help for `Character`'s player this frame: the enemies alive in its view that it can see, by the tuning now. */
	FAssist ForCharacter(const ACheeseStrikeCharacter* Character, bool bTurning, float DeltaSeconds);
}
