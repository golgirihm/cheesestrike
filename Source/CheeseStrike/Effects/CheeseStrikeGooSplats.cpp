// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGooSplats.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CheeseStrikeCharacter.h"
#include "Rules/CheeseStrikeGoo.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Weapons/CheeseStrikeSmokeCloud.h"

namespace
{
	/** The cheese blood's splat (tools/art/cheese_blood.py), coloured as melted gouda: a golden orange, its thick middle paler. */
	const TCHAR* const GooDecalLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseBloodDecal.M_CheeseBloodDecal");
	const FLinearColor GooDeep(0.86f, 0.34f, 0.012f, 1.0f);
	const FLinearColor GooPale(1.0f, 0.6f, 0.07f, 1.0f);
	/** Glossier than a wound's: wet, molten. */
	constexpr float GooRoughness = 0.1f;
	/** How deep a puddle projects, through the floor it lies on (enough for a slope or a step's edge). */
	constexpr float GooDepth = 30.0f;
	/** The globs (tools/art/goo_blobs.py), one of them at random for each puddle. */
	const TCHAR* const GooBlobPaths[] = {
		TEXT("/Game/CheeseStrike/Effects/SM_GooBlob1.SM_GooBlob1"),
		TEXT("/Game/CheeseStrike/Effects/SM_GooBlob2.SM_GooBlob2"),
		TEXT("/Game/CheeseStrike/Effects/SM_GooBlob3.SM_GooBlob3"),
	};
}

ACheeseStrikeGooSplats::ACheeseStrikeGooSplats()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetReplicates(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ACheeseStrikeGooSplats* ACheeseStrikeGooSplats::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ACheeseStrikeGooSplats> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

ACheeseStrikeGooSplats* ACheeseStrikeGooSplats::For(UWorld* World)
{
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return nullptr;
	}
	if (ACheeseStrikeGooSplats* Found = Find(World))
	{
		return Found;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.ObjectFlags |= RF_Transient;
	return World->SpawnActor<ACheeseStrikeGooSplats>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
}

int32 ACheeseStrikeGooSplats::CountNear(const FVector& At, float Reach) const
{
	int32 Count = 0;
	for (const FPuddle& Each : Puddles)
	{
		Count += Each.Decal != nullptr && FVector::Dist2D(Each.Decal->GetComponentLocation(), At) <= Reach ? 1 : 0;
	}
	return Count;
}

int32 ACheeseStrikeGooSplats::CountBlobsNear(const FVector& At, float Reach, float* OutTallest) const
{
	int32 Count = 0;
	float Tallest = 0.0f;
	for (const FPuddle& Each : Puddles)
	{
		if (Each.Blob != nullptr && FVector::Dist2D(Each.Blob->GetComponentLocation(), At) <= Reach)
		{
			++Count;
			Tallest = FMath::Max(Tallest, static_cast<float>(Each.Blob->Bounds.BoxExtent.Z) * 2.0f);
		}
	}
	if (OutTallest != nullptr)
	{
		*OutTallest = Tallest;
	}
	return Count;
}

void ACheeseStrikeGooSplats::ShapeBlob(UStaticMeshComponent* Blob, float Radius, float Since)
{
	// As wide as the puddle, as tall as the goo is thick for it: a heap as it lands, slumping, then melting flat.
	const float Wide = Radius / CheeseStrikeGoo::BlobRadius;
	const float Tall = FMath::Max(0.01f, Wide * CheeseStrikeGoo::BlobThick * CheeseStrikeGoo::BlobHeightAt(Since));
	Blob->SetRelativeScale3D(FVector(Wide, Wide, Tall));
}

void ACheeseStrikeGooSplats::Clear(FPuddle& Puddle)
{
	if (Puddle.Decal != nullptr)
	{
		Puddle.Decal->DestroyComponent();
	}
	if (Puddle.Blob != nullptr)
	{
		Puddle.Blob->DestroyComponent();
	}
}

bool ACheeseStrikeGooSplats::Lay(ACheeseStrikeCharacter* Who, float Full, float Now)
{
	UWorld* World = GetWorld();
	if (!bLookLoaded)
	{
		bLookLoaded = true;
		Look = LoadObject<UMaterialInterface>(nullptr, GooDecalLookPath);
		for (const TCHAR* Path : GooBlobPaths)
		{
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
			{
				Blobs.Add(Mesh);
			}
		}
	}
	if (World == nullptr || Look == nullptr)
	{
		return false;
	}
	// The floor under its feet: none while it is in the air.
	const float Half = Who->GetCapsuleComponent() != nullptr ? Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector From = Who->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeGoo), false);
	Params.AddIgnoredActor(Who);
	ACheeseStrikeSmokeCloud::IgnoreClouds(Params, World);
	FHitResult Floor;
	if (!World->LineTraceSingleByObjectType(Floor, From, From - FVector(0.0f, 0.0f, Half + CheeseStrikeGoo::FloorReach), FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic)), Params))
	{
		return false;
	}
	// Past the cap, the oldest goes.
	while (Puddles.Num() >= CheeseStrikeGoo::MostPuddles)
	{
		Clear(Puddles[0]);
		Puddles.RemoveAt(0);
	}
	UDecalComponent* Decal = NewObject<UDecalComponent>(this);
	// Projected into the floor, turned at random about it.
	FRotator Facing = (-Floor.ImpactNormal).Rotation();
	Facing.Roll = FMath::FRandRange(0.0f, 360.0f);
	Decal->SetWorldLocationAndRotation(Floor.ImpactPoint, Facing);
	const float Radius = CheeseStrikeGoo::RadiusAt(Full, 0.0f);
	Decal->DecalSize = FVector(GooDepth, Radius * CheeseStrikeGoo::FilmShare, Radius * CheeseStrikeGoo::FilmShare);
	Decal->SetDecalMaterial(Look);
	// Under the cheese a stomp or a wound leaves on it.
	Decal->SortOrder = -2;
	Decal->RegisterComponent();
	if (UMaterialInstanceDynamic* Goo = Decal->CreateDynamicMaterialInstance())
	{
		Goo->SetVectorParameterValue(TEXT("Colour"), GooDeep);
		Goo->SetVectorParameterValue(TEXT("Pale"), GooPale);
		Goo->SetScalarParameterValue(TEXT("Roughness"), GooRoughness);
		Goo->SetScalarParameterValue(TEXT("Seed"), FMath::FRandRange(0.0f, 100.0f));
		Goo->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	}
	// The glob itself, lying on the floor in the film, turned at random about it; seen, lit and shadowed, never hit.
	UStaticMeshComponent* Blob = nullptr;
	if (Blobs.Num() > 0)
	{
		Blob = NewObject<UStaticMeshComponent>(this);
		Blob->SetStaticMesh(Blobs[FMath::RandHelper(Blobs.Num())]);
		Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blob->SetCanEverAffectNavigation(false);
		Blob->SetGenerateOverlapEvents(false);
		Blob->bReceivesDecals = false;
		const FQuat OnFloor = FQuat::FindBetweenNormals(FVector::UpVector, Floor.ImpactNormal) * FQuat(FVector::UpVector, FMath::FRandRange(0.0f, 2.0f * PI));
		Blob->SetWorldLocationAndRotation(Floor.ImpactPoint, OnFloor);
		ShapeBlob(Blob, Radius, 0.0f);
		Blob->RegisterComponent();
	}
	FPuddle Made;
	Made.Decal = Decal;
	Made.Blob = Blob;
	Made.Born = Now;
	Made.Full = Full;
	Made.Radius = Radius;
	Puddles.Add(Made);
	++Laid;
	return true;
}

void ACheeseStrikeGooSplats::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const AGameStateBase* State = World->GetGameState();
	const float ServerNow = State != nullptr ? static_cast<float>(State->GetServerWorldTimeSeconds()) : Now;

	// Every character the gouda slows here leaves puddles where it goes: a big one as it lands, then a fresh one every
	// few steps, or every second or so where it stands, smaller as the goo thins.
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		ACheeseStrikeCharacter* Who = *It;
		if (!IsValid(Who) || !Who->IsAlive() || Who->GooFactorNow() >= 1.0f)
		{
			continue;
		}
		FTrail& Trail = Trails.FindOrAdd(Who);
		if (Trail.SplatAt != Who->GetSplatAt())
		{
			// Just splatted (or splatted again): the big one.
			if (Lay(Who, CheeseStrikeGoo::FirstRadius, Now))
			{
				Trail.SplatAt = Who->GetSplatAt();
				Trail.LastAt = Who->GetActorLocation();
				Trail.LastTime = Now;
			}
			continue;
		}
		if (CheeseStrikeGoo::PuddleDue(FVector::Dist2D(Who->GetActorLocation(), Trail.LastAt), Now - Trail.LastTime))
		{
			const float Wash = CheeseStrikeStreaks::GooWash(ServerNow, Who->GetSplatAt(), Who->GetGooUntil());
			if (Lay(Who, CheeseStrikeGoo::RadiusFor(Wash), Now))
			{
				Trail.LastAt = Who->GetActorLocation();
				Trail.LastTime = Now;
			}
		}
	}
	for (auto Each = Trails.CreateIterator(); Each; ++Each)
	{
		if (!Each.Key().IsValid())
		{
			Each.RemoveCurrent();
		}
	}

	// Each spreads, stays and fades in its time.
	for (int32 Index = Puddles.Num() - 1; Index >= 0; --Index)
	{
		FPuddle& Puddle = Puddles[Index];
		const float Since = Now - Puddle.Born;
		if (Puddle.Decal == nullptr || CheeseStrikeGoo::IsGone(Since))
		{
			Clear(Puddle);
			Puddles.RemoveAt(Index);
			continue;
		}
		const float Radius = CheeseStrikeGoo::RadiusAt(Puddle.Full, Since);
		if (!FMath::IsNearlyEqual(Radius, Puddle.Radius, 0.25f))
		{
			Puddle.Radius = Radius;
			Puddle.Decal->DecalSize = FVector(GooDepth, Radius * CheeseStrikeGoo::FilmShare, Radius * CheeseStrikeGoo::FilmShare);
			Puddle.Decal->MarkRenderStateDirty();
		}
		if (Puddle.Blob != nullptr && (Since < CheeseStrikeGoo::PuddleGrowSeconds + 0.1f || Since > CheeseStrikeGoo::PuddleStays))
		{
			ShapeBlob(Puddle.Blob, Radius, Since);
		}
		const float Opacity = CheeseStrikeGoo::OpacityAt(Since);
		if (!FMath::IsNearlyEqual(Opacity, Puddle.Opacity, 0.01f))
		{
			Puddle.Opacity = Opacity;
			if (UMaterialInstanceDynamic* Goo = Cast<UMaterialInstanceDynamic>(Puddle.Decal->GetDecalMaterial()))
			{
				Goo->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			}
		}
	}
}
