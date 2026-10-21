// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Rules/CheeseStrikeRules.h"
#include "CheeseStrikeGameState.generated.h"

class ACheeseStrikePlayerState;

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
	/** The server's time it happened. */
	UPROPERTY()
	float At = 0.0f;
};

/**
 * What everyone knows about the match: whether it is on or over (the
 * engine's match state), the kill limit and respawn delay the host set, the
 * last few kills for the feed, and who won.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeGameState : public AGameState
{
	GENERATED_BODY()

public:
	int32 GetKillLimit() const { return KillLimit; }
	float GetRespawnDelay() const { return RespawnDelay; }
	const TArray<FKillFeedEntry>& GetKillFeed() const { return KillFeed; }
	/** Who won, once the match is over; empty before. */
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
	/** Server only: tells every machine at once, so that the feed is never behind. */
	void AddKill(const FString& Killer, const FString& Victim, bool bHead);
	void SetWinnerName(const FString& Name) { WinnerName = Name; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastKill(const FString& Killer, const FString& Victim, bool bHead);

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
};
