// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeModes.h"
#include "Kismet/GameplayStatics.h"
#include "CheeseStrikeRules.h"

namespace CheeseStrikeModes
{
	const FModeInfo Modes[NumModes] = {
		{EMode::FreeForAll, TEXT("Free-for-all"), TEXT("FreeForAll"), TEXT("Everyone for themself: the first to the kill limit wins."), false, EWinBy::PlayerKills, true, CheeseStrikeRules::KillLimit},
		{EMode::Deathmatch, TEXT("Deathmatch"), TEXT("Deathmatch"), TEXT("Two teams: the first team to the kill limit wins."), true, EWinBy::TeamKills, true, 50},
		{EMode::Elimination, TEXT("Elimination"), TEXT("Elimination"), TEXT("Two teams, one life a round: the side left standing takes the round."), true, EWinBy::Rounds, false, CheeseStrikeRules::KillLimit},
	};

	const FModeInfo& Info(EMode Mode)
	{
		return Modes[static_cast<int32>(ModeAt(static_cast<int32>(Mode)))];
	}

	EMode ModeAt(int32 Index)
	{
		return Index >= 0 && Index < NumModes ? static_cast<EMode>(Index) : EMode::FreeForAll;
	}

	EMode ModeFrom(const FString& Options)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("Mode"));
		if (Value.IsNumeric())
		{
			return ModeAt(FCString::Atoi(*Value));
		}
		for (const FModeInfo& Each : Modes)
		{
			if (Value.Equals(Each.Key, ESearchCase::IgnoreCase))
			{
				return Each.Mode;
			}
		}
		return EMode::FreeForAll;
	}

	int32 KillLimitFor(EMode Mode, const FString& Options)
	{
		const float Asked = CheeseStrikeRules::Option(Options, TEXT("KillLimit"), static_cast<float>(Info(Mode).DefaultKillLimit));
		return FMath::Clamp(FMath::RoundToInt(Asked), 1, 1000);
	}

	int32 RoundLimitFrom(const FString& Options)
	{
		return FMath::Clamp(FMath::RoundToInt(CheeseStrikeRules::Option(Options, TEXT("Rounds"), static_cast<float>(RoundLimit))), 1, 15);
	}

	float RoundTimeFrom(const FString& Options)
	{
		return FMath::Clamp(CheeseStrikeRules::Option(Options, TEXT("RoundTime"), RoundTime), 30.0f, 600.0f);
	}

	int32 TeamAtLimit(TArrayView<const int32> Scores, int32 Limit)
	{
		if (Limit <= 0)
		{
			return NoTeam;
		}
		for (int32 Team = 0; Team < Scores.Num(); ++Team)
		{
			if (Scores[Team] >= Limit)
			{
				return Team;
			}
		}
		return NoTeam;
	}

	bool RoundOver(const FSide& First, const FSide& Second, bool bTimeUp, int32& OutWinner)
	{
		OutWinner = NoTeam;
		const bool bBothHavePlayers = First.Members > 0 && Second.Members > 0;
		const bool bFirstGone = First.Alive <= 0;
		const bool bSecondGone = Second.Alive <= 0;
		if (bBothHavePlayers && (bFirstGone || bSecondGone))
		{
			OutWinner = bFirstGone && bSecondGone ? NoTeam : (bFirstGone ? 1 : 0);
			return true;
		}
		if (!bTimeUp)
		{
			return false;
		}
		if (First.Alive != Second.Alive)
		{
			OutWinner = First.Alive > Second.Alive ? 0 : 1;
		}
		else if (!FMath::IsNearlyEqual(First.Health, Second.Health, 0.5f))
		{
			OutWinner = First.Health > Second.Health ? 0 : 1;
		}
		return true;
	}

	bool CanHurt(bool bTeams, int32 AttackerTeam, int32 VictimTeam, bool bSelf, bool bPaused)
	{
		if (bPaused || bSelf)
		{
			return false;
		}
		return !bTeams || AttackerTeam != VictimTeam;
	}

	bool AreEnemies(bool bTeams, int32 TeamA, int32 TeamB, bool bSame)
	{
		if (bSame)
		{
			return false;
		}
		return !bTeams || TeamA != TeamB;
	}

	const TCHAR* const TeamStartTags[2] = {TEXT("Gouda"), TEXT("Mozza")};

	TArray<int32> SidesOf(TArrayView<const FVector> Spots)
	{
		TArray<int32> Sides;
		Sides.Init(0, Spots.Num());
		if (Spots.Num() < 2)
		{
			return Sides;
		}
		// The two furthest apart give the line the sides face each other along.
		int32 A = 0;
		int32 B = 1;
		double Widest = -1.0;
		for (int32 I = 0; I < Spots.Num(); ++I)
		{
			for (int32 J = I + 1; J < Spots.Num(); ++J)
			{
				const double Apart = FVector::DistSquared2D(Spots[I], Spots[J]);
				if (Apart > Widest)
				{
					Widest = Apart;
					A = I;
					B = J;
				}
			}
		}
		const FVector Along = (Spots[B] - Spots[A]).GetSafeNormal2D();
		TArray<int32> Order;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			Order.Add(Index);
		}
		Order.StableSort([&](int32 Left, int32 Right) { return FVector::DotProduct(Spots[Left] - Spots[A], Along) < FVector::DotProduct(Spots[Right] - Spots[A], Along); });
		// The nearer half to the first end is the first side's; an odd one out goes to the first side.
		const int32 FirstHalf = (Spots.Num() + 1) / 2;
		for (int32 Place = 0; Place < Order.Num(); ++Place)
		{
			Sides[Order[Place]] = Place < FirstHalf ? 0 : 1;
		}
		return Sides;
	}

	int32 ChooseStart(TArrayView<const FVector> Spots, TArrayView<const FVector> Enemies, TArrayView<const FVector> Others, float Room)
	{
		if (Spots.Num() == 0)
		{
			return -1;
		}
		TArray<FVector> Free;
		TArray<int32> FreeIndex;
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			bool bTaken = false;
			for (const FVector& Other : Others)
			{
				bTaken |= FVector::Dist2D(Spots[Index], Other) < Room && FMath::Abs(Spots[Index].Z - Other.Z) < 300.0f;
			}
			if (!bTaken)
			{
				Free.Add(Spots[Index]);
				FreeIndex.Add(Index);
			}
		}
		if (Free.Num() == 0)
		{
			return CheeseStrikeRules::FurthestFrom(Spots, Enemies);
		}
		return FreeIndex[CheeseStrikeRules::FurthestFrom(Free, Enemies)];
	}

	FLinearColor TeamColour(int32 Team)
	{
		switch (Team)
		{
		case 0:
			return FLinearColor(0.835f, 0.369f, 0.0f);
		case 1:
			return FLinearColor(0.337f, 0.706f, 0.914f);
		default:
			return FLinearColor(0.85f, 0.85f, 0.85f);
		}
	}

	FString Title(EMode Mode)
	{
		return FString(Info(Mode).Name).ToUpper();
	}

	FString Goal(EMode Mode, int32 KillLimit, int32 Rounds)
	{
		switch (Info(Mode).WinBy)
		{
		case EWinBy::TeamKills:
			return FString::Printf(TEXT("first team to %d kills"), KillLimit);
		case EWinBy::Rounds:
			return FString::Printf(TEXT("first to win %d round%s"), Rounds, Rounds == 1 ? TEXT("") : TEXT("s"));
		default:
			return FString::Printf(TEXT("first to %d kills"), KillLimit);
		}
	}
}
