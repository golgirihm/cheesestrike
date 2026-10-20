// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "CheeseStrikeGameMode.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGameState;

/**
 * The game's rules, applied on the server: a free-for-all. Every player is
 * a CheeseStrikeCharacter driven by a CheeseStrikePlayerController. A kill
 * is scored to the killer (CheeseStrikePlayerState) and shown to everyone
 * (CheeseStrikeGameState's feed); the killed come back after the respawn
 * delay at the start furthest from everyone else; the match ends when
 * someone reaches the kill limit. The host sets the limit and the delay
 * on the map's URL (`?KillLimit=10?RespawnDelay=1`), and may change them
 * from the lobby; the template's own wandering enemies are kept off the
 * level.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ACheeseStrikeGameMode();

	int32 GetKillLimit() const { return KillLimit; }
	float GetRespawnDelay() const { return RespawnDelay; }

	/** `Victim` has been killed by `Killer` (null for nobody), on the head or not. */
	void Killed(ACheeseStrikeCharacter* Killer, ACheeseStrikeCharacter* Victim, bool bHead);

	/**
	 * Starts the match from the lobby: at once, or after the design's
	 * countdown so that everyone sees it coming. Nothing if the match isn't
	 * waiting or is counting down already. A match hosted with `?StartNow`
	 * on its URL never waits.
	 */
	void AskToStart(bool bAtOnce);
	/** Stops the countdown, back to waiting in the lobby. */
	void CancelStart();
	/**
	 * The map the match is to be played on (CheeseStrikeRules::Maps): the
	 * one it is on, or another the host picked in the lobby, which the
	 * match travels to when it starts, everyone with it.
	 */
	int32 GetMapIndex() const { return MapIndex; }
	/** The map this level is, or INDEX_NONE for one not in the list. */
	int32 CurrentMapIndex() const;
	/** Whether the match is waiting in the lobby for the host. */
	bool IsWaitingToStart() const;
	/** The host sets the rules from the lobby: on the match's state for everyone, and on the next match's URL. */
	void SetRules(int32 InKillLimit, float InRespawnDelay, int32 InMapIndex = INDEX_NONE);
	/** Back to the lobby after the results, with the rules as they are now. */
	virtual void RestartGame() override;

	virtual void HandleMatchHasStarted() override;
	/** The results stay up a while, then everyone is back in the lobby. */
	virtual void HandleMatchHasEnded() override;

	/** Where `Player` comes back: the start furthest from every living enemy. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	/** Every spawn chooses afresh, rather than reusing the first. */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	/** A newcomer joins the side with fewer; the host is named on the match's state. */
	virtual void PostLogin(APlayerController* NewPlayer) override;
	/** A match the lobby started on another map begins once everyone has arrived (or after a while), so that bots fill only the slots of nobody coming. */
	virtual bool ReadyToStartMatch_Implementation() override;
	/** How many bots fill in once the match starts: -1 for every empty slot. */
	int32 GetBotsWanted() const { return BotsWanted; }
	/** A player may name themself with `?Nick=` on the URL they join by (for development; Steam will name them). */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

protected:
	/** Gives `Who` a new character, if they and the match are still here. */
	void Respawn(TWeakObjectPtr<AController> Who);
	/** Fills the empty slots with bots, by the roster the lobby showed. */
	void SpawnBots();
	/** The countdown is over: the match starts here, or travels to the map the host picked and starts there. */
	void CountdownOver();
	/** The URL options that carry this match's rules to its next level. */
	FString RulesOptions() const;

	int32 KillLimit;
	float RespawnDelay;
	int32 BotsWanted = -1;
	int32 MapIndex = INDEX_NONE;
	int32 BotSeed = 0;
	/** How many players a match started by the lobby on another map waits for; 0 for a match not started that way. */
	int32 PlayersExpected = 0;
	FTimerHandle CountdownTimer;
	FTimerHandle ResultsTimer;
};
