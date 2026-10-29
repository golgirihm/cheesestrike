// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeStreaks.h"

/**
 * The achievements and the records they are earned by, as plain functions
 * over plain values, so that they can be tested without a world
 * (CheeseStrike.Achievements.*). A player's records are counts kept on their
 * own machine (UCheeseStrikeRecordsSubsystem, saved in the "Records" slot):
 * the server tells each player's machine what they did as it happens
 * (ACheeseStrikePlayerController::ClientRecord: a kill, a kill's badges, a
 * streak called in, a teammate revived, a match played to its end and won),
 * and each event adds one to the stats StatsFor names. An achievement is
 * unlocked once its stat reaches its threshold.
 *
 * The ids are the ones to create on Steamworks once the game has an App ID
 * (docs/unreal/design.md, "Achievements"); until then unlocking one logs it
 * and shows a toast on the HUD.
 */
namespace CheeseStrikeAchievements
{
	/** What the server tells a player's machine they did; `Detail` says which badge, streak or mode. */
	enum class ERecord : uint8
	{
		/** A kill of another player or bot. */
		Kill,
		/** One of a kill's badges (Detail: CheeseStrikeBadges::EBadge). */
		Badge,
		/** A streak called in (Detail: CheeseStrikeStreaks::EStreak). */
		StreakCalled,
		/** A match played to its end (Detail: CheeseStrikeModes::EMode); the records are saved with it. */
		MatchPlayed,
		/** A match won. */
		Win,
		/** A teammate down got up by one's holding Interact. */
		Revive,
	};

	/** The stats the records count. */
	namespace Stat
	{
		extern const TCHAR* Kills;
		extern const TCHAR* Headshots;
		extern const TCHAR* Assassinations;
		extern const TCHAR* Executions;
		extern const TCHAR* Revives;
		extern const TCHAR* Wins;
		extern const TCHAR* Matches;
		/** Not counted but worked out: how many of the lobby's modes have been played (ModeStat above zero). */
		extern const TCHAR* ModesPlayed;
	}
	/** The stat of matches played in `Mode`: "Mode.Deathmatch". */
	FString ModeStat(CheeseStrikeModes::EMode Mode);
	/** The stat of `Kind` called in: "Streak.CheeseStrike". */
	FString StreakStat(CheeseStrikeStreaks::EStreak Kind);
	/** The stats `Record` adds one to: a kill's "Kills"; a badge's own ("Headshots", "Assassinations", "Executions", or "Badge.REVENGE" for the rest); a streak's; a match's "Matches" and its mode's; "Wins"; "Revives". Nothing for a detail there isn't. */
	TArray<FString> StatsFor(ERecord Record, uint8 Detail);

	struct FAchievement
	{
		/** Steam's API name: "ACH_KILLS_10". */
		FString Id;
		/** As players read it. */
		FString Name;
		FString Description;
		/** The stat it watches, and how much of it unlocks it. */
		FString Stat;
		int32 Threshold = 1;
	};
	/** Every achievement: kills, headshots and assassinations at 1, 10, 100 and 1000; each of the lobby's modes played once and all of them; each streak called in once; the first execution and revive; a match won and ten. */
	const TArray<FAchievement>& All();
	/** The one with `Id`, or null. */
	const FAchievement* Find(const FString& Id);

	/** `Stat`'s value in `Stats`: as counted, or for ModesPlayed, worked out from the modes' stats. */
	int32 Value(const TMap<FString, int32>& Stats, const FString& Stat);
	bool IsUnlocked(const FAchievement& Achievement, const TMap<FString, int32>& Stats);
	/** The ids of the achievements `After` unlocks that `Before` didn't, in the table's order. */
	TArray<FString> NewlyUnlocked(const TMap<FString, int32>& Before, const TMap<FString, int32>& After);
	/** What the HUD's toast says: "ACHIEVEMENT: HEAD CHEESE". */
	FString Toast(const FAchievement& Achievement);
	/** How long the toast shows, and how long it takes to come in and go. */
	constexpr float ToastSeconds = 4.0f;
	constexpr float ToastFade = 0.3f;
}
