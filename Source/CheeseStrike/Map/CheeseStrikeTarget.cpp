// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "Rules/CheeseStrikeRange.h"

ACheeseStrikeTarget::ACheeseStrikeTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Wheel(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cheese(TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese"));
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetStaticMesh(Wheel.Object);
	if (Cheese.Object != nullptr)
	{
		Mesh->SetMaterial(0, Cheese.Object);
	}
	// On its edge, its face toward the actor's forward (the cylinder stands along its own Z).
	Mesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCanEverAffectNavigation(false);
	Health = CheeseStrikeRange::TargetHealth;
}

void ACheeseStrikeTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeTarget, Stage);
	DOREPLIFETIME(ACheeseStrikeTarget, BackAt);
}

void ACheeseStrikeTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Mesh->SetRelativeScale3D(FullScale());
}

void ACheeseStrikeTarget::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ShowLook();
}

float ACheeseStrikeTarget::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
	return State != nullptr ? static_cast<float>(State->GetServerWorldTimeSeconds()) : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
}

float ACheeseStrikeTarget::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || IsBroken() || Damage <= 0.0f)
	{
		return Taken;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	if (Health <= 0.0f)
	{
		BackAt = ServerNow() + CheeseStrikeRange::RegrowSeconds;
		UE_LOG(LogCheeseStrike, Verbose, TEXT("Target: %s broken, back in %.0f s"), *GetName(), CheeseStrikeRange::RegrowSeconds);
	}
	Stage = static_cast<uint8>(CheeseStrikeRange::TargetStage(Health));
	ShowLook();
	ForceNetUpdate();
	return Damage;
}

void ACheeseStrikeTarget::OnRep_Look()
{
	ShowLook();
}

void ACheeseStrikeTarget::ShowLook()
{
	if (Mesh == nullptr)
	{
		return;
	}
	// The cheese material's holes grow with it, as a block's do under the mice's bites (tools/art/cheese_material.py).
	Mesh->SetCustomPrimitiveDataFloat(0, Stage);
	const bool bBroken = IsBroken();
	if (bBroken != bShownBroken)
	{
		bShownBroken = bBroken;
		SetActorHiddenInGame(bBroken);
		SetActorEnableCollision(!bBroken);
		// Back: it grows from a seed, on every machine.
		GrowingSince = bBroken ? -1.0f : ServerNow();
		if (!bBroken)
		{
			Mesh->SetRelativeScale3D(FullScale() * CheeseStrikeRange::GrownShare(0.0f));
		}
	}
	// Broken, it waits (on the server, to grow back); growing back, it grows.
	SetActorTickEnabled(bBroken || GrowingSince >= 0.0f);
}

FVector ACheeseStrikeTarget::FullScale() const
{
	return FVector(Diameter / 100.0f, Diameter / 100.0f, Thickness / 100.0f);
}

void ACheeseStrikeTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = ServerNow();
	if (HasAuthority() && IsBroken() && Now >= BackAt)
	{
		Health = CheeseStrikeRange::TargetHealth;
		Stage = 0;
		BackAt = 0.0f;
		ForceNetUpdate();
		ShowLook();
	}
	if (IsBroken() || GrowingSince < 0.0f)
	{
		return;
	}
	const float Share = CheeseStrikeRange::GrownShare(Now - GrowingSince);
	Mesh->SetRelativeScale3D(FullScale() * Share);
	if (Share >= 1.0f)
	{
		GrowingSince = -1.0f;
		SetActorTickEnabled(false);
	}
}
