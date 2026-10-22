// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLastStand.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

using CheeseStrikeModes::EInteract;

UCheeseStrikeLastStand::UCheeseStrikeLastStand()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCheeseStrikeLastStand::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCheeseStrikeLastStand, bDowned);
	DOREPLIFETIME(UCheeseStrikeLastStand, WorkingOn);
	DOREPLIFETIME(UCheeseStrikeLastStand, Work);
	DOREPLIFETIME(UCheeseStrikeLastStand, WorkStartedAt);
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::Body() const
{
	return Cast<ACheeseStrikeCharacter>(GetOwner());
}

float UCheeseStrikeLastStand::ServerNow() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

bool UCheeseStrikeLastStand::HasLastStand() const
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return Match != nullptr && Match->GetModeInfo().bLastStand;
}

float UCheeseStrikeLastStand::WorkDone() const
{
	const float Takes = CheeseStrikeModes::InteractTime(GetWork());
	return WorkingOn != nullptr && Takes > 0.0f ? FMath::Clamp((ServerNow() - WorkStartedAt) / Takes, 0.0f, 1.0f) : 0.0f;
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::WorkedOnBy() const
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || GetWorld() == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		const UCheeseStrikeLastStand* Theirs = It->GetLastStand();
		if (*It != Me && It->IsAlive() && Theirs != nullptr && Theirs->GetWorkingOn() == Me)
		{
			return *It;
		}
	}
	return nullptr;
}

ACheeseStrikeCharacter* UCheeseStrikeLastStand::InteractTarget(EInteract& OutKind) const
{
	OutKind = EInteract::None;
	const ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Me == nullptr || Match == nullptr || !Me->IsAlive() || bDowned || !Match->GetModeInfo().bLastStand)
	{
		return nullptr;
	}
	ACheeseStrikeCharacter* Best = nullptr;
	float Nearest = TNumericLimits<float>::Max();
	for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Me || !It->IsAlive() || !It->IsDowned())
		{
			continue;
		}
		const float Distance = FVector::Dist(Me->GetActorLocation(), It->GetActorLocation());
		const EInteract Kind = CheeseStrikeModes::InteractWith(true, Match->AreTeammates(Me->GetPlayerState(), It->GetPlayerState()), Distance);
		if (Kind != EInteract::None && Distance < Nearest)
		{
			Nearest = Distance;
			Best = *It;
			OutKind = Kind;
		}
	}
	return Best;
}

bool UCheeseStrikeLastStand::Intercept(float& Damage, const ACheeseStrikeCharacter* From, ACheeseStrikePlayerState* By, bool& bOutHurt)
{
	ACheeseStrikeCharacter* Me = Body();
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Me == nullptr || Match == nullptr || bFinishing || !Match->GetModeInfo().bLastStand)
	{
		return false;
	}
	// Only a hand up close finishes: a shot or a stroke from an enemy within reach, never a streak.
	const bool bEnemyClose = From != nullptr && From != Me && Match->AreEnemies(From->GetPlayerState(), Me->GetPlayerState())
		&& FVector::Dist(From->GetActorLocation(), Me->GetActorLocation()) <= CheeseStrikeModes::FinishRange;
	switch (CheeseStrikeModes::LastStandHit(true, bDowned, bUsed, Me->GetHealth(), Damage, bEnemyClose))
	{
	case CheeseStrikeModes::ELastStandHit::Finish:
		Damage = Me->GetHealth() + 1000.0f;
		return false;
	case CheeseStrikeModes::ELastStandHit::Absorbed:
		bOutHurt = false;
		return true;
	case CheeseStrikeModes::ELastStandHit::Down:
		GoDown(By != nullptr ? By : (From != nullptr ? From->GetPlayerState<ACheeseStrikePlayerState>() : nullptr));
		bOutHurt = true;
		return true;
	default:
		return false;
	}
}

void UCheeseStrikeLastStand::GoDown(ACheeseStrikePlayerState* By)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || bDowned)
	{
		return;
	}
	bDowned = true;
	bUsed = true;
	DownedBy = By;
	bInteractHeld = false;
	StopWork();
	// Only the sidearm, in hand at once.
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory())
	{
		Inventory->RestrictTo(EWeaponSlot::Sidearm);
	}
	KeepDown();
	// Falling back from whatever is just ahead, so that the legs lie out on the floor.
	Me->MakeRoomToLieDown();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is down, by %s"), *Me->DisplayName(), By != nullptr ? *By->GetPlayerName() : TEXT("nobody"));
	if (ACheeseStrikeGameMode* Mode = GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>())
	{
		Mode->Downed(Me, By);
	}
}

void UCheeseStrikeLastStand::Revive()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || !bDowned || !Me->IsAlive())
	{
		return;
	}
	bDowned = false;
	DownedBy = nullptr;
	Me->SetHealth(CheeseStrikeModes::ReviveHealth);
	if (UCheeseStrikeInventory* Inventory = Me->GetInventory())
	{
		Inventory->LiftRestriction();
	}
	OnRep_Downed();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is back up"), *Me->DisplayName());
}

void UCheeseStrikeLastStand::GiveUp()
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || !bDowned || !Me->IsAlive())
	{
		return;
	}
	ACheeseStrikePlayerState* By = DownedBy.Get();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s has nobody up to revive them: finished, as %s's kill"), *Me->DisplayName(), By != nullptr ? *By->GetPlayerName() : TEXT("nobody"));
	Finish(By);
}

void UCheeseStrikeLastStand::Finish(ACheeseStrikePlayerState* By)
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->HasAuthority() || !Me->IsAlive())
	{
		return;
	}
	// The blow lands as the finisher's own (or, with them gone, as a kill of theirs from nowhere).
	bFinishing = true;
	ACheeseStrikeCharacter* From = By != nullptr ? By->GetPawn<ACheeseStrikeCharacter>() : nullptr;
	if (From != nullptr && From->IsAlive())
	{
		Me->TakeShot(Me->GetHealth() + 1000.0f, From, false);
	}
	else if (By != nullptr)
	{
		Me->TakeBlast(Me->GetHealth() + 1000.0f, By, FString());
	}
	else
	{
		Me->TakeShot(Me->GetHealth() + 1000.0f, nullptr, false);
	}
	bFinishing = false;
}

void UCheeseStrikeLastStand::SetInteracting(bool bHeld)
{
	const ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return;
	}
	if (Me->HasAuthority())
	{
		ServerInteract_Implementation(bHeld);
	}
	else
	{
		ServerInteract(bHeld);
	}
}

void UCheeseStrikeLastStand::ServerInteract_Implementation(bool bHeld)
{
	bInteractHeld = bHeld;
	if (!bHeld)
	{
		StopWork();
	}
}

void UCheeseStrikeLastStand::StopWork()
{
	WorkingOn = nullptr;
	Work = static_cast<uint8>(EInteract::None);
	WorkStartedAt = 0.0f;
}

void UCheeseStrikeLastStand::DoWork(float DeltaTime)
{
	EInteract Kind = EInteract::None;
	ACheeseStrikeCharacter* Target = InteractTarget(Kind);
	if (Target == nullptr)
	{
		StopWork();
		return;
	}
	// A new one, or another way: from the start.
	if (Target != WorkingOn || static_cast<uint8>(Kind) != Work)
	{
		WorkingOn = Target;
		Work = static_cast<uint8>(Kind);
		WorkStartedAt = ServerNow();
		return;
	}
	if (ServerNow() - WorkStartedAt < CheeseStrikeModes::InteractTime(Kind))
	{
		return;
	}
	StopWork();
	bInteractHeld = false;
	UCheeseStrikeLastStand* Theirs = Target->GetLastStand();
	if (Theirs == nullptr)
	{
		return;
	}
	const ACheeseStrikeCharacter* Me = Body();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s %s %s"), *Me->DisplayName(), Kind == EInteract::Revive ? TEXT("revives") : TEXT("finishes"), *Target->DisplayName());
	if (Kind == EInteract::Revive)
	{
		Theirs->Revive();
	}
	else
	{
		Theirs->Finish(Me->GetPlayerState<ACheeseStrikePlayerState>());
	}
}

void UCheeseStrikeLastStand::KeepDown()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !bDowned || !Me->IsAlive())
	{
		return;
	}
	// The machine that moves it: its player's, or the server's for a bot. Crouching there reaches the server with the moves.
	if (!Me->IsLocallyControlled())
	{
		return;
	}
	// From the hip only.
	if (const ACheeseStrikeWeapon* Held = Me->GetWeapon(); Held != nullptr && Held->IsAimed())
	{
		Me->DoAimDownSights(false);
	}
	if (UCheeseStrikeMovementComponent* Movement = Me->GetCheeseStrikeMovement())
	{
		Movement->SetWantsToSprint(false);
		if (!Movement->bWantsToCrouch)
		{
			Me->Crouch();
		}
	}
}

void UCheeseStrikeLastStand::OnRep_Downed()
{
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr || !Me->IsLocallyControlled())
	{
		return;
	}
	if (bDowned)
	{
		KeepDown();
	}
	else
	{
		Me->UnCrouch();
	}
}

void UCheeseStrikeLastStand::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACheeseStrikeCharacter* Me = Body();
	if (Me == nullptr)
	{
		return;
	}
	KeepDown();
	if (!Me->HasAuthority())
	{
		return;
	}
	if (bDowned && !Me->IsAlive())
	{
		bDowned = false;
	}
	if (bInteractHeld && Me->CanAct() && !bDowned)
	{
		DoWork(DeltaTime);
	}
	else if (WorkingOn != nullptr)
	{
		StopWork();
	}
}
