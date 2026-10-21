// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCheese.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"

namespace
{
	constexpr float Gravity = 980.0f;
	/** How near a block's bottom is to another's top for it to rest on it (cm). */
	constexpr float Resting = 8.0f;
}

ACheeseStrikeCheese::ACheeseStrikeCheese()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	NetDormancy = DORM_Initial;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cheese(TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(Cube.Object);
	if (Cheese.Object != nullptr)
	{
		Mesh->SetMaterial(0, Cheese.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// Movable: what rests on eaten cheese falls.
	Mesh->SetMobility(EComponentMobility::Movable);
	Health = CheeseStrikeRules::CheeseBlockHealth;
}

void ACheeseStrikeCheese::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeCheese, Stage);
}

void ACheeseStrikeCheese::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ShowStage();
}

void ACheeseStrikeCheese::OnRep_Stage()
{
	ShowStage();
}

void ACheeseStrikeCheese::ShowStage()
{
	// The cheese material's holes grow with it (tools/art/cheese_material.py).
	if (Mesh != nullptr)
	{
		Mesh->SetCustomPrimitiveDataFloat(0, Stage);
	}
}

void ACheeseStrikeCheese::Bite(float Amount)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || Health <= 0.0f)
	{
		return;
	}
	Health = FMath::Max(0.0f, Health - Amount);
	const uint8 Now = static_cast<uint8>(CheeseStrikeMice::Stage(Health, CheeseStrikeRules::CheeseBlockHealth));
	if (Now != Stage)
	{
		Stage = Now;
		ShowStage();
		FlushNetDormancy();
	}
	if (Health > 0.0f)
	{
		return;
	}
	// Gone: what rested on it (its bottom at this one's top, over it) falls.
	const FBox Gone = GetComponentsBoundingBox();
	TArray<ACheeseStrikeCheese*> Above;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->Health <= 0.0f)
		{
			continue;
		}
		const FBox Box = It->GetComponentsBoundingBox();
		const bool bOver = Box.Min.X < Gone.Max.X - 2.0f && Box.Max.X > Gone.Min.X + 2.0f && Box.Min.Y < Gone.Max.Y - 2.0f && Box.Max.Y > Gone.Min.Y + 2.0f;
		if (bOver && FMath::Abs(Box.Min.Z - Gone.Max.Z) < Resting)
		{
			Above.Add(*It);
		}
	}
	Destroy();
	for (ACheeseStrikeCheese* Each : Above)
	{
		Each->Fall();
	}
}

void ACheeseStrikeCheese::Fall()
{
	if (bFalling)
	{
		return;
	}
	bFalling = true;
	FallSpeed = 0.0f;
	SetActorTickEnabled(true);
	FlushNetDormancy();
}

void ACheeseStrikeCheese::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bFalling)
	{
		return;
	}
	FlushNetDormancy();
	FallSpeed += Gravity * DeltaSeconds;
	const float Drop = FallSpeed * DeltaSeconds;
	// Down until the level or other cheese holds it; people and mice don't.
	const FBox Box = GetComponentsBoundingBox();
	const FVector Middle = Box.GetCenter();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseFall), false, this);
	// What it already touches beside it doesn't hold it up (the Wheel Stack's rows lean on each other).
	Params.bFindInitialOverlaps = false;
	FHitResult Hit;
	// What holds it is what is under its middle: a wheel whose own is eaten falls, whatever is beside it.
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(Box.GetExtent().X * 0.3f, Box.GetExtent().Y * 0.3f, Box.GetExtent().Z - 2.0f));
	if (GetWorld()->SweepSingleByObjectType(Hit, Middle, Middle - FVector(0.0f, 0.0f, Drop), FQuat::Identity, Objects, Shape, Params))
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, -Hit.Distance));
		bFalling = false;
		FallSpeed = 0.0f;
		SetActorTickEnabled(false);
		return;
	}
	AddActorWorldOffset(FVector(0.0f, 0.0f, -Drop));
	// Nothing under it at all (off the map): gone.
	if (GetActorLocation().Z < -5000.0f)
	{
		Destroy();
	}
}
