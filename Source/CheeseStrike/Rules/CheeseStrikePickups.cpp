// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Rules/CheeseStrikePickups.h"
#include "Weapons/CheeseStrikeArsenal.h"

namespace
{
	/** The distance from `At` to the nearest of `Starts`, or a great one for none. */
	float PickupNearestStart(const FVector& At, const TArray<FVector>& Starts)
	{
		float Nearest = TNumericLimits<float>::Max();
		for (const FVector& Start : Starts)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist(At, Start));
		}
		return Nearest;
	}
}

namespace CheeseStrikePickups
{
	FName SpawnPrimary()
	{
		return CheeseStrikeArsenal::DefaultPrimary();
	}

	bool InMode(CheeseStrikeModes::EMode Mode)
	{
		return Mode != CheeseStrikeModes::EMode::GunGame;
	}

	bool CanLieOnAStand(FName Weapon)
	{
		const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
		return Stats != nullptr && Stats->Slot == EWeaponSlot::Primary && Stats->Id != SpawnPrimary();
	}

	bool CanBeDropped(FName Weapon)
	{
		const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
		return Stats != nullptr && Stats->Slot == EWeaponSlot::Primary;
	}

	ETake HowToTake(FName Carried, FName Offered, bool bRoundsFull, bool bDropped)
	{
		if (bDropped ? !CanBeDropped(Offered) : !CanLieOnAStand(Offered))
		{
			return ETake::None;
		}
		const FWeaponStats* Mine = CheeseStrikeArsenal::Find(Carried);
		if (Mine != nullptr && Mine->Slot == EWeaponSlot::Primary && Mine->Id == CheeseStrikeArsenal::Find(Offered)->Id)
		{
			return bRoundsFull ? ETake::None : ETake::TopUp;
		}
		return ETake::Swap;
	}

	int32 TopUpRounds(bool bDropped, int32 StandReserve, int32 DroppedMagazine, int32 DroppedReserve)
	{
		return bDropped ? FMath::Max(0, DroppedMagazine) + FMath::Max(0, DroppedReserve) : FMath::Max(0, StandReserve);
	}

	bool DroppedShown(float Age)
	{
		const float Left = DroppedSeconds - Age;
		if (Left > DroppedBlink)
		{
			return true;
		}
		return FMath::Frac(FMath::Max(0.0f, Left) * BlinksPerSecond) >= 0.35f;
	}

	int32 ToClearForDrop(int32 Lying)
	{
		return FMath::Max(0, Lying + 1 - MostDropped);
	}

	bool MayTake(bool bAlive, bool bCanAct, bool bDowned, bool bPrimaryAllowed)
	{
		return bAlive && bCanAct && !bDowned && bPrimaryAllowed;
	}

	bool InReach(const FVector& Stand, const FVector& Feet, float Across)
	{
		return FVector::DistSquared2D(Stand, Feet) <= FMath::Square(Across) && FMath::Abs(Feet.Z - Stand.Z) <= ReachUp;
	}

	int32 Worth(FName Weapon)
	{
		namespace W = CheeseStrikeWeapons;
		const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
		if (Stats == nullptr || Stats->Slot != EWeaponSlot::Primary || Stats->Id == SpawnPrimary())
		{
			return 0;
		}
		// Made for one range: close, for the Grater, the Shotgun and the Dual Burratas.
		if (Stats->Id == W::Grater().Id || Stats->Id == W::Shotgun().Id || Stats->Id == W::Burratas().Id)
		{
			return 1;
		}
		if (Stats->Id == W::Roquefort().Id)
		{
			return 3;
		}
		// Good at every range: the Blaster and the Munster.
		return 2;
	}

	FLinearColor WorthColour(int32 /*Worth*/)
	{
		// Every weapon is the one colour, the cheese's gold: what a gun is worth is not shown by a tint.
		return FLinearColor(0.98f, 0.76f, 0.22f);
	}

	float SecondsLeft(float Now, float BackAt)
	{
		return BackAt > 0.0f ? FMath::Max(0.0f, BackAt - Now) : 0.0f;
	}

	int32 ShownSeconds(float Left)
	{
		return Left > 0.0f ? FMath::CeilToInt(Left) : 0;
	}

	float BotWants(FName Carried, FName Offered, float Distance)
	{
		const int32 Gain = Worth(Offered) - Worth(Carried);
		if (!CanLieOnAStand(Offered) || Gain <= 0 || Distance > BotRange)
		{
			return 0.0f;
		}
		// Each step of worth is worth a walk; nearer is better.
		return static_cast<float>(Gain) * (1.0f - 0.7f * Distance / BotRange);
	}

	float Contest(const FVector& At, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza)
	{
		const float ToGouda = PickupNearestStart(At, Gouda);
		const float ToMozza = PickupNearestStart(At, Mozza);
		const float Mean = (ToGouda + ToMozza) / 2.0f;
		return Mean > KINDA_SMALL_NUMBER ? FMath::Min(ToGouda, ToMozza) / Mean : 1.0f;
	}

	bool AreFair(const TArray<FSpot>& Spots, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza, FString& Why, float Tolerance)
	{
		const auto Lean = [&Gouda, &Mozza](const FVector& At, float& ToGouda, float& ToMozza)
		{
			ToGouda = PickupNearestStart(At, Gouda);
			ToMozza = PickupNearestStart(At, Mozza);
			return ToGouda + ToMozza;
		};
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			float ToGouda, ToMozza;
			const float Both = Lean(Spots[Index].At, ToGouda, ToMozza);
			if (FMath::Abs(ToGouda - ToMozza) <= Tolerance * Both)
			{
				continue;
			}
			// Nearer one side: another of the same weapon as near the other.
			bool bMatched = false;
			for (int32 Other = 0; Other < Spots.Num() && !bMatched; ++Other)
			{
				float TheirGouda, TheirMozza;
				const float Theirs = Lean(Spots[Other].At, TheirGouda, TheirMozza);
				bMatched = Other != Index && Spots[Other].Weapon == Spots[Index].Weapon
					&& FMath::Abs(TheirGouda - ToMozza) <= Tolerance * (Both + Theirs) / 2.0f
					&& FMath::Abs(TheirMozza - ToGouda) <= Tolerance * (Both + Theirs) / 2.0f;
			}
			if (!bMatched)
			{
				Why = FString::Printf(TEXT("the %s at %s is %.0f m from the Gouda Guys and %.0f m from the Mozza Radicals, and no other is the other way round"),
					*Spots[Index].Weapon.ToString(), *Spots[Index].At.ToCompactString(), ToGouda / 100.0f, ToMozza / 100.0f);
				return false;
			}
		}
		return true;
	}

	bool BetterLieRiskier(const TArray<FSpot>& Spots, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza, FString& Why)
	{
		int32 Best = 0;
		int32 Least = TNumericLimits<int32>::Max();
		for (const FSpot& Spot : Spots)
		{
			Best = FMath::Max(Best, Worth(Spot.Weapon));
			Least = FMath::Min(Least, Worth(Spot.Weapon));
		}
		const auto MeanContest = [&](int32 Of)
		{
			float Sum = 0.0f;
			int32 Count = 0;
			for (const FSpot& Spot : Spots)
			{
				if (Worth(Spot.Weapon) == Of)
				{
					Sum += Contest(Spot.At, Gouda, Mozza);
					++Count;
				}
			}
			return Count > 0 ? Sum / Count : 0.0f;
		};
		if (Spots.Num() == 0 || Best == Least)
		{
			return true;
		}
		const float BestContest = MeanContest(Best);
		const float LeastContest = MeanContest(Least);
		if (BestContest + KINDA_SMALL_NUMBER < LeastContest)
		{
			Why = FString::Printf(TEXT("the best weapons (worth %d) are less contested (%.2f) than the least (worth %d, %.2f)"), Best, BestContest, Least, LeastContest);
			return false;
		}
		return true;
	}
}
