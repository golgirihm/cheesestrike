// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The Practice Range (docs/unreal/maps/range.md), as plain functions tested
 * without a world (CheeseStrike.Rules.ThePracticeRange): a yard behind a
 * firing line with a stand for every weapon that can lie on one (each
 * `Instant`: taken, it is back at once, so the row is never empty), markers
 * at 5, 10, 20 and 40 m, a wall of cheese targets that shots break and that
 * grow back (ACheeseStrikeTarget), and four dummies: bots that stand where
 * the map puts them (its `DummySpot` points) and never move, aim or fire,
 * and come back where they stood when killed (`?BotSkill=dummy`,
 * CheeseStrikeBots::DummiesFrom). The front end's PRACTICE plays it alone,
 * with no lobby, a free-for-all with no mice that never ends by itself.
 */
namespace CheeseStrikeRange
{
	/** The range's level. */
	extern const TCHAR* const Level;
	/** The dummies the range has, one for each of its spots. */
	constexpr int32 Dummies = 4;
	/** The kill limit of a practice match: more than anyone will reach, so that it never ends by itself. */
	constexpr int32 KillLimit = 1000;
	/** Seconds before a dummy (or the player) comes back. */
	constexpr float RespawnDelay = 2.0f;
	/** The options the front end's PRACTICE plays the range with: alone, at once, its dummies, no mice, no count. */
	FString PracticeOptions();
	/** The URL the front end travels to for PRACTICE: the range's level and PracticeOptions. */
	FString PracticeUrl();

	/** The tag of the points a map's dummies stand at (map_kit's `dummy_spot`). */
	extern const TCHAR* const DummySpotTag;
	/**
	 * The spot a dummy comes in at, of `Spots`, when the other dummies hold
	 * `Taken`: the first nobody holds, or with every one held, the dummies
	 * share them in turn (`Taken.Num()` round them). INDEX_NONE with none.
	 */
	int32 DummySpotFor(const TArray<int32>& Taken, int32 Spots);

	/** A cheese target's health: a burst of the Colby Carbine, two shots of the Blaster, a shotgun's blast break it. */
	constexpr float TargetHealth = 40.0f;
	/** Seconds a broken target is gone before it grows back. */
	constexpr float RegrowSeconds = 3.0f;
	/** Seconds it takes to grow back to its full size. */
	constexpr float GrowSeconds = 0.4f;
	/** How a target with `Health` left looks: 0 whole to 2 nearly broken (the cheese material's holes); broken at none. */
	int32 TargetStage(float Health);
	/** The share of its size a target shows `Since` seconds after it began to grow back (from a small seed to whole). */
	float GrownShare(float Since);
}
