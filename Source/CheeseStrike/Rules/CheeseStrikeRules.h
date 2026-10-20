// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The rules of a match, as plain functions over plain values, so that they
 * can be tested without a world: when a player comes back and where, who
 * is winning, and when a match is over. The numbers are the design
 * document's (docs/unreal/design.md, "Health and armour" and "Modes").
 * ACheeseStrikeGameMode applies them.
 */
namespace CheeseStrikeRules
{
	/** Seconds a killed player waits before coming back. */
	constexpr float RespawnDelay = 3.0f;
	/** Seconds a player just come back can't be hurt, unless they fire first. */
	constexpr float SpawnProtection = 3.0f;
	/** Kills that win a free-for-all unless the host says otherwise. */
	constexpr int32 KillLimit = 20;
	/** Seconds a kill stays in the feed, and how many it shows at most. */
	constexpr float KillFeedLife = 6.0f;
	constexpr int32 KillFeedLength = 5;
	/** Seconds from the host starting a match to its first shot, and seconds the results stay up before everyone is back in the lobby. */
	constexpr float MatchCountdown = 5.0f;
	constexpr float ResultsTime = 10.0f;
	/** What a host may pick (docs/unreal/design.md, "Modes"), and what is picked to begin with. */
	constexpr int32 KillLimitChoices[] = {5, 10, 20, 30, 50};
	constexpr float RespawnDelayChoices[] = {1.0f, 3.0f, 5.0f};

	/** Whether a match with these URL `Options` begins as soon as there is a player (`?StartNow`), rather than waiting in the lobby for the host. */
	bool StartsAtOnce(const FString& Options);

	/** One player's line on the scoreboard. */
	struct FStanding
	{
		FString Name;
		int32 Kills = 0;
		int32 Deaths = 0;
		/** Tells players with the same kills and deaths apart, in the order they came. */
		int32 Order = 0;
	};

	/**
	 * Which of `Spots` to come back at: the one whose nearest enemy is
	 * furthest away; with no enemies, the first. -1 with no spots.
	 */
	int32 FurthestFrom(TArrayView<const FVector> Spots, TArrayView<const FVector> Enemies);

	/** `Standings` in scoreboard order: most kills first, then fewest deaths, then the order they came in. */
	TArray<FStanding> Sorted(TArray<FStanding> Standings);

	/** Whether a match with a kill limit of `Limit` is over when the most anyone has is `MostKills`. */
	bool IsOver(int32 MostKills, int32 Limit);

	/** The name of whoever leads `Standings` (sorted first), or nothing. */
	FString Leader(const TArray<FStanding>& Standings);

	/** The number option `Name` in a map URL's `Options` (`?Name=Value`), or `Otherwise` if it isn't there or isn't a number. */
	float Option(const FString& Options, const TCHAR* Name, float Otherwise);

	/** The kill limit and respawn delay a host asked for in a map URL's `Options`, within what the design allows. */
	int32 KillLimitFrom(const FString& Options);
	float RespawnDelayFrom(const FString& Options);
}
