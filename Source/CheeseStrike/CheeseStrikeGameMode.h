// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Rules/CheeseStrikeBots.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "CheeseStrikeGameMode.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGameState;
class ACheeseStrikeHill;
class ACheeseStrikePlayerState;

/**
 * The game's rules, applied on the server, in the mode the host picked
 * (CheeseStrikeModes; `?Mode=` on the map's URL): free-for-all, Deathmatch
 * (two teams to a kill limit) or Elimination (two teams, one life a round,
 * first to the round limit). Every player is a CheeseStrikeCharacter driven
 * by a CheeseStrikePlayerController. A kill is scored to the killer
 * (CheeseStrikePlayerState), and to their team in Deathmatch, and shown to
 * everyone (CheeseStrikeGameState's feed); the killed come back after the
 * respawn delay at the start furthest from their enemies (in a team mode,
 * at one of their side's), except in Elimination, where they are out until
 * the next round; the match ends when someone, or a team, reaches the
 * limit. Friendly fire is off in a team mode. Kills in a row earn streaks
 * (CheeseStrikeStreaks), called in through CallStreak. The host sets the
 * limits on the URL (`?KillLimit=10?RespawnDelay=1?Rounds=4?RoundTime=120`),
 * and may change them from the lobby; the template's own wandering enemies
 * are kept off the level.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ACheeseStrikeGameMode();

	int32 GetKillLimit() const { return KillLimit; }
	float GetRespawnDelay() const { return RespawnDelay; }

	/** `Victim` has been killed by `Killer` (null for nobody), on the head or not; or by `By`'s streak, named `How`, whether or not they have a character now. */
	void Killed(ACheeseStrikeCharacter* Killer, ACheeseStrikeCharacter* Victim, bool bHead, ACheeseStrikePlayerState* By = nullptr, const FString& How = FString());

	/** The mode being played, and the host changing it from the lobby (on the match's state for everyone, and on the next match's URL). */
	CheeseStrikeModes::EMode GetMode() const { return Mode; }
	void SetMode(CheeseStrikeModes::EMode InMode);
	/** The host sets the rounds to win, from the lobby. */
	void SetRoundLimit(int32 Rounds);
	/** Execution: `Victim` has gone down, put there by `By`; down still counts as in, until executed (UCheeseStrikeLastStand). */
	void Downed(ACheeseStrikeCharacter* Victim, ACheeseStrikePlayerState* By);
	/** Gun Game: a character is given its player's step of the ladder (and nothing else), as it comes in. */
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	/** Elimination's rounds to win, and a round's length. */
	int32 GetRoundLimit() const { return RoundLimit; }
	float GetRoundTime() const { return RoundTime; }
	/** Whether `Attacker`'s shots or streaks may hurt `Victim` (`bSelf` for their own): never a teammate in a team mode, nobody between rounds; anything with no attacker may. */
	bool CanHurt(const ACheeseStrikePlayerState* Attacker, const ACheeseStrikePlayerState* Victim, bool bSelf) const;
	/**
	 * `Caller` calls in `Kind`, their next streak: the Unmanned Aerial
	 * Cheese, the Cheese Strike on `Spot` along `Yaw`, or the Gouda
	 * Overload. False, and nothing happens, unless it is their next, they
	 * are alive in a match that is on, and (for a strike) the spot is in
	 * range.
	 */
	bool CallStreak(ACheeseStrikePlayerState* Caller, CheeseStrikeStreaks::EStreak Kind, const FVector& Spot, float Yaw);
	/** From the host's console or a play script: gives a streak (1 the Unmanned Aerial Cheese, 2 the Cheese Strike, 3 the Gouda Overload) to the host, to the player or bot named, or to an enemy of the host's ("enemy"). */
	UFUNCTION(Exec)
	void CheeseStrikeGiveStreak(int32 Kind, const FString& Player = TEXT(""));
	/** From the host's console or a play script, in a mode with a last stand (not in shipping builds): puts down the host, the player or bot named, or the first of the host's enemies or teammates ("enemy", "mate"), as if hurt enough by nobody. */
	UFUNCTION(Exec)
	void CheeseStrikeDown(const FString& Player = TEXT(""));
	/** The same, for looking at getting up: revives everyone down, or the player or bot named. */
	UFUNCTION(Exec)
	void CheeseStrikeRevive(const FString& Player = TEXT(""));
	/** The same, for looking at an execution: the host (or the player or bot named) executes the nearest enemy down, wherever they are, by `Kind` (1 the stab, 2 the stomp, 0 either). */
	UFUNCTION(Exec)
	void CheeseStrikeExecute(int32 Kind = 0, const FString& Player = TEXT(""));
	/** King of the Hill: the hill has made `Team`'s points, or in the solo mode `Player`'s, reach the target: they win. */
	void HillWon(int32 Team, ACheeseStrikePlayerState* Player);
	/** King of the Hill's hill (ACheeseStrikeHill), once the match has begun; null in the other modes. */
	ACheeseStrikeHill* GetHill() const { return Hill.Get(); }
	/**
	 * From the host's console or a play script (not in shipping builds): ends the match now, won by whoever leads
	 * (the side ahead, or the player first as the podium orders them), for looking at the results and the podium.
	 */
	UFUNCTION(Exec)
	void CheeseStrikeEndMatch();
	/** From the host's console or a play script (not in shipping builds): moves the hill to its next spot at once. */
	UFUNCTION(Exec)
	void CheeseStrikeMoveHill();
	/** For looking at the hill (development): puts the host's view `Distance` cm from it, `Yaw` degrees round from the west and `Pitch` down, looking at it; CheeseStrikeOutside with no distance puts it back. */
	UFUNCTION(Exec)
	void CheeseStrikeViewHill(float Distance = 1600.0f, float Yaw = 225.0f, float Pitch = 40.0f);
	/** In Elimination a newcomer comes in only at a round's start; later, they are out until the next. */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	/** Someone leaving may end a round of Elimination. */
	virtual void Logout(AController* Exiting) override;

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
	 * Why the match can't be started from the lobby now (CheeseStrikeRules::
	 * WhyNotStart: one alone, or a side with nobody), or nothing. The
	 * countdown asks it, and so does its end; starting at once (a script's,
	 * a test's) doesn't.
	 */
	FString WhyNotStart() const;
	/** Whether the host may still change the lobby's rules and seats: waiting, and not yet counting down to the start. */
	bool CanChangeLobby() const;
	/**
	 * In a mode of sides, in the lobby: `Player` goes over to the other side,
	 * if it has fewer than a side's seats of players (a bot makes way for
	 * them). False if they can't.
	 */
	bool SwitchSide(APlayerController* Player);
	/** The host moves the lobby's bot `Bot` (its number, CheeseStrikeRules::FRosterEntry::Bot) to the other side, if it has an open seat. */
	bool MoveBot(int32 Bot);
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
	/** The host, over the podium: back to the lobby in a moment (CheeseStrikePodiums::SkipSeconds, for every machine to keep the podium up), not at the end of its countdown. */
	void BackToLobbyNow();

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
	/** How well the bots play (CheeseStrikeBots::Skills), as the host picks it in the lobby (`?BotSkill=`). */
	int32 GetBotSkill() const { return BotSkill; }
	/** The host sets how many bots fill in, -1 for every empty seat. */
	void SetBotsWanted(int32 Wanted);
	void SetBotSkill(int32 Skill);
	/** A player may name themself with `?Nick=` on the URL they join by (for development; Steam will name them). */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

protected:
	/** Gives `Who` a new character, if they and the match are still here. */
	void Respawn(TWeakObjectPtr<AController> Who);
	/** Fills the empty slots with bots, by the roster the lobby showed. */
	void SpawnBots();
	/** The mice come over the edge of the map: one for every so many blocks of cheese, at its MouseEntry points (CheeseStrikeMice). */
	void SpawnMice();
	/** The countdown is over: the match starts here, or travels to the map the host picked and starts there. */
	void CountdownOver();
	/** The countdown's own end: called off if the match can no longer be played (someone left), else CountdownOver. */
	void CountdownEnded();
	/** The URL options that carry this match's rules to its next level. */
	FString RulesOptions() const;
	/** What ServerTravel is given for `Url`: its map and options, without its host and port. */
	static FString TravelTo(const FURL& Url);
	/** The match is over: `Team` has won it. */
	void EndMatchFor(int32 Team);
	/** Elimination: a round begins, everyone back at their side's starts with full health (the first comes with the match). */
	void StartRound(bool bFirst);
	void NextRound() { StartRound(false); }
	/**
	 * Elimination: ends the round if a side is gone, or whatever stands when
	 * `bTimeUp`. In Execution a side whose players left in are all down is
	 * gone too: each of them is finished first, as the kill of whoever put them down.
	 */
	void CheckRound(bool bTimeUp);
	void RoundTimeUp() { CheckRound(true); }
	void CheckRoundNow() { CheckRound(false); }
	/** Elimination: `Winner` (or nobody, NoTeam) takes the round; the match, at the round limit, or a pause before the next. */
	void EndRound(int32 Winner);
	/** Gun Game: gives `Body` its player's step of the ladder, in hand at once, and nothing else. */
	void ArmForGunGame(TWeakObjectPtr<ACheeseStrikeCharacter> Body);

	int32 KillLimit;
	float RespawnDelay;
	CheeseStrikeModes::EMode Mode = CheeseStrikeModes::EMode::FreeForAll;
	int32 RoundLimit = CheeseStrikeModes::RoundLimit;
	float RoundTime = CheeseStrikeModes::RoundTime;
	/** The world's time the round being played began. */
	float RoundStartedAt = 0.0f;
	FTimerHandle RoundTimer;
	int32 BotsWanted = -1;
	/** The sides the host moved the lobby's bots to (CheeseStrikeRules::Seats), carried to the match's levels (`?BotSides=`). */
	TArray<int32> BotSides;
	int32 MapIndex = INDEX_NONE;
	int32 BotSeed = 0;
	int32 BotSkill = CheeseStrikeBots::DefaultSkill;
	/** Whether this match has mice (`?Mice=0` for none), and how long into it they come (`?MiceAt=`). */
	bool bMice = true;
	float MiceAfter = 60.0f;
	FTimerHandle MiceTimer;
	/** Seconds counted before the match begins (`?Countdown=`; CheeseStrikeStart), and so before each later round. */
	float StartCountdown = 3.0f;
	/** When the mice came (the world's time), how many blocks they came to, and the log of how they are getting on (every ten seconds, until all is eaten). */
	float MiceCameAt = -1.0f;
	int32 MiceCameTo = 0;
	FTimerHandle MiceProgressTimer;
	void LogMiceProgress();
	/** How many players a match started by the lobby on another map waits for; 0 for a match not started that way. */
	int32 PlayersExpected = 0;
	FTimerHandle CountdownTimer;
	FTimerHandle ResultsTimer;
	/** King of the Hill: the hill, put up as the match begins, its first minute counted from the end of the count. */
	void StartHill(float CountLeft);
	TWeakObjectPtr<ACheeseStrikeHill> Hill;
};
