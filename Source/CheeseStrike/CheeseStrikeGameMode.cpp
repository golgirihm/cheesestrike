// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeHUD.h"

ACheeseStrikeGameMode::ACheeseStrikeGameMode()
{
	DefaultPawnClass = ACheeseStrikeCharacter::StaticClass();
	PlayerControllerClass = ACheeseStrikePlayerController::StaticClass();
	PlayerStateClass = ACheeseStrikePlayerState::StaticClass();
	GameStateClass = ACheeseStrikeGameState::StaticClass();
	HUDClass = ACheeseStrikeHUD::StaticClass();
	KillLimit = CheeseStrikeRules::KillLimit;
	RespawnDelay = CheeseStrikeRules::RespawnDelay;
}

void ACheeseStrikeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	KillLimit = CheeseStrikeRules::KillLimitFrom(Options);
	RespawnDelay = CheeseStrikeRules::RespawnDelayFrom(Options);
	// The match waits in the lobby for the host to start it, unless told not to.
	bDelayedStart = !CheeseStrikeRules::StartsAtOnce(Options);
	UE_LOG(LogCheeseStrike, Display, TEXT("Free-for-all to %d kills, back %.1f s after a death%s"), KillLimit, RespawnDelay, bDelayedStart ? TEXT(", waiting in the lobby") : TEXT(""));
}

bool ACheeseStrikeGameMode::IsWaitingToStart() const
{
	return GetMatchState() == MatchState::WaitingToStart;
}

void ACheeseStrikeGameMode::AskToStart(bool bAtOnce)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (!IsWaitingToStart() || State == nullptr || State->IsCountingDown())
	{
		return;
	}
	if (bAtOnce)
	{
		StartMatch();
		return;
	}
	State->SetCountdownEndsAt(State->GetServerWorldTimeSeconds() + CheeseStrikeRules::MatchCountdown);
	UE_LOG(LogCheeseStrike, Display, TEXT("The match starts in %.0f s"), CheeseStrikeRules::MatchCountdown);
	GetWorldTimerManager().SetTimer(CountdownTimer, this, &AGameMode::StartMatch, CheeseStrikeRules::MatchCountdown, false);
}

void ACheeseStrikeGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetCountdownEndsAt(0.0f);
	}
}

void ACheeseStrikeGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	// The results stay up a while; then the level is played again from the lobby, the session kept.
	GetWorldTimerManager().SetTimer(ResultsTimer, this, &AGameMode::RestartGame, CheeseStrikeRules::ResultsTime, false);
}

FString ACheeseStrikeGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	// The engine names a player after their machine (or, later, their Steam
	// account) through the URL's own Name option; this one is theirs to pick.
	const FString Nick = UGameplayStatics::ParseOption(Options, TEXT("Nick")).Left(20);
	if (!Nick.IsEmpty() && NewPlayerController != nullptr)
	{
		ChangeName(NewPlayerController, Nick, false);
	}
	return Error;
}

void ACheeseStrikeGameMode::InitGameState()
{
	Super::InitGameState();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetLimits(KillLimit, RespawnDelay);
	}
}

void ACheeseStrikeGameMode::Killed(ACheeseStrikeCharacter* Killer, ACheeseStrikeCharacter* Victim, bool bHead)
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (Victim == nullptr || State == nullptr)
	{
		return;
	}
	ACheeseStrikePlayerState* KillerState = Killer != nullptr ? Killer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	ACheeseStrikePlayerState* VictimState = Victim->GetPlayerState<ACheeseStrikePlayerState>();
	if (KillerState != nullptr && KillerState != VictimState)
	{
		KillerState->AddKill();
	}
	if (VictimState != nullptr)
	{
		VictimState->AddDeath();
	}
	State->AddKill(Killer != nullptr ? Killer->DisplayName() : FString(), Victim->DisplayName(), bHead);
	UE_LOG(LogCheeseStrike, Display, TEXT("Kill: %s killed %s%s; %d of %d"), Killer != nullptr ? *Killer->DisplayName() : TEXT("nobody"), *Victim->DisplayName(), bHead ? TEXT(" on the head") : TEXT(""), State->MostKills(), KillLimit);

	if (IsMatchInProgress() && CheeseStrikeRules::IsOver(State->MostKills(), KillLimit))
	{
		State->SetWinnerName(CheeseStrikeRules::Leader(State->Standings()));
		UE_LOG(LogCheeseStrike, Display, TEXT("Match over: %s wins"), *State->GetWinnerName());
		EndMatch();
		return;
	}
	if (AController* Player = Victim->GetController())
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ACheeseStrikeGameMode::Respawn, TWeakObjectPtr<AController>(Player)), RespawnDelay, false);
	}
}

void ACheeseStrikeGameMode::Respawn(TWeakObjectPtr<AController> Who)
{
	AController* Player = Who.Get();
	if (Player == nullptr || !IsMatchInProgress())
	{
		return;
	}
	APawn* Body = Player->GetPawn();
	Player->UnPossess();
	RestartPlayer(Player);
	if (Body != nullptr)
	{
		// The body lies there a while longer.
		Body->SetLifeSpan(5.0f);
	}
}

AActor* ACheeseStrikeGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	TArray<APlayerStart*> Starts;
	TArray<FVector> Spots;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Starts.Add(*It);
		Spots.Add(It->GetActorLocation());
	}
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	TArray<FVector> Enemies;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		if (It->IsAlive() && It->GetController() != Player)
		{
			Enemies.Add(It->GetActorLocation());
		}
	}
	return Starts[CheeseStrikeRules::FurthestFrom(Spots, Enemies)];
}
