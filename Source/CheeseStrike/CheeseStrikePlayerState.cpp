// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "Modes/CheeseStrikeWatchCamera.h"

void ACheeseStrikePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikePlayerState, Kills);
	DOREPLIFETIME(ACheeseStrikePlayerState, Deaths);
	DOREPLIFETIME(ACheeseStrikePlayerState, Team);
	DOREPLIFETIME(ACheeseStrikePlayerState, JoinOrder);
	DOREPLIFETIME(ACheeseStrikePlayerState, GunLevel);
	DOREPLIFETIME(ACheeseStrikePlayerState, StreakKills);
	DOREPLIFETIME(ACheeseStrikePlayerState, ReadyStreaks);
	DOREPLIFETIME(ACheeseStrikePlayerState, LastEarned);
	DOREPLIFETIME(ACheeseStrikePlayerState, LastEarnedAt);
	DOREPLIFETIME(ACheeseStrikePlayerState, bOut);
	DOREPLIFETIME(ACheeseStrikePlayerState, RevealUntil);
	DOREPLIFETIME(ACheeseStrikePlayerState, RevealFrom);
	DOREPLIFETIME(ACheeseStrikePlayerState, HillPoints);
	DOREPLIFETIME(ACheeseStrikePlayerState, HillSeconds);
}

void ACheeseStrikePlayerState::AddHillTime(float Seconds)
{
	HillTime += FMath::Max(0.0f, Seconds);
	HillSeconds = FMath::FloorToInt(HillTime);
}

CheeseStrikeRules::FStanding ACheeseStrikePlayerState::Standing() const
{
	CheeseStrikeRules::FStanding Line;
	Line.Name = GetPlayerName();
	Line.Kills = Kills;
	Line.Deaths = Deaths;
	Line.Order = GetPlayerId();
	return Line;
}

CheeseStrikeStreaks::EStreak ACheeseStrikePlayerState::AddKill(bool bCountsForStreak)
{
	++Kills;
	const CheeseStrikeStreaks::EStreak Earned = Run.AddKill(bCountsForStreak);
	if (Earned != CheeseStrikeStreaks::EStreak::None)
	{
		LastEarned = static_cast<uint8>(Earned);
		const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
		LastEarnedAt = State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0f;
	}
	PublishRun();
	return Earned;
}

void ACheeseStrikePlayerState::AddDeath()
{
	++Deaths;
	Run.Died();
	PublishRun();
}

void ACheeseStrikePlayerState::GiveStreak(CheeseStrikeStreaks::EStreak Kind)
{
	if (Kind == CheeseStrikeStreaks::EStreak::None)
	{
		return;
	}
	Run.Ready.Add(Kind);
	LastEarned = static_cast<uint8>(Kind);
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	LastEarnedAt = State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0f;
	PublishRun();
}

bool ACheeseStrikePlayerState::TakeStreak(CheeseStrikeStreaks::EStreak Kind)
{
	const bool bTaken = Run.Take(Kind);
	PublishRun();
	return bTaken;
}

void ACheeseStrikePlayerState::PublishRun()
{
	StreakKills = Run.Kills;
	ReadyStreaks.Reset();
	for (const CheeseStrikeStreaks::EStreak Each : Run.Ready)
	{
		ReadyStreaks.Add(static_cast<uint8>(Each));
	}
}

TArray<CheeseStrikeStreaks::EStreak> ACheeseStrikePlayerState::GetReadyStreaks() const
{
	TArray<CheeseStrikeStreaks::EStreak> Ready;
	for (const uint8 Each : ReadyStreaks)
	{
		Ready.Add(CheeseStrikeStreaks::StreakAt(Each));
	}
	return Ready;
}

CheeseStrikeStreaks::EStreak ACheeseStrikePlayerState::NextStreak() const
{
	return ReadyStreaks.Num() > 0 ? CheeseStrikeStreaks::StreakAt(ReadyStreaks[0]) : CheeseStrikeStreaks::EStreak::None;
}

void ACheeseStrikePlayerState::SetOut(bool bInOut)
{
	bOut = bInOut;
	// The host's own player hears of it here; everyone else's through OnRep_Out.
	const APlayerController* Player = GetPlayerController();
	if (Player != nullptr && Player->IsLocalController())
	{
		ShowOut();
	}
}

void ACheeseStrikePlayerState::OnRep_Out()
{
	ShowOut();
}

void ACheeseStrikePlayerState::ShowOut()
{
	APlayerController* Player = GetPlayerController();
	if (Player == nullptr || !Player->IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	if (bOut && Watching == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.ObjectFlags |= RF_Transient;
		Watching = GetWorld()->SpawnActor<ACheeseStrikeWatchCamera>(ACheeseStrikeWatchCamera::StaticClass(), Player->PlayerCameraManager != nullptr ? Player->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
		if (Watching != nullptr)
		{
			Watching->Watch(Player);
		}
	}
	else if (!bOut && Watching != nullptr)
	{
		Watching->Stop();
		Watching = nullptr;
	}
}

void ACheeseStrikePlayerState::Destroyed()
{
	if (Watching != nullptr)
	{
		Watching->Destroy();
		Watching = nullptr;
	}
	Super::Destroyed();
}

void ACheeseStrikePlayerState::StreakPressed()
{
	if (bMarkingStrike)
	{
		CancelStrike();
		return;
	}
	const CheeseStrikeStreaks::EStreak Next = NextStreak();
	if (Next == CheeseStrikeStreaks::EStreak::None)
	{
		return;
	}
	if (Next == CheeseStrikeStreaks::EStreak::CheeseStrike)
	{
		// First, where: the next shot marks it.
		bMarkingStrike = true;
		if (ACheeseStrikeCharacter* Body = GetPawn<ACheeseStrikeCharacter>())
		{
			Body->DoFire(false);
		}
		return;
	}
	ServerCallStreak(static_cast<uint8>(Next), FVector::ZeroVector, 0.0f);
}

bool ACheeseStrikePlayerState::StrikeAim(FVector& OutSpot, float& OutYaw) const
{
	const APlayerController* Player = GetPlayerController();
	UWorld* World = GetWorld();
	if (Player == nullptr || World == nullptr)
	{
		return false;
	}
	FVector From;
	FRotator Way;
	Player->GetPlayerViewPoint(From, Way);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeStrikeAim), false);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, From, From + Way.Vector() * CheeseStrikeStreaks::StrikeRange, ECC_Visibility, Params))
	{
		return false;
	}
	OutSpot = Hit.ImpactPoint;
	OutYaw = Way.Yaw;
	return true;
}

void ACheeseStrikePlayerState::MarkStrike()
{
	FVector Spot;
	float Yaw = 0.0f;
	if (!bMarkingStrike || !StrikeAim(Spot, Yaw))
	{
		return;
	}
	bMarkingStrike = false;
	ServerCallStreak(static_cast<uint8>(CheeseStrikeStreaks::EStreak::CheeseStrike), Spot, Yaw);
}

void ACheeseStrikePlayerState::CancelStrike()
{
	bMarkingStrike = false;
}

void ACheeseStrikePlayerState::ServerCallStreak_Implementation(uint8 Kind, FVector_NetQuantize Spot, float Yaw)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->CallStreak(this, CheeseStrikeStreaks::StreakAt(Kind), Spot, Yaw);
	}
}
