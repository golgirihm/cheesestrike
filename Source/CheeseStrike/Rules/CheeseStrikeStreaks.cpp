// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeStreaks.h"

namespace CheeseStrikeStreaks
{
	const FStreakInfo Streaks[NumStreaks] = {
		{EStreak::AerialCheese, 3, TEXT("Unmanned Aerial Cheese"), TEXT("an")},
		{EStreak::CheeseStrike, 5, TEXT("Cheese Strike"), TEXT("a")},
		{EStreak::GoudaOverload, 10, TEXT("Gouda Overload"), TEXT("the")},
	};

	namespace
	{
		const FStreakInfo NoStreak{EStreak::None, 0, TEXT(""), TEXT("")};
	}

	const FStreakInfo& Info(EStreak Kind)
	{
		for (const FStreakInfo& Each : Streaks)
		{
			if (Each.Kind == Kind)
			{
				return Each;
			}
		}
		return NoStreak;
	}

	EStreak StreakAt(int32 Index)
	{
		return Index >= 1 && Index <= NumStreaks ? static_cast<EStreak>(Index) : EStreak::None;
	}

	EStreak EarnedAt(int32 KillsInARow)
	{
		for (const FStreakInfo& Each : Streaks)
		{
			if (Each.Kills == KillsInARow)
			{
				return Each.Kind;
			}
		}
		return EStreak::None;
	}

	EStreak NextAfter(int32 KillsInARow)
	{
		for (const FStreakInfo& Each : Streaks)
		{
			if (Each.Kills > KillsInARow)
			{
				return Each.Kind;
			}
		}
		return EStreak::None;
	}

	bool IsStreakName(const FString& Name)
	{
		for (const FStreakInfo& Each : Streaks)
		{
			if (Name.Equals(Each.Name))
			{
				return true;
			}
		}
		return false;
	}

	FString CalledIn(const FString& Who, EStreak Kind)
	{
		const FStreakInfo& Streak = Info(Kind);
		return FString::Printf(TEXT("%s called in %s %s"), *Who, Streak.Article, Streak.Name);
	}

	EStreak FRun::AddKill(bool bCounts)
	{
		if (!bCounts)
		{
			return EStreak::None;
		}
		++Kills;
		const EStreak Earned = EarnedAt(Kills);
		if (Earned != EStreak::None)
		{
			Ready.Add(Earned);
		}
		return Earned;
	}

	bool FRun::Take(EStreak Kind)
	{
		if (Kind == EStreak::None || Next() != Kind)
		{
			return false;
		}
		Ready.RemoveAt(0);
		return true;
	}

	FReveal AddReveal(const FReveal& Running, float Now, float Seconds)
	{
		if (Running.Until > Now)
		{
			return {Running.From, Running.Until + Seconds};
		}
		return {Now, Now + Seconds};
	}

	TArray<FVector> StrikeLine(const FVector& Spot, float Yaw)
	{
		const FVector Along = FRotator(0.0f, Yaw, 0.0f).Vector();
		TArray<FVector> Line;
		for (int32 Index = 0; Index < StrikeWheels; ++Index)
		{
			const float Offset = (Index - (StrikeWheels - 1) / 2.0f) * StrikeSpacing;
			Line.Add(Spot + Along * Offset);
		}
		return Line;
	}

	float BlastDamage(const FBlast& Blast, float Distance)
	{
		if (Distance > Blast.Radius || Blast.Radius <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::Lerp(Blast.Centre, Blast.Edge, FMath::Max(0.0f, Distance) / Blast.Radius);
	}

	float FallTime(float Height, float Speed)
	{
		// Height = v t + g t² / 2, for t.
		return (-Speed + FMath::Sqrt(Speed * Speed + 2.0f * DropGravity * FMath::Max(0.0f, Height))) / DropGravity;
	}

	float HeightBefore(float Left, float Height, float Speed)
	{
		// Run backwards from landing: it was going v + g T there, and slower above.
		const float Total = FallTime(Height, Speed);
		const float Since = FMath::Clamp(Total - Left, 0.0f, Total);
		return FMath::Max(0.0f, Height - (Speed * Since + 0.5f * DropGravity * Since * Since));
	}

	float DroneHeight(float Roof, float LowestStart, float HighestStart, float& OutScale)
	{
		if (Roof <= 0.0f)
		{
			OutScale = 1.0f;
			return HighestStart + DroneAltitude;
		}
		OutScale = Roof - LowestStart < DroneLowRoof ? DroneIndoorScale : 1.0f;
		return FMath::Max(Roof - DroneUnderRoof * OutScale, HighestStart + DroneAboveStarts * OutScale);
	}

	float DroneRoof(TArray<float> Roofs)
	{
		if (Roofs.Num() == 0)
		{
			return 0.0f;
		}
		Roofs.Sort();
		return Roofs[Roofs.Num() / 2];
	}

	int32 ChooseDroneWay(TConstArrayView<FDroneWay> Ways)
	{
		int32 Best = INDEX_NONE;
		for (int32 Index = 0; Index < Ways.Num(); ++Index)
		{
			if (Best == INDEX_NONE || Ways[Index].Blocked < Ways[Best].Blocked)
			{
				Best = Index;
			}
		}
		return Best;
	}
}
