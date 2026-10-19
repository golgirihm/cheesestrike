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

	/** The scoreboard, in order. */
	TArray<CheeseStrikeRules::FStanding> Standings() const;
	/** The most kills anyone has. */
	int32 MostKills() const;

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
};
