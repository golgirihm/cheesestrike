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
	/** Seconds a killed player waits before coming back, unless the host says otherwise. */
	constexpr float RespawnDelay = 5.0f;
	/** Seconds a player just come back can't be hurt, unless they fire first. */
	constexpr float SpawnProtection = 3.0f;
	/** Kills that win a free-for-all unless the host says otherwise. */
	constexpr int32 KillLimit = 20;
	/** Seconds a kill stays in the feed, and how many it shows at most. */
	constexpr float KillFeedLife = 6.0f;
	constexpr int32 KillFeedLength = 5;
	/** The longest a player's name may be, in letters: as long as a Steam name. The HUD cuts a long one to fit, with dots. */
	constexpr int32 MaxNameLength = 32;
	/**
	 * Seconds from the host starting a match to its first shot, and seconds from its end until everyone is back in the
	 * lobby: the results a moment, then the podium (CheeseStrikePodiums::After and Seconds).
	 */
	constexpr float MatchCountdown = 5.0f;
	constexpr float ResultsTime = 18.0f;
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
	/** The maps, in the order the lobby offers them. */
	extern const FMapChoice Maps[];
	int32 NumMaps();
	/** The map a lobby hosted from the front end begins with (HostOptions): Gruyère Grotto. */
	int32 DefaultMap();
	/**
	 * The URL options a match hosted from the front end begins its lobby
	 * with, before the host changes anything: Gruyère Grotto, Deathmatch to
	 * 50 team kills, back 5 s after a death, no bots.
	 */
	FString HostOptions();
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
	/**
	 * Whether a match the lobby sent everyone to begins: once `Ready` of the
	 * `Expected` players have said their level is loaded and their view can
	 * be shown, or, with somebody in, after WaitForPlayers seconds of
	 * waiting for anyone who never comes.
	 */
	bool ShouldBegin(int32 Expected, int32 Ready, int32 In, float SecondsWaited);
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
	/** The seats in a match, players and bots together. */
	constexpr int32 SeatCount = Teams * TeamSize;
	/** How many bots a hosted match starts with, before the host changes it in the lobby. */
	constexpr int32 DefaultBots = 0;
	/** How many bots fill in: the URL's `?Bots=` (-1 for every empty slot), or DefaultBots without it. */
	int32 BotsFrom(const FString& Options);
	/** How many bots a match with `Players` players has when `BotsWanted` of them are asked for (-1 for every empty slot). */
	int32 BotsIn(int32 Players, int32 BotsWanted);
	/** The most bots the host may ask for with `Players` players in: every seat they leave. */
	int32 MostBots(int32 Players);

	/** One line of a team's column in the lobby: a player, a bot, or an empty seat. */
	struct FRosterEntry
	{
		FString Name;
		int32 Team = 0;
		bool bBot = false;
		/** A seat nobody holds, drawn as a placeholder. */
		bool bEmpty = false;
		/** A bot that will stand in: its number, in the order the bots were added (0 the first); INDEX_NONE for a player. */
		int32 Bot = INDEX_NONE;
	};
	/**
	 * Every seat taken, in the order it was taken: the `Players` in the
	 * order they joined (as given), then the bots in the order they were
	 * added, the nth named `Bots[n]`. How many bots: every seat the players
	 * leave, or `BotsWanted` (0 or more), at most what is left. Each bot sits
	 * on the side `BotSides` gives it (the host moved it there), or else is
	 * dealt to whichever side has fewer, the first among equals; a side
	 * that is full (TeamSize, players first) sends it to the other. A
	 * player in `Players` who is a bot (the match's own, once in) keeps
	 * their seat and takes one of the bots' places.
	 */
	TArray<FRosterEntry> Seats(const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, const TArray<int32>& BotSides = {});
	/** Team `Team`'s column: its seats of Seats, in the same order, at most TeamSize players. */
	TArray<FRosterEntry> TeamColumn(int32 Team, const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, const TArray<int32>& BotSides = {});
	/** How many of `Seats` hold someone: players and bots. */
	int32 Taken(const TArray<FRosterEntry>& Seats, int32 Team = INDEX_NONE);
	/** Whether a player may go over to side `Team`: it has fewer than TeamSize players (bots make way for them). */
	bool HasRoomForAPlayer(const TArray<FRosterEntry>& Players, int32 Team);
	/**
	 * The host moves bot `Bot` to the other side, if it has an open seat:
	 * every bot keeps the side it has now (BotSides pins them all, so that
	 * the dealing doesn't even the sides out again), and that one goes over.
	 * False, and `BotSides` as it was, if it can't.
	 */
	bool MoveBot(const TArray<FRosterEntry>& Players, const TArray<FString>& Bots, int32 BotsWanted, TArray<int32>& BotSides, int32 Bot);
	/** The sides the host moved the bots to, as a URL carries them (`?BotSides=0110`: a digit a bot), and back; empty for none. */
	FString BotSidesOption(const TArray<int32>& BotSides);
	TArray<int32> BotSidesFrom(const FString& Options);
	/**
	 * Why the host can't start the match with these `Seats` (Seats: players
	 * and bots), or nothing if they can: one alone has nobody to play, and a
	 * match of sides needs someone on each.
	 */
	FString WhyNotStart(const TArray<FRosterEntry>& Seats, bool bTeams);
	/**
	 * A lobby column's seats, as many as `NumSeats`: who is in `Column`
	 * (players, then bots), then empty seats of `Team` to fill it. A team's
	 * column has TeamSize seats; a free-for-all's one column has SeatCount.
	 */
	TArray<FRosterEntry> WithEmptySeats(TArray<FRosterEntry> Column, int32 NumSeats, int32 Team = 0);
}
