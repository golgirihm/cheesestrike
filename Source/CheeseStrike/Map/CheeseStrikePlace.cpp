// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePlace.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ACheeseStrikePlace::ACheeseStrikePlace()
{
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	RootComponent = Area;
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetCanEverAffectNavigation(false);
	Area->SetMobility(EComponentMobility::Static);
	Area->InitBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	SetActorHiddenInGame(true);
}

FBox ACheeseStrikePlace::GetArea() const
{
	const FVector Extent = Area->GetScaledBoxExtent();
	const FVector Middle = Area->GetComponentLocation();
	return FBox(Middle - Extent, Middle + Extent);
}

FString ACheeseStrikePlace::NameAt(const UWorld* World, const FVector& Where)
{
	FString Found;
	double Smallest = TNumericLimits<double>::Max();
	if (World != nullptr)
	{
		for (TActorIterator<ACheeseStrikePlace> It(World); It; ++It)
		{
			const FBox Box = It->GetArea();
			if (Box.IsInsideOrOn(Where) && Box.GetVolume() < Smallest)
			{
				Smallest = Box.GetVolume();
				Found = It->PlaceName;
			}
		}
	}
	return Found;
}
