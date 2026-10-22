// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Falls that kill: a map with a drop that is death (The Fondue Floor's vat of
 * fondue) sets a kill height, its World Settings' KillZ (tools/art/map_kit.py,
 * kill_below). Whoever goes below it is killed by the fall, which counts as a
 * kill for whoever hurt them just before; a mouse below it has fallen in, and
 * comes back over the edge a while later; cheese that comes to rest below it
 * has sunk. Plain functions, tested without a world (CheeseStrike.Rules.Falls*).
 */
namespace CheeseStrikeFalls
{
	/** A KillZ at or below this is the engine's own (far under any map): the map has no kill height. */
	constexpr float NoKillHeight = -100000.0f;
	bool HasKillHeight(float KillZ);
	/** The kill height of `World`'s level, or NoKillHeight if it has none (or there is no world). */
	float KillHeight(const UWorld* World);
	/** Whether `Z` is under `World`'s kill height (never, on a map without one). */
	bool IsBelowKillHeight(const UWorld* World, float Z);

	/** Seconds after being hurt that a fall still counts as a kill for whoever hurt them. */
	constexpr float CreditFor = 5.0f;
	/** Whether a fall at `Now` counts for whoever hurt the faller at `HurtAt`. */
	bool CreditsHurter(float HurtAt, float Now);

	/**
	 * What the kill feed and the death screen say of a fall nobody is credited
	 * with, into the place named `Place` (a hazard's place is a common noun:
	 * "The Fondue" makes "fell into the fondue"), or "fell" with no place.
	 */
	FString Words(const FString& Place);

	/** Seconds a mouse that fell in is gone before it comes back over the edge. */
	constexpr float MouseGoneFor = 8.0f;
}
