// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Turning the loadout page's hero (ACheeseStrikeHeroView) by hand, as plain
 * rules: a drag across the picture with the mouse, or the right stick, turns
 * the figure; left alone for IdleSeconds it turns by itself again from
 * where it was left, easing back up to its slow pace over ResumeSeconds.
 */
namespace CheeseStrikeHeroTurn
{
	/** Degrees the figure turns for a Slate unit dragged across (the picture is 330 wide: a drag across it is about half a turn). */
	constexpr float DragDegreesPerUnit = 0.55f;
	/** Degrees a second the right stick held over turns it, and the stick's dead zone. */
	constexpr float StickDegreesPerSecond = 160.0f;
	constexpr float StickDeadZone = 0.25f;
	/** Seconds after the last turn by hand before it turns by itself again, and how long it takes to come back up to its pace. */
	constexpr float IdleSeconds = 3.0f;
	constexpr float ResumeSeconds = 0.6f;
	/**
	 * The share of its own slow pace the figure turns at by itself, `SinceTurned` seconds after it was last turned by hand
	 * (negative: never): none until IdleSeconds, then easing up to all of it over ResumeSeconds.
	 */
	float AutoShare(double SinceTurned);
	/** Degrees the right stick at `Axis` (-1 to 1, right positive) turns the figure over `DeltaSeconds`: none in the dead zone, then rising to the full rate at the edge. */
	float StickTurn(float Axis, float DeltaSeconds);
}
