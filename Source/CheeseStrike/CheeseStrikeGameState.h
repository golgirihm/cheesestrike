// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "CheeseStrikeGameState.generated.h"

class ACheeseStrikePlayerState;
class APlayerState;

/** One kill, as the feed shows it. */
USTRUCT()
struct FKillFeedEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString Killer;
	UPROPERTY()
	FString Victim;
	UPROPERTY()
	bool bHead = false;
	/** What did it when it wasn't the killer's weapon: a streak's name ("Cheese Strike"); empty otherwise. */
	UPROPERTY()
	FString How;
	/** The server's time it happened. */
	UPROPERTY()
	float At = 0.0f;
};

/** Word for everyone, drawn by the HUD for a few seconds: a streak called in, a round won. */
USTRUCT()
struct FAnnouncement
{
	GENERATED_BODY()

	UPROPERTY()
	FString Text;
	/** The streak it is about (CheeseStrikeStreaks::EStreak), or 0. */
	UPROPERTY()
	uint8 Streak = 0;
	/** The side it is for, or against: the caller's team in a team mode; NoTeam otherwise. */
	UPROPERTY()
	int32 Team = CheeseStrikeModes::NoTeam;
	/** The player it is about, by name; empty for none. */
	UPROPERTY()
	FString Who;
	/** The server's time it was made. */
	UPROPERTY()
	float At = 0.0f;
};

/**
 * What everyone knows about the match: whether it is on or over (the
 * engine's match state), the mode and the limits the host set, the last
 * few kills for the feed, the teams' scores, the rounds of Elimination,
 * which sides an Unmanned Aerial Cheese shows the enemy to, the last few
 * announcements, and who won.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeGameState : public AGameState
{
	GENERATED_BODY()

public:
	ACheeseStrikeGameState();

	int32 GetKillLimit() const { return KillLimit; }
	float GetRespawnDelay() const { return RespawnDelay; }
	const TArray<FKillFeedEntry>& GetKillFeed() const { return KillFeed; }
	/** Who won, once the match is over: a player, or in a team mode a team's name; empty before. */
	const FString& GetWinnerName() const { return WinnerName; }
	/** Whether the host has started the match and it is counting down, and how long is left. */
	bool IsCountingDown() const { return CountdownEndsAt > 0.0f; }
	float SecondsToStart() const { return IsCountingDown() ? FMath::Max(0.0f, CountdownEndsAt - static_cast<float>(GetServerWorldTimeSeconds())) : 0.0f; }
	/** Server only: when the countdown ends, in the server's time; 0 for none. */
	void SetCountdownEndsAt(float At) { CountdownEndsAt = At; }

	/** The bots' names the match drew, and how many bots fill in (-1 for every empty slot). */
	const TArray<FString>& GetBotNames() const { return BotNames; }
	int32 GetBotsWanted() const { return BotsWanted; }
	void SetBots(const TArray<FString>& Names, int32 Wanted) { BotNames = Names; BotsWanted = Wanted; }
	/** The players as roster entries: name, side, and whether a bot. */
	TArray<CheeseStrikeRules::FRosterEntry> Roster() const;
	/** Team `Team`'s column in the lobby: its players, then the bots that will fill in. */
	TArray<CheeseStrikeRules::FRosterEntry> TeamColumn(int32 Team) const;

	/** The scoreboard, in order. */
	TArray<CheeseStrikeRules::FStanding> Standings() const;
	/** The most kills anyone has. */
	int32 MostKills() const;

	/** The map the host picked in the lobby (CheeseStrikeRules::Maps), INDEX_NONE for this level as it is. */
	int32 GetMapIndex() const { return MapIndex; }
	void SetMapIndex(int32 Index) { MapIndex = Index; }

	/** The server's time the mice come (0 for none), for the countdown. */
	float GetMiceAt() const { return MiceAt; }
	void SetMiceAt(float At) { MiceAt = At; }

	/** The server's time everyone goes back to the lobby after the results; 0 before the match is over. */
	float GetLobbyAt() const { return LobbyAt; }
	void SetLobbyAt(float At) { LobbyAt = At; }

	/** How well the bots play (CheeseStrikeBots::Skills), as the host picked it. */
	int32 GetBotSkill() const { return BotSkill; }
	void SetBotSkill(int32 Skill) { BotSkill = Skill; }

	/**
	 * A match the lobby sent everyone to another level for waits, behind
	 * each player's loading cover, until they are all ready: how many it
	 * waits for (0 for a match that doesn't), and how many are ready.
	 */
	int32 GetPlayersExpected() const { return PlayersExpected; }
	int32 GetPlayersReady() const { return PlayersReady; }
	bool IsWaitingForPlayers() const { return PlayersExpected > 0 && !HasMatchStarted(); }
	/** Server only. */
	void SetPlayersReady(int32 Ready, int32 Expected) { PlayersReady = Ready; PlayersExpected = Expected; }

	/** The host's name, as the lobby crowns it; empty until the host is in. */
	const FString& GetHostName() const { return HostName; }
	void SetHostName(const FString& Name) { HostName = Name; }

	/** Server only. */
	void SetLimits(int32 InKillLimit, float InRespawnDelay);
	/** Server only: tells every machine at once, so that the feed is never behind. `How` names a streak that did it. */
	void AddKill(const FString& Killer, const FString& Victim, bool bHead, const FString& How = FString());
	void SetWinnerName(const FString& Name) { WinnerName = Name; }

	// The mode.

	/** The mode being played (CheeseStrikeModes), and what it is. */
	CheeseStrikeModes::EMode GetMode() const { return CheeseStrikeModes::ModeAt(Mode); }
	const CheeseStrikeModes::FModeInfo& GetModeInfo() const { return CheeseStrikeModes::Info(GetMode()); }
	/** Whether the mode is played in two teams: the lobby's two columns, team colours, team scores; otherwise everyone is on their own. */
	bool HasTeams() const { return GetModeInfo().bTeams; }
	/** Server only. */
	void SetMode(CheeseStrikeModes::EMode InMode) { Mode = static_cast<uint8>(InMode); }
	/** Whether two players are on the same side: in a team mode, the same team; otherwise only a player and themself. */
	bool AreTeammates(const APlayerState* A, const APlayerState* B) const;
	/** Whether two players are enemies: anyone else in free-for-all, the other team's players in a team mode. */
	bool AreEnemies(const APlayerState* A, const APlayerState* B) const;
	/** The team a player is on, or NoTeam for none (in free-for-all too). */
	int32 TeamOf(const APlayerState* Player) const;

	/** A team's score: its kills in Deathmatch, its rounds in Elimination; 0 in free-for-all. */
	int32 GetTeamScore(int32 Team) const { return TeamScores.IsValidIndex(Team) ? TeamScores[Team] : 0; }
	/** The team that won, once a team mode is over; NoTeam before, for a draw, or in free-for-all. */
	int32 GetWinningTeam() const { return WinningTeam; }
	/** Server only. */
	void AddTeamScore(int32 Team, int32 Points);
	void SetWinningTeam(int32 Team) { WinningTeam = Team; }

	/** In Gun Game, who is furthest up the ladder (most kills among equals); null with nobody. */
	const ACheeseStrikePlayerState* GunGameLeader() const;
	/** The name of the weapon of Gun Game's step `Level`. */
	static FString GunGameWeaponName(int32 Level);

	/** What the match plays to, in words ("first team to 50 kills"), for the scoreboard and the HUD. */
	FString GoalText() const { return CheeseStrikeModes::Goal(GetMode(), KillLimit, RoundLimit); }
	/** One line on how it stands, for the top of the HUD: the teams' scores in a team mode ("The Gouda Guys 12 · 9 Mozza Radicals"), with the round in Elimination and Execution; the leader's kills in free-for-all; the leader's weapon in Gun Game ("Ash on the Roquefort Rifle, 6 of 8"). */
	FString ScoreSummary() const;

	// Elimination's rounds.

	/** Rounds to win the match, and the round being played (1 on), 0 before the first. */
	int32 GetRoundLimit() const { return RoundLimit; }
	int32 GetRound() const { return Round; }
	/** Whether the match is between rounds: nobody can be hurt, and everyone comes back at the next. */
	bool IsRoundBreak() const { return RoundBreakUntil > 0.0f; }
	/** Seconds until the next round, during a break; 0 otherwise. */
	float SecondsToNextRound() const { return IsRoundBreak() ? FMath::Max(0.0f, RoundBreakUntil - static_cast<float>(GetServerWorldTimeSeconds())) : 0.0f; }
	/** Seconds left in the round being played; 0 between rounds and outside Elimination. */
	float SecondsLeftInRound() const { return RoundEndsAt > 0.0f && !IsRoundBreak() ? FMath::Max(0.0f, RoundEndsAt - static_cast<float>(GetServerWorldTimeSeconds())) : 0.0f; }
	/** Who took the last round: a team, NoTeam for a draw; -2 before any has ended. */
	int32 GetLastRoundWinner() const { return LastRoundWinner; }
	/** Server only. */
	void SetRoundLimit(int32 Limit) { RoundLimit = Limit; }
	void StartRound(int32 Number, float EndsAt);
	void EndRound(int32 Winner, float BreakUntil);

	// Streaks.

	/** Whether an Unmanned Aerial Cheese shows `Viewer` where the enemies are just now: their side's in a team mode, their own in free-for-all. */
	bool IsRevealedTo(const APlayerState* Viewer) const;
	/** The server's time that ends, or 0 if it isn't on. */
	float RevealEndsFor(const APlayerState* Viewer) const;
	/** Server only: the Unmanned Aerial Cheese `Caller` called in shows their side the enemy until `Until`. */
	void RevealFor(ACheeseStrikePlayerState* Caller, float Until);

	/** The last few announcements, as this machine heard them, oldest first. */
	const TArray<FAnnouncement>& GetAnnouncements() const { return Announcements; }
	/** Server only: tells every machine. */
	void Announce(const FString& Text, CheeseStrikeStreaks::EStreak Streak = CheeseStrikeStreaks::EStreak::None, int32 Team = CheeseStrikeModes::NoTeam, const FString& Who = FString());
	/** Seconds an announcement shows, and how many at most. */
	static constexpr float AnnouncementLife = 5.0f;
	static constexpr int32 AnnouncementsKept = 3;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastKill(const FString& Killer, const FString& Victim, bool bHead, const FString& How);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAnnounce(const FString& Text, uint8 Streak, int32 Team, const FString& Who);

	UPROPERTY(Replicated)
	int32 KillLimit = CheeseStrikeRules::KillLimit;
	UPROPERTY(Replicated)
	float RespawnDelay = CheeseStrikeRules::RespawnDelay;
	/** The last few kills, as each machine heard of them; a late joiner starts with none. */
	TArray<FKillFeedEntry> KillFeed;
	UPROPERTY(Replicated)
	FString WinnerName;
	UPROPERTY(Replicated)
	float CountdownEndsAt = 0.0f;
	UPROPERTY(Replicated)
	TArray<FString> BotNames;
	UPROPERTY(Replicated)
	int32 BotsWanted = -1;
	UPROPERTY(Replicated)
	FString HostName;
	UPROPERTY(Replicated)
	int32 MapIndex = INDEX_NONE;
	UPROPERTY(Replicated)
	int32 BotSkill = 1;
	UPROPERTY(Replicated)
	float LobbyAt = 0.0f;
	UPROPERTY(Replicated)
	float MiceAt = 0.0f;
	UPROPERTY(Replicated)
	int32 PlayersExpected = 0;
	UPROPERTY(Replicated)
	int32 PlayersReady = 0;

	/** CheeseStrikeModes::EMode. */
	UPROPERTY(Replicated)
	uint8 Mode = 0;
	UPROPERTY(Replicated)
	TArray<int32> TeamScores;
	UPROPERTY(Replicated)
	int32 WinningTeam = CheeseStrikeModes::NoTeam;
	UPROPERTY(Replicated)
	int32 RoundLimit = CheeseStrikeModes::RoundLimit;
	UPROPERTY(Replicated)
	int32 Round = 0;
	UPROPERTY(Replicated)
	float RoundEndsAt = 0.0f;
	UPROPERTY(Replicated)
	float RoundBreakUntil = 0.0f;
	UPROPERTY(Replicated)
	int32 LastRoundWinner = -2;
	/** Until when each team's Unmanned Aerial Cheese shows it the enemy, in the server's time. */
	UPROPERTY(Replicated)
	TArray<float> TeamRevealUntil;
	TArray<FAnnouncement> Announcements;
};
