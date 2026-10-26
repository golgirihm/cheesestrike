// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FWeaponStats;

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
		/** Elimination with a last stand: a player hurt enough goes down with only the sidearm, to be executed by an enemy up close, revived by a teammate or to get up alone. */
		Execution,
		/** Everyone for themself, each kill a step up a ladder of weapons; the first to a kill with the last, the knife, wins. */
		GunGame,
		/** Two teams; a side scores while only its players stand in the hill, which moves every minute; the first to the points wins. */
		KingOfTheHill,
		/** King of the Hill, everyone for themself: a player scores while they stand in the hill alone. */
		SoloKingOfTheHill,
	};
	constexpr int32 NumModes = 7;

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
		/** A side (or in the solo mode, a player) reaches the points by holding the hill (CheeseStrikeHills); the kill limit is the points. */
		Hill,
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
		/** The kill limit a host who names none plays to (King of the Hill's points); 0 for a mode that has none. */
		int32 DefaultKillLimit;
		/** Whether a player hurt enough goes down into a last stand rather than dying (Execution). */
		bool bLastStand;
		/** Whether kills in a row earn streaks (not in Gun Game, which is about the weapons). */
		bool bStreaks;
	};
	extern const FModeInfo Modes[NumModes];
	/** The modes in the order the lobby lists them: the teams' first, free-for-all last. */
	extern const EMode LobbyOrder[NumModes];
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
	constexpr float RoundTime = 180.0f;
	/** What a host may pick for a round's length in the lobby, in seconds (1 to 5 minutes; no overtime). */
	constexpr float RoundTimeChoices[] = {60.0f, 120.0f, 180.0f, 240.0f, 300.0f};
	constexpr float RoundBreak = 6.0f;
	/**
	 * As a round's time runs out, everyone is told how long is left, at
	 * these seconds left (the clock is on the score at the bottom all
	 * along): RoundWarning says it ("30 SECONDS LEFT IN THE ROUND"; empty
	 * away from them), NextRoundWarning is the seconds from now, with
	 * `Left` left (and `Within` seconds that must pass first, a count
	 * before the round), to the next warning; none (0) after the last.
	 */
	constexpr float RoundWarnings[] = {30.0f, 10.0f};
	FString RoundWarning(float Left);
	float NextRoundWarning(float Left, float Within);
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
		/** Of those, how many are still in (alive, down or not), and their health together. */
		int32 Alive = 0;
		float Health = 0.0f;
		/** Of those still in, how many are down (Execution's last stand). */
		int32 Down = 0;
		/**
		 * Whether it is out: nobody left in. Someone down is still in, even
		 * with the whole side down (Execution): each must be executed, or
		 * they get up by themselves (SelfReviveTime).
		 */
		bool IsOut() const { return Alive <= 0; }
		/** How many are standing: in, and not down. */
		int32 Standing() const { return Alive - Down; }
	};
		/**
		 * Whether a round of Elimination is over, and who took it (`OutWinner`,
		 * NoTeam for a draw). It is over once a side with players is out
		 * (FSide::IsOut: none left in; someone down is in) while the other has players (nothing ends while a side is
		 * empty: there is nobody to beat), and the other side takes it; both
		 * gone at once is a draw. When its time is up (`bTimeUp`) there is no
		 * overtime: if both sides still have someone in (alive, down or not) it
		 * is a draw; otherwise the side with someone in takes it.
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
	 * Whose starts side `Team` comes in at in round `Round` (1 the first, 0
	 * before any): its own, but in a mode played in rounds (`bRounds`) the
	 * other side's every second round, so that the sides swap ends each round.
	 */
	int32 StartSide(int32 Team, int32 Round, bool bRounds);
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

	/** How near an enemy must be to finish a player down, by a shot or by Interact (cm): no further, from any weapon or streak, does them any harm. */
	constexpr float FinishRange = 250.0f;
	/**
	 * How near Interact reaches a player down (cm). A press executes an
	 * enemy at once (CheeseStrikeExecution: an animation of a couple of
	 * seconds, the executor unhurt through it); reviving a teammate is held
	 * for ReviveTime.
	 */
	constexpr float InteractRange = 220.0f;
	constexpr float ReviveTime = 3.0f;
	/** The health a revived player gets up with. */
	constexpr float ReviveHealth = 50.0f;
	/** Down and not executed, a player gets up by themself after SelfReviveTime, with SelfReviveHealth: low. */
	constexpr float SelfReviveTime = 20.0f;
	constexpr float SelfReviveHealth = 25.0f;
	/** Seconds until someone down at `DownedAt` gets up by themself, at `Now` (0 once it is time). */
	float SelfReviveLeft(float DownedAt, float Now);
	/** The countdown to that shows to the player down, and to anyone within this of them (cm) who can see them. */
	constexpr float SelfReviveShownWithin = 2000.0f;

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
	 * puts them down instead, every time: getting up again (revived, or by
	 * themself) they can go down again, as often as it comes. Outside a
	 * mode with a last stand (`bLastStand`), every hit hurts.
	 */
	ELastStandHit LastStandHit(bool bLastStand, bool bDowned, float Health, float Damage, bool bEnemyClose);

	/** What Interact does to a player down: a teammate's revives them, an enemy's executes them; nothing to anyone else, out of reach, or standing. */
	enum class EInteract : uint8
	{
		None,
		Revive,
		Execute,
	};
	EInteract InteractWith(bool bTargetDowned, bool bSameSide, float Distance);
	/** Seconds Interact is held for `Kind`: ReviveTime to revive; none (a press) to execute. */
	float InteractTime(EInteract Kind);
	/** What the HUD says over someone down in reach: "PRESS E  ·  EXECUTE BOB", "HOLD LB  ·  REVIVE ANN" (`Key` the Interact key's name). */
	FString InteractPrompt(EInteract Kind, const FString& Key, const FString& Name);
	/** The countdown to getting up, as the HUD shows it: whole seconds, rounded up ("20" to "1"). */
	int32 SelfReviveSeconds(float Left);

	/**
	 * Down, a player barely fights: lying on their back, propped on an
	 * elbow, hurt, the pistol held out one-handed. Their shots do
	 * DownedDamage of their damage, come DownedFireInterval times further
	 * apart, kick DownedKick times as hard, and spread over DownedSpread
	 * times the hip's cone from the first shot of a run (no steady first
	 * shot); the view sways heavily (DownedSway) and is dark and blurred
	 * past arm's length. A pest to an enemy who takes their time, never a
	 * threat to one who comes to execute them. Bots down fight by the same
	 * weapon.
	 */
	constexpr float DownedDamage = 0.35f;
	constexpr float DownedFireInterval = 3.0f;
	constexpr float DownedSpread = 2.0f;
	constexpr float DownedKick = 2.5f;
	/** The heavy sway of a hurt player's view: degrees of yaw and pitch at `Seconds` (both at most DownedSwayYaw and DownedSwayPitch). */
	constexpr float DownedSwayYaw = 5.0f;
	constexpr float DownedSwayPitch = 3.0f;
	FRotator DownedSway(float Seconds);
	/** `Weapon` as someone down fires it. */
	FWeaponStats DownedWeapon(const FWeaponStats& Weapon);

	// Gun Game.

	/**
	 * What a kill does in Gun Game, where each player carries only their
	 * step's weapon (the knife only once it is the step: no changing weapons).
	 */
	struct FGunGameKill
	{
		/** The killer's step on the ladder after it. */
		int32 KillerLevel = 0;
		/** Whether it wins the match: a kill on the last step. */
		bool bWins = false;
	};
	/** A kill by a player on step `KillerLevel` of a ladder `Steps` long. */
	FGunGameKill GunGameKill(int32 KillerLevel, int32 Steps);

	/**
	 * A side's colour, the game's one, on the lobby's chips and columns, the
	 * HUD and the announcements: The Gouda Guys' orange (Okabe and Ito's)
	 * and the Mozza Radicals' vivid violet (a hue far from the orange, and from
	 * the solo hill's green and red); any other team, as in a
	 * free-for-all, a cheese yellow.
	 */
	FLinearColor TeamColour(int32 Team);

	/** What the scoreboard's title says: the mode's name, in capitals. */
	FString Title(EMode Mode);
	/** What it plays to: "first to 20 kills", "first team to 50 kills", "first to win 4 rounds". */
	FString Goal(EMode Mode, int32 KillLimit, int32 RoundLimit);
}
