// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeRules.h"
#include "Kismet/GameplayStatics.h"

namespace CheeseStrikeRules
{
	int32 FurthestFrom(TArrayView<const FVector> Spots, TArrayView<const FVector> Enemies)
	{
		int32 Best = Spots.Num() > 0 ? 0 : -1;
		double BestDistance = -1.0;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (const FVector& Enemy : Enemies)
			{
				Nearest = FMath::Min(Nearest, FVector::DistSquared(Spots[Index], Enemy));
			}
			if (Enemies.Num() > 0 && Nearest > BestDistance)
			{
				BestDistance = Nearest;
				Best = Index;
			}
		}
		return Best;
	}

	TArray<FStanding> Sorted(TArray<FStanding> Standings)
	{
		Standings.StableSort([](const FStanding& A, const FStanding& B)
		{
			if (A.Kills != B.Kills)
			{
				return A.Kills > B.Kills;
			}
			if (A.Deaths != B.Deaths)
			{
				return A.Deaths < B.Deaths;
			}
			return A.Order < B.Order;
		});
		return Standings;
	}

	bool IsOver(int32 MostKills, int32 Limit)
	{
		return Limit > 0 && MostKills >= Limit;
	}

	FString Leader(const TArray<FStanding>& Standings)
	{
		const TArray<FStanding> InOrder = Sorted(Standings);
		return InOrder.Num() > 0 ? InOrder[0].Name : FString();
	}

	float Option(const FString& Options, const TCHAR* Name, float Otherwise)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, Name);
		return Value.IsNumeric() ? FCString::Atof(*Value) : Otherwise;
	}

	int32 KillLimitFrom(const FString& Options)
	{
		return FMath::Clamp(FMath::RoundToInt(Option(Options, TEXT("KillLimit"), static_cast<float>(KillLimit))), 1, 1000);
	}

	float RespawnDelayFrom(const FString& Options)
	{
		return FMath::Clamp(Option(Options, TEXT("RespawnDelay"), RespawnDelay), 0.5f, 15.0f);
	}

	bool StartsAtOnce(const FString& Options)
	{
		return UGameplayStatics::HasOption(Options, TEXT("StartNow"));
	}
}
