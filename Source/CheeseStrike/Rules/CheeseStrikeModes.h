// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The modes a match is played in, as plain functions over plain values, so
 * that they can be tested without a world (docs/unreal/design.md,
 * "Modes"): what each is called, whether it is played in two teams, how it
 * is won; who may hurt whom; who takes a round of Elimination; and where a
 * side comes in. ACheeseStrikeGameMode applies them, and
 * ACheeseStrikeGameState tells every machine which mode is on.
 */
namespace CheeseStrikeModes
{
	/** The modes, in the order a host is offered them. The URL names one by its key (`?Mode=Deathmatch`) or its number (`?Mode=1`). */
	enum class EMode : uint8
	{
		/** Everyone for themself; the first to the kill limit wins. */
		FreeForAll,
		/** Two teams; the first team to the kill limit wins. */
		Deathmatch,
		/** Two teams, one life a round; a round is won by the side left standing, the match by the first to the round limit. */
		Elimination,
		/** Elimination with a last stand: a player hurt enough goes down with only the sidearm, to be finished by an enemy up close or revived by a teammate. */
		Execution,
		/** Everyone for themself, each kill a step up a ladder of weapons; the first to a kill with the last, the knife, wins. */
		GunGame,
	};
	constexpr int32 NumModes = 5;

	/** How a mode is won. */
	enum class EWinBy : uint8
	{
		/** A player reaches the kill limit. */
		PlayerKills,
		/** A team's kills together reach the kill limit. */
		TeamKills,
		/** A team wins the round limit's rounds. */
		Rounds,
		/** A player makes a kill with the last weapon of Gun Game's ladder. */
		Ladder,
	};

	struct FModeInfo
	{
		EMode Mode;
		/** As players read it: "Free-for-all". */
		const TCHAR* Name;
		/** As the URL names it: "FreeForAll". */
		const TCHAR* Key;
		/** A line for the lobby. */
		const TCHAR* Summary;
		/** Whether it is played in two teams (CheeseStrikeRules::TeamNames): the lobby's two columns, team colours, friendly fire off. */
		bool bTeams;
		EWinBy WinBy;
		/** Whether the killed come back during play; in Elimination they are out until the next round. */
		bool bRespawns;
		/** The kill limit a host who names none plays to; 0 for a mode that has none. */
		int32 DefaultKillLimit;
		/** Whether a player hurt enough goes down into a last stand rather than dying (Execution). */
		bool bLastStand;
		/** Whether kills in a row earn streaks (not in Gun Game, which is about the weapons). */
		bool bStreaks;
	};
	extern const FModeInfo Modes[NumModes];
	const FModeInfo& Info(EMode Mode);
	/** The mode with number `Index` (as replicated), or free-for-all for one there isn't. */
	EMode ModeAt(int32 Index);

	/** The mode a map URL's `?Mode=` names, by key (any case) or number; free-for-all without one, or for one there isn't. */
	EMode ModeFrom(const FString& Options);
	/** The kill limit for `Mode`: the URL's `?KillLimit=` (1 to 1000), or the mode's own without it. */
	int32 KillLimitFor(EMode Mode, const FString& Options);

	/** Elimination: rounds a side must win to take the match (the design's "Rounds": 4), and what a host may pick. */
	constexpr int32 RoundLimit = 4;
	constexpr int32 RoundLimitChoices[] = {2, 3, 4, 5, 7};
	/** Seconds a round lasts at most, and the pause between rounds (the Godot game's ROUND_BREAK). */
	constexpr float RoundTime = 120.0f;
	constexpr float RoundBreak = 6.0f;
	/** How long into a round a newcomer may still come in; after it they are out until the next. */
	constexpr float LateJoin = 10.0f;
	/** The URL's `?Rounds=` (1 to 15) and `?RoundTime=` (30 to 600 s), or the usual. */
	int32 RoundLimitFrom(const FString& Options);
	float RoundTimeFrom(const FString& Options);

	/** No team: a drawn round, or nobody yet. */
	constexpr int32 NoTeam = -1;

	/** The team whose score has reached `Limit`, the first such; NoTeam for none, or for no limit. */
	int32 TeamAtLimit(TArrayView<const int32> Scores, int32 Limit);

	/** One side of a round of Elimination, as it stands. */
	struct FSide
	{
		/** Players on it, out or not. */
		int32 Members = 0;
		/** Of those, how many are still standing, and their health together. */
		int32 Alive = 0;
		float Health = 0.0f;
	};
	/**
	 * Whether a round of Elimination is over, and who took it (`OutWinner`,
	 * NoTeam for a draw). It is over once a side with players has none left
	 * standing while the other has players (nothing ends while a side is
	 * empty: there is nobody to beat), and the other side takes it; both
	 * gone at once is a draw. When its time is up (`bTimeUp`) it is over
	 * whatever stands: the side with more left standing takes it, then the
	 * one with more health left; level on both, a draw.
	 */
	bool RoundOver(const FSide& First, const FSide& Second, bool bTimeUp, int32& OutWinner);

	/**
	 * Whether a player on `AttackerTeam` may hurt one on `VictimTeam`. Never
	 * themself (`bSelf`; a kill from the world, with no attacker, is another
	 * matter, and allowed), never a teammate when `bTeams` (friendly fire
	 * is off, as the design has it), and nobody during the pause between
	 * rounds (`bPaused`).
	 */
	bool CanHurt(bool bTeams, int32 AttackerTeam, int32 VictimTeam, bool bSelf, bool bPaused);
	/** Whether two players are on opposite sides: in a team mode, teams that differ; otherwise anyone else. */
	bool AreEnemies(bool bTeams, int32 TeamA, int32 TeamB, bool bSame);

	/**
	 * The player-start tags (APlayerStart::PlayerStartTag) a map marks each
	 * side's starts with, as The Creamery does; a start tagged for neither
	 * ("Free") is for free-for-all only, when a map marks sides at all.
	 */
	extern const TCHAR* const TeamStartTags[2];
	/**
	 * For a map that marks no sides: which side each of `Spots` is for,
	 * 0 or 1, by halves along the line between the two that are furthest
	 * apart, so that the sides come in at opposite ends.
	 */
	TArray<int32> SidesOf(TArrayView<const FVector> Spots);
	/**
	 * Which of `Spots` to come in at: the one whose nearest enemy is
	 * furthest away (the first, with no enemies), passing over any with
	 * someone standing within `Room` of it (`Others`, friend or foe) while
	 * there are free ones. -1 with no spots.
	 */
	int32 ChooseStart(TArrayView<const FVector> Spots, TArrayView<const FVector> Enemies, TArrayView<const FVector> Others, float Room);

	// Execution's last stand.

	/** Seconds a player down can last before bleeding out, as the kill of whoever put them down. */
	constexpr float LastStandTime = 30.0f;
	/** How near an enemy must be to finish a player down, by a shot or by Interact (cm): no further, from any weapon or streak, does them any harm. */
	constexpr float FinishRange = 250.0f;
	/** How near Interact reaches a player down (cm), and the seconds it is held to finish an enemy or revive a teammate. */
	constexpr float InteractRange = 220.0f;
	constexpr float ExecuteTime = 1.0f;
	constexpr float ReviveTime = 3.0f;
	/** The health a revived player gets up with. */
	constexpr float ReviveHealth = 50.0f;

	/** What a hit does to a player in a mode with a last stand. */
	enum class ELastStandHit : uint8
	{
		/** The usual: health off, and death at none. */
		Hurt,
		/** It would kill, but they go down instead. */
		Down,
		/** They are down, and it does nothing. */
		Absorbed,
		/** They are down, and an enemy up close finishes them. */
		Finish,
	};
	/**
	 * What a hit of `Damage` does to a player with `Health` left: down
	 * (`bDowned`), only an enemy within FinishRange (`bEnemyClose`) can
	 * harm them, and that finishes them; standing, a hit that would kill
	 * puts them down instead, once a life (`bLastStandUsed`). Outside a
	 * mode with a last stand (`bLastStand`), every hit hurts.
	 */
	ELastStandHit LastStandHit(bool bLastStand, bool bDowned, bool bLastStandUsed, float Health, float Damage, bool bEnemyClose);

	/** What Interact does to a player down: a teammate's revives them, an enemy's finishes them; nothing to anyone else, out of reach, or standing. */
	enum class EInteract : uint8
	{
		None,
		Revive,
		Execute,
	};
	EInteract InteractWith(bool bTargetDowned, bool bSameSide, float Distance);
	/** Seconds Interact is held for `Kind`. */
	float InteractTime(EInteract Kind);

	// Gun Game.

	/** What a kill does in Gun Game. */
	struct FGunGameKill
	{
		/** The killer's step on the ladder after it. */
		int32 KillerLevel = 0;
		/** Whether the killed drops a step (a kill by the knife before its own step: humiliation, as in Counter-Strike). */
		bool bVictimDemoted = false;
		/** Whether it wins the match: a kill on the last step. */
		bool bWins = false;
	};
	/** A kill by a player on step `KillerLevel` of a ladder `Steps` long, `bWithKnife` or not. */
	FGunGameKill GunGameKill(int32 KillerLevel, bool bWithKnife, int32 Steps);
	/** What a Gun Game player on step `Level` carries besides its step's weapon: the knife, until the knife is the step. */
	bool GunGameKeepsKnife(int32 Level, int32 Steps);

	/** The teams' colours (the Godot game's, from Okabe and Ito's palette): The Gouda Guys a red orange, Mozza Radicals a blue; a light grey for no team. */
	FLinearColor TeamColour(int32 Team);

	/** What the scoreboard's title says: the mode's name, in capitals. */
	FString Title(EMode Mode);
	/** What it plays to: "first to 20 kills", "first team to 50 kills", "first to win 4 rounds". */
	FString Goal(EMode Mode, int32 KillLimit, int32 RoundLimit);
}
