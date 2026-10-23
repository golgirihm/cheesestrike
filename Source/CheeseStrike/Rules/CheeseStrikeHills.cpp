// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHills.h"

namespace CheeseStrikeHills
{
	FHold HoldOf(bool bTeams, TArrayView<const FInHill> Inside)
	{
		FHold Hold;
		for (const FInHill& Each : Inside)
		{
			const int32 Who = bTeams ? Each.Team : Each.Player;
			if (Hold.State == EHold::Neutral)
			{
				Hold.State = EHold::Held;
				Hold.Holder = Who;
			}
			else if (Hold.Holder != Who)
			{
				Hold.State = EHold::Contested;
				Hold.Holder = INDEX_NONE;
				break;
			}
		}
		return Hold;
	}

	bool IsInside(const FVector& Feet, const FVector& Spot, float HillRadius)
	{
		return FVector::DistSquared2D(Feet, Spot) <= FMath::Square(HillRadius) && Feet.Z >= Spot.Z - Below && Feet.Z <= Spot.Z + Above;
	}

	int32 FClock::Advance(float Seconds, const FHold& Hold)
	{
		if (Hold.State != EHold::Held || Last.State != EHold::Held || Last.Holder != Hold.Holder)
		{
			Held = 0.0f;
		}
		Last = Hold;
		if (Hold.State != EHold::Held)
		{
			return 0;
		}
		Held += FMath::Max(0.0f, Seconds);
		const int32 Whole = FMath::FloorToInt(Held);
		Held -= Whole;
		return Whole * PointsPerSecond;
	}

	int32 NextSpot(int32 Current, int32 Count)
	{
		return Count > 0 ? (FMath::Max(0, Current) + 1) % Count : 0;
	}

	bool IsWarning(float SecondsLeft)
	{
		return SecondsLeft > 0.0f && SecondsLeft <= MoveWarning;
	}

	TArray<int32> AwayFromHill(TArrayView<const FVector> Spots, const FVector& Hill, float Clear, bool bNext, const FVector& Next)
	{
		TArray<int32> Away;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			if (FVector::Dist2D(Spots[Index], Hill) >= Clear && (!bNext || FVector::Dist2D(Spots[Index], Next) >= Clear))
			{
				Away.Add(Index);
			}
		}
		if (Away.Num() == 0)
		{
			for (int32 Index = 0; Index < Spots.Num(); ++Index)
			{
				Away.Add(Index);
			}
		}
		return Away;
	}

	EBotPlan BotPlan(bool bInHill, bool bSeesEnemy, float EnemyDistance, bool bWarning)
	{
		if (bInHill)
		{
			return EBotPlan::HoldHill;
		}
		if (bSeesEnemy && EnemyDistance < BotFightRange)
		{
			return EBotPlan::Fight;
		}
		return bWarning ? EBotPlan::GoToNext : EBotPlan::GoToHill;
	}
}
