// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeOverhead.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CheeseStrikeCharacter.h"
#include "Map/CheeseStrikeMouse.h"

FCheeseStrikeOverhead::FCheeseStrikeOverhead(UWorld* InWorld)
	: World(InWorld)
{
	if (World == nullptr)
	{
		return;
	}
	for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
	{
		const UStaticMeshComponent* Mesh = *It;
		if (Mesh->GetWorld() != World || !Mesh->IsRegistered() || Mesh->Mobility != EComponentMobility::Static || Mesh->bHiddenInGame
			|| Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Mesh->GetStaticMesh() == nullptr)
		{
			continue;
		}
		const FBox Box = Mesh->Bounds.GetBox();
		if (Box.GetSize().X >= MinAcross && Box.GetSize().Y >= MinAcross)
		{
			Ceilings.Add(Box);
		}
	}
}

const FCheeseStrikeOverhead& FCheeseStrikeOverhead::Of(UWorld* World)
{
	// One for each world in play (a test plays several in one process), forgotten with it.
	static TMap<TWeakObjectPtr<UWorld>, TSharedRef<FCheeseStrikeOverhead>> Found;
	for (auto It = Found.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
	if (const TSharedRef<FCheeseStrikeOverhead>* Known = Found.Find(World))
	{
		return Known->Get();
	}
	return Found.Add(World, MakeShared<FCheeseStrikeOverhead>(World)).Get();
}

TArray<const AActor*> FCheeseStrikeOverhead::Movers() const
{
	TArray<const AActor*> Found;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	for (TActorIterator<ACheeseStrikeMouse> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	return Found;
}

TOptional<float> FCheeseStrikeOverhead::RoofOver(const FVector& At, float Within) const
{
	TOptional<float> Roof;
	if (World == nullptr)
	{
		return Roof;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeRoof), false);
	Params.AddIgnoredActors(Movers());
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, At, At + FVector(0.0f, 0.0f, Within), ECC_Visibility, Params))
	{
		Roof = Hit.ImpactPoint.Z;
	}
	for (const FBox& Ceiling : Ceilings)
	{
		if (At.X >= Ceiling.Min.X && At.X <= Ceiling.Max.X && At.Y >= Ceiling.Min.Y && At.Y <= Ceiling.Max.Y
			&& Ceiling.Min.Z > At.Z && Ceiling.Min.Z <= At.Z + Within && (!Roof.IsSet() || Ceiling.Min.Z < Roof.GetValue()))
		{
			Roof = Ceiling.Min.Z;
		}
	}
	return Roof;
}

bool FCheeseStrikeOverhead::IsClear(const FVector& From, const FVector& To, const FVector& HalfSize) const
{
	if (World == nullptr)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeDroneWay), false);
	Params.AddIgnoredActors(Movers());
	FHitResult Hit;
	return !World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(HalfSize), Params);
}
