// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBadges.h"
#include "Weapons/CheeseStrikeArsenal.h"

namespace CheeseStrikeBadges
{
	uint8 BadgesFor(const FKillFacts& Facts)
	{
		if (!Facts.bByAnother)
		{
			return 0;
		}
		uint8 Badges = 0;
		const auto Add = [&Badges](EBadge Badge) { Badges |= 1 << static_cast<uint8>(Badge); };
		const bool bGun = !Facts.Weapon.IsNone() && !Facts.bExecuted;
		const FWeaponStats* Stats = bGun ? CheeseStrikeArsenal::Find(Facts.Weapon) : nullptr;
		const bool bBlade = Stats != nullptr && Stats->IsMelee();
		if (Facts.bHead && bGun && !bBlade)
		{
			Add(EBadge::Headshot);
		}
		if (bBlade && Facts.bFromBehind)
		{
			Add(EBadge::Assassination);
		}
		if (Facts.bExecuted)
		{
			Add(EBadge::Execution);
		}
		if (bGun && Facts.Weapon == CheeseStrikeWeapons::Shotgun().Id && Facts.Distance <= PointBlankRange)
		{
			Add(EBadge::PointBlank);
		}
		if (bGun && !bBlade && Facts.Distance >= LongshotRange)
		{
			Add(EBadge::Longshot);
		}
		if (Facts.bRevenge)
		{
			Add(EBadge::Revenge);
		}
		const bool bRoquefortHead = Facts.bHead && Facts.Weapon == CheeseStrikeWeapons::Roquefort().Id;
		if (bGun && !bBlade && Facts.bOneHit && !bRoquefortHead)
		{
			Add(EBadge::OneShot);
		}
		return Badges;
	}

	TArray<EBadge> List(uint8 Badges)
	{
		TArray<EBadge> Out;
		for (int32 Each = 0; Each < NumBadges; ++Each)
		{
			if (Has(Badges, static_cast<EBadge>(Each)))
			{
				Out.Add(static_cast<EBadge>(Each));
			}
		}
		return Out;
	}

	const TCHAR* Word(EBadge Badge)
	{
		switch (Badge)
		{
		case EBadge::Headshot: return TEXT("HEADSHOT");
		case EBadge::Assassination: return TEXT("ASSASSINATION");
		case EBadge::Execution: return TEXT("EXECUTION");
		case EBadge::PointBlank: return TEXT("POINT BLANK");
		case EBadge::Longshot: return TEXT("LONGSHOT");
		case EBadge::Revenge: return TEXT("REVENGE");
		case EBadge::OneShot: return TEXT("ONE SHOT");
		default: return TEXT("");
		}
	}
}
