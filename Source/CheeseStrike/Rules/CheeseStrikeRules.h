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

	/** A map a host may pick: its name in the lobby, its level's package, and its picture in the lobby (a file in Content/UI/Maps). */
	struct FMapChoice
	{
		const TCHAR* Name;
		const TCHAR* Level;
		const TCHAR* Picture;
	};
	/** The maps, the first picked to begin with when hosting from the front end. */
	extern const FMapChoice Maps[];
	int32 NumMaps();
	/** Which of Maps a level's package (or its path with the object name) is, or INDEX_NONE. */
	int32 MapIndexFor(const FString& LevelPackage);
	/** The map a URL's `?Map=` picks, or `Otherwise` if it picks none of Maps. */
	int32 MapFrom(const FString& Options, int32 Otherwise);
	/**
	 * How many players a match that the lobby started on another map waits
	 * for before it begins (`?Players=`; 0 when it wasn't), so that bots fill
	 * the slots of nobody still on the way; and how long it waits at most.
	 */
	int32 PlayersFrom(const FString& Options);
	constexpr float WaitForPlayers = 15.0f;
	/** The seed the bots' names were drawn with (`?BotSeed=`), so that a travel keeps the names the lobby showed; or `Otherwise`. */
	int32 BotSeedFrom(const FString& Options, int32 Otherwise);

	/** How much eating a block of cheese takes (docs/unreal/design.md, "The mice"). */
	constexpr float CheeseBlockHealth = 400.0f;

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

	/** The two sides, by name, and how many each holds: the match holds MaxPlayers in all. */
	constexpr int32 Teams = 2;
	constexpr int32 TeamSize = 8;
	extern const TCHAR* const TeamNames[Teams];
	/** Which side a newcomer joins: the one with fewer, the first among equals. */
	int32 TeamToJoin(const int32 (&Counts)[Teams]);

	/** The bots' names, cheese and all; there are at least enough for every slot. */
	int32 NumBotNames();
	/** `Count` of them, in an order drawn with `Seed`, no two alike. */
	TArray<FString> BotNames(int32 Seed, int32 Count);
	/** How many bots fill in: the URL's `?Bots=`, or every empty slot (-1) without it. */
	int32 BotsFrom(const FString& Options);
	/** How many bots a match with `Players` players has when `BotsWanted` of them are asked for (-1 for every empty slot). */
	int32 BotsIn(int32 Players, int32 BotsWanted);

	/** One line of a team's column in the lobby. */
	struct FRosterEntry
	{
		FString Name;
		int32 Team = 0;
		bool bBot = false;
	};
	/**
	 * Team `Team`'s column: its players, then bots from `Bots` (the names
	 * the match drew, the first TeamSize of them for the first team and so
	 * on) until it is full; or, if `BotsWanted` (0 or more) says how many,
	 * that many in all, dealt one at a time to whichever side has fewer.
	 */
	TArray<FRosterEntry> TeamColumn(int32 Team, const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted);
}
