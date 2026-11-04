// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLoadout.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Weapons/CheeseStrikeArsenal.h"

namespace CheeseStrikeLoadout
{
	const TArray<FName>& Primaries()
	{
		static const TArray<FName> Offered = {CheeseStrikeWeapons::Carbine().Id, CheeseStrikeWeapons::Blaster().Id, CheeseStrikeWeapons::Munster().Id};
		return Offered;
	}

	bool IsPrimaryOffered(FName Id)
	{
		return Primaries().Contains(Id);
	}

	FName DefaultPrimary()
	{
		return CheeseStrikeWeapons::Carbine().Id;
	}

	int32 DefaultHero()
	{
		return CheeseStrikeBodies::DefaultHero;
	}

	FName PrimaryOrDefault(FName Id)
	{
		return IsPrimaryOffered(Id) ? Id : DefaultPrimary();
	}

	bool HasLoadout(CheeseStrikeModes::EMode Mode)
	{
		return CheeseStrikeModes::Info(Mode).WinBy != CheeseStrikeModes::EWinBy::Ladder;
	}

	bool MayChange(EWhen When)
	{
		return When == EWhen::FrontEnd || When == EWhen::Lobby || When == EWhen::CountIn;
	}

	EWhen WhenFor(bool bInAMatch, bool bMatchStarted, bool bMatchEnded, bool bCountingIn)
	{
		if (!bInAMatch)
		{
			return EWhen::FrontEnd;
		}
		if (bMatchEnded)
		{
			return EWhen::Over;
		}
		if (!bMatchStarted)
		{
			return EWhen::Lobby;
		}
		return bCountingIn ? EWhen::CountIn : EWhen::Playing;
	}

	bool OffersPage(bool bHasLoadout, bool bCountingIn, float GoAt, float DismissedGoAt)
	{
		return bHasLoadout && bCountingIn && GoAt > 0.0f && DismissedGoAt != GoAt;
	}

	FString CountText(bool bRound, float Left)
	{
		return FString::Printf(TEXT("THE %s BEGINS IN %d"), bRound ? TEXT("ROUND") : TEXT("MATCH"), FMath::Max(1, FMath::CeilToInt(Left)));
	}
}
