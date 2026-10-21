// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMice.h"
#include "Kismet/GameplayStatics.h"

namespace CheeseStrikeMice
{
	int32 MiceFor(int32 Blocks)
	{
		if (Blocks <= 0)
		{
			return 0;
		}
		return FMath::Clamp(FMath::RoundToInt(static_cast<float>(Blocks) / BlocksPerMouse), MinMice, MaxMice);
	}

	bool HasMice(const FString& Options)
	{
		return UGameplayStatics::ParseOption(Options, TEXT("Mice")) != TEXT("0");
	}

	float ArrivalFrom(const FString& Options)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("MiceAt"));
		return Value.IsNumeric() ? FMath::Max(0.0f, FCString::Atof(*Value)) : ArriveAfter;
	}

	float CheeseCost(float Distance, float Height, bool bClaimedByAnother)
	{
		return Distance + HeightCost * FMath::Max(0.0f, Height) + (bClaimedByAnother ? Claimed : 0.0f);
	}

	int32 Stage(float Health, float Full)
	{
		if (Full <= 0.0f || Health >= Full)
		{
			return 0;
		}
		// Bitten into at all: the first stage; two thirds gone, the third.
		const float Eaten = 1.0f - FMath::Max(0.0f, Health) / Full;
		return FMath::Clamp(1 + FMath::FloorToInt(Eaten * 3.0f), 1, 3);
	}

	float FearAfter(float Fear, float Seconds)
	{
		return FMath::Max(0.0f, Fear - FearFades * Seconds);
	}
}
