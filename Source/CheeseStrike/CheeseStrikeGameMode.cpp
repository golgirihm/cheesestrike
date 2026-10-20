// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "CheeseStrike.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeHUD.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "Variant_Shooter/AI/ShooterNPCSpawner.h"

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
	BotsWanted = CheeseStrikeRules::BotsFrom(Options);
	UE_LOG(LogCheeseStrike, Display, TEXT("Free-for-all to %d kills, back %.1f s after a death%s, %s bots"), KillLimit, RespawnDelay, bDelayedStart ? TEXT(", waiting in the lobby") : TEXT(""), BotsWanted < 0 ? TEXT("filling in with") : *FString::FromInt(BotsWanted));
	// The template's level comes with spawners for its own enemies, who would wander about the menu's view and the match: none of them.
	for (TActorIterator<AShooterNPCSpawner> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}

void ACheeseStrikeGameMode::SetRules(int32 InKillLimit, float InRespawnDelay)
{
	KillLimit = InKillLimit;
	RespawnDelay = InRespawnDelay;
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetLimits(KillLimit, RespawnDelay);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Rules: to %d kills, back %.1f s after a death"), KillLimit, RespawnDelay);
}

void ACheeseStrikeGameMode::RestartGame()
{
	if (GameSession == nullptr || !GameSession->CanRestartGame() || GetMatchState() == MatchState::LeavingMap || GetWorld() == nullptr)
	{
		return;
	}
	// The engine's restart would travel by the URL the match was hosted with; this one carries the rules as the host left them.
	FURL Url = GetWorld()->URL;
	Url.RemoveOption(TEXT("KillLimit"));
	Url.RemoveOption(TEXT("RespawnDelay"));
	Url.AddOption(*FString::Printf(TEXT("KillLimit=%d"), KillLimit));
	Url.AddOption(*FString::Printf(TEXT("RespawnDelay=%.1f"), RespawnDelay));
	GetWorld()->ServerTravel(Url.ToString(), GetTravelType());
}

void ACheeseStrikeGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ACheeseStrikePlayerState* State = NewPlayer != nullptr ? NewPlayer->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	const ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
	if (State == nullptr || Match == nullptr)
	{
		return;
	}
	int32 Counts[CheeseStrikeRules::Teams] = {};
	for (const APlayerState* Each : Match->PlayerArray)
	{
		const ACheeseStrikePlayerState* Other = Cast<ACheeseStrikePlayerState>(Each);
		if (Other != nullptr && Other != State && Other->GetTeam() >= 0 && Other->GetTeam() < CheeseStrikeRules::Teams)
		{
			++Counts[Other->GetTeam()];
		}
	}
	State->SetTeam(CheeseStrikeRules::TeamToJoin(Counts));
	// The host is the player at the server's own keyboard; the lobby crowns them.
	if (NewPlayer->IsLocalController())
	{
		if (ACheeseStrikeGameState* Ours = GetGameState<ACheeseStrikeGameState>())
		{
			Ours->SetHostName(State->GetPlayerName());
		}
	}
}

void ACheeseStrikeGameMode::SpawnBots()
{
	ACheeseStrikeGameState* Match = GetGameState<ACheeseStrikeGameState>();
	UWorld* World = GetWorld();
	if (Match == nullptr || World == nullptr)
	{
		return;
	}
	for (int32 Team = 0; Team < CheeseStrikeRules::Teams; ++Team)
	{
		for (const CheeseStrikeRules::FRosterEntry& Entry : Match->TeamColumn(Team))
		{
			if (!Entry.bBot)
			{
				continue;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACheeseStrikeBotController* Bot = World->SpawnActor<ACheeseStrikeBotController>(ACheeseStrikeBotController::StaticClass(), FTransform::Identity, Parameters);
			if (Bot == nullptr)
			{
				continue;
			}
			if (ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				State->SetPlayerName(Entry.Name);
				State->SetIsABot(true);
				State->SetTeam(Team);
			}
			RestartPlayer(Bot);
		}
	}
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

void ACheeseStrikeGameMode::CancelStart()
{
	ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>();
	if (!IsWaitingToStart() || State == nullptr || !State->IsCountingDown())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	State->SetCountdownEndsAt(0.0f);
	UE_LOG(LogCheeseStrike, Display, TEXT("The start is called off"));
}

void ACheeseStrikeGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	if (ACheeseStrikeGameState* State = GetGameState<ACheeseStrikeGameState>())
	{
		State->SetCountdownEndsAt(0.0f);
	}
	SpawnBots();
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
		// The bots' names are drawn once, so that the lobby and the match agree on them.
		State->SetBots(CheeseStrikeRules::BotNames(FMath::Rand(), CheeseStrikeRules::Teams * CheeseStrikeRules::TeamSize), BotsWanted);
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
