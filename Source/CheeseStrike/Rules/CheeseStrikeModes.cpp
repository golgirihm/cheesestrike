// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeModes.h"
#include "Kismet/GameplayStatics.h"
#include "CheeseStrikeHills.h"
#include "CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeArsenal.h"

namespace CheeseStrikeModes
{
	const FModeInfo Modes[NumModes] = {
		{EMode::FreeForAll, TEXT("Free-for-all"), TEXT("FreeForAll"), TEXT("Everyone for themself: the first to the kill limit wins."), false, EWinBy::PlayerKills, true, CheeseStrikeRules::KillLimit, false, true},
		{EMode::Deathmatch, TEXT("Deathmatch"), TEXT("Deathmatch"), TEXT("Two teams: the first team to the kill limit wins."), true, EWinBy::TeamKills, true, 50, false, true},
		{EMode::Elimination, TEXT("Elimination"), TEXT("Elimination"), TEXT("Two teams, one life a round: the side left standing takes the round."), true, EWinBy::Rounds, false, 0, false, true},
		{EMode::Execution, TEXT("Execution"), TEXT("Execution"), TEXT("Two teams, one life a round; hurt enough, you go down: executed up close, revived, or up again in 20 s."), true, EWinBy::Rounds, false, 0, true, true},
		{EMode::GunGame, TEXT("Gun Game"), TEXT("GunGame"), TEXT("Everyone for themself: each kill moves you to the next weapon; a kill with the last, the knife, wins."), false, EWinBy::Ladder, true, 0, false, false},
		{EMode::KingOfTheHill, TEXT("King of the Hill"), TEXT("KingOfTheHill"), TEXT("Two teams: a point a second while only your side stands in the hill, which moves every minute."), true, EWinBy::Hill, true, CheeseStrikeHills::TeamTarget, false, true},
		{EMode::SoloKingOfTheHill, TEXT("Solo King of the Hill"), TEXT("SoloKingOfTheHill"), TEXT("Everyone for themself: a point a second while you stand in the hill alone; it moves every minute."), false, EWinBy::Hill, true, CheeseStrikeHills::SoloTarget, false, true},
	};

	const EMode LobbyOrder[NumModes] = {EMode::Deathmatch, EMode::Elimination, EMode::Execution, EMode::GunGame, EMode::KingOfTheHill, EMode::SoloKingOfTheHill, EMode::FreeForAll};

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
		const int32 Usual = Info(Mode).DefaultKillLimit > 0 ? Info(Mode).DefaultKillLimit : CheeseStrikeRules::KillLimit;
		const float Asked = CheeseStrikeRules::Option(Options, TEXT("KillLimit"), static_cast<float>(Usual));
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

	FString RoundWarning(float Left)
	{
		for (const float At : RoundWarnings)
		{
			if (FMath::Abs(Left - At) < 1.0f)
			{
				return FString::Printf(TEXT("%.0f seconds left in the round"), At);
			}
		}
		return FString();
	}

	float NextRoundWarning(float Left, float Within)
	{
		for (const float At : RoundWarnings)
		{
			const float In = Left - At;
			if (In >= FMath::Max(Within, 0.5f))
			{
				return In;
			}
		}
		return 0.0f;
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

	bool RoundOver(const FSide& First, const FSide& Second, bool bTimeUp, int32& OutWinner, bool bOvertimeOver)
	{
		OutWinner = NoTeam;
		const bool bBothHavePlayers = First.Members > 0 && Second.Members > 0;
		const bool bFirstGone = First.IsOut();
		const bool bSecondGone = Second.IsOut();
		if (bBothHavePlayers && (bFirstGone || bSecondGone))
		{
			OutWinner = bFirstGone && bSecondGone ? NoTeam : (bFirstGone ? 1 : 0);
			return true;
		}
		// Out of time, it ends once nobody is down (overtime), or at the end of the overtime.
		if (!bTimeUp || ((First.Down > 0 || Second.Down > 0) && !bOvertimeOver))
		{
			return false;
		}
		if (First.Alive != Second.Alive)
		{
			OutWinner = First.Alive > Second.Alive ? 0 : 1;
		}
		else if (First.Standing() != Second.Standing())
		{
			OutWinner = First.Standing() > Second.Standing() ? 0 : 1;
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

	int32 StartSide(int32 Team, int32 Round, bool bRounds)
	{
		if (!bRounds || Team < 0 || Team > 1 || Round < 2)
		{
			return Team;
		}
		return (Round - 1) % 2 == 0 ? Team : 1 - Team;
	}

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

	ELastStandHit LastStandHit(bool bLastStand, bool bDowned, float Health, float Damage, bool bEnemyClose)
	{
		if (!bLastStand)
		{
			return ELastStandHit::Hurt;
		}
		if (bDowned)
		{
			return bEnemyClose ? ELastStandHit::Finish : ELastStandHit::Absorbed;
		}
		return Health - Damage <= 0.0f ? ELastStandHit::Down : ELastStandHit::Hurt;
	}

	EInteract InteractWith(bool bTargetDowned, bool bSameSide, float Distance)
	{
		if (!bTargetDowned || Distance > InteractRange)
		{
			return EInteract::None;
		}
		return bSameSide ? EInteract::Revive : EInteract::Execute;
	}

	float InteractTime(EInteract Kind)
	{
		switch (Kind)
		{
		case EInteract::Revive:
			return ReviveTime;
		default:
			// Executing is a press.
			return 0.0f;
		}
	}

	FString InteractPrompt(EInteract Kind, const FString& Key, const FString& Name)
	{
		if (Kind == EInteract::None)
		{
			return FString();
		}
		const bool bRevive = Kind == EInteract::Revive;
		return FString::Printf(TEXT("%s %s  ·  %s %s"), bRevive ? TEXT("HOLD") : TEXT("PRESS"), *Key, bRevive ? TEXT("REVIVE") : TEXT("EXECUTE"), *Name.ToUpper());
	}

	int32 SelfReviveSeconds(float Left)
	{
		return FMath::Max(0, FMath::CeilToInt(Left - 0.001f));
	}

	float SelfReviveLeft(float DownedAt, float Now)
	{
		return FMath::Max(0.0f, DownedAt + SelfReviveTime - Now);
	}

	FRotator DownedSway(float Seconds)
	{
		// Two slow waves each way that never quite repeat, as a hurt arm and a heaving chest move the aim.
		const float Yaw = 0.65f * FMath::Sin(Seconds * 1.9f) + 0.35f * FMath::Sin(Seconds * 4.3f + 1.1f);
		const float Pitch = 0.6f * FMath::Sin(Seconds * 2.6f + 0.4f) + 0.4f * FMath::Sin(Seconds * 5.1f + 2.3f);
		return FRotator(Pitch * DownedSwayPitch, Yaw * DownedSwayYaw, 0.0f);
	}

	FWeaponStats DownedWeapon(const FWeaponStats& Weapon)
	{
		FWeaponStats Down = Weapon;
		Down.Damage *= DownedDamage;
		Down.BackstabDamage *= DownedDamage;
		Down.FireInterval *= DownedFireInterval;
		Down.BurstInterval *= DownedFireInterval;
		Down.HipSpreadDegrees *= DownedSpread;
		Down.SpreadDegrees *= DownedSpread;
		Down.RecoilUp *= DownedKick;
		Down.RecoilSide *= DownedKick;
		Down.RecoilCap *= DownedKick;
		// Fully spread from the first shot: no steady first shot to tap out one at a time.
		Down.BloomShots = 1;
		return Down;
	}

	FGunGameKill GunGameKill(int32 KillerLevel, int32 Steps)
	{
		FGunGameKill Kill;
		const int32 Last = FMath::Max(0, Steps - 1);
		Kill.KillerLevel = FMath::Clamp(KillerLevel, 0, Last);
		if (Kill.KillerLevel >= Last)
		{
			Kill.bWins = true;
			return Kill;
		}
		Kill.KillerLevel = Kill.KillerLevel + 1;
		return Kill;
	}

	FLinearColor TeamColour(int32 Team)
	{
		switch (Team)
		{
		case 0:
			return FLinearColor::FromSRGBColor(FColor(230, 159, 0));
		case 1:
			return FLinearColor::FromSRGBColor(FColor(240, 236, 224));
		default:
			return FLinearColor(0.96f, 0.72f, 0.2f);
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
		case EWinBy::Ladder:
			return FString::Printf(TEXT("first through all %d weapons"), CheeseStrikeArsenal::GunGameLadder().Num());
		case EWinBy::Hill:
			return FString::Printf(TEXT("first %sto %d points"), Info(Mode).bTeams ? TEXT("team ") : TEXT(""), KillLimit);
		default:
			return FString::Printf(TEXT("first to %d kills"), KillLimit);
		}
	}
}
