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
 * has sunk. A long fall onto anything hurts, as in Counter-Strike: nothing
 * from a drop up to SafeDrop, then more the faster the landing, until a drop
 * of DeadlyDrop kills; a kill by a fall is credited as a fall into the fondue
 * is. Plain functions, tested without a world (CheeseStrike.Rules.Falls*).
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

	/**
	 * Fall damage. A drop of up to SafeDrop (cm, from the top of the fall,
	 * a jump's rise included) is free: a jump down from The Crumb Kitchen's
	 * worktop (4.5 m) or a 4 m level of the Grotto, the Creamery's hall or the
	 * Labyrinth, and the Fondue's gantry (3.2 m). Over it, the damage grows
	 * with the speed of the landing, from nothing to DeadlyDamage at
	 * DeadlyDrop: the Market's belfry (7 m) to the square takes about a
	 * fifth, the Grotto's gallery (8 m) to its floor about a third.
	 */
	constexpr float SafeDrop = 550.0f;
	constexpr float DeadlyDrop = 1600.0f;
	constexpr float DeadlyDamage = 100.0f;
	/** The drop (cm) that a landing at `Speed` (cm/s, downward) under `Gravity` (cm/s², positive) ends. */
	float DropFor(float Speed, float Gravity);
	/** The damage a landing after a drop of `Drop` cm does: none up to SafeDrop, DeadlyDamage from DeadlyDrop, whole numbers. */
	float DamageFor(float Drop);
	/** Whether a bot steps off an edge with the ground `Drop` cm below it (or none within `Drop`): only where the fall can't hurt it. */
	bool BotMayStepOff(float Drop);
	/** What the kill feed and the death screen say of a fall onto the ground nobody is credited with. */
	FString HardLandingWords();

	/** Seconds a mouse that fell in is gone before it comes back over the edge. */
	constexpr float MouseGoneFor = 8.0f;
}
