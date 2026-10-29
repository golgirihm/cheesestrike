// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeArsenal.h"
#include "Movement/CheeseStrikeMovement.h"

namespace
{
	/** A Godot metre, at life size: the design's lengths are three times life size. */
	constexpr float ArsenalGodotMetre = 100.0f / 3.0f;

	FWeaponStats MakeWeapon(const TCHAR* Id, const FText& Name, EWeaponSlot Slot)
	{
		FWeaponStats Stats;
		Stats.Id = FName(Id);
		Stats.DisplayName = Name;
		Stats.Slot = Slot;
		return Stats;
	}

	/** Range and falloff, from the design's Godot metres. */
	void SetReach(FWeaponStats& Stats, float Range, float FalloffStart, float FalloffEnd, float FalloffFloor)
	{
		Stats.MaxRange = Range * ArsenalGodotMetre;
		Stats.FalloffStart = FalloffStart * ArsenalGodotMetre;
		Stats.FalloffEnd = FalloffEnd * ArsenalGodotMetre;
		Stats.FalloffFloor = FalloffFloor;
	}

	void SetKick(FWeaponStats& Stats, float Up, float Cap, float Side, float Reset)
	{
		Stats.RecoilUp = Up;
		Stats.RecoilCap = Cap;
		Stats.RecoilSide = Side;
		Stats.RecoilReset = Reset;
	}
}

namespace CheeseStrikeWeapons
{
	const FWeaponStats& Blaster()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("blaster"), NSLOCTEXT("CheeseStrike", "Blaster", "Cheddar Blaster"), EWeaponSlot::Primary);
			Weapon.Damage = 20.0f;
			Weapon.HeadMultiplier = 1.75f;
			Weapon.FireInterval = 0.12f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 24;
			Weapon.ReloadTime = 1.6f;
			Weapon.Reserve = 72;
			Weapon.ReserveMax = 120;
			SetReach(Weapon, 150.0f, 40.0f, 90.0f, 0.6f);
			Weapon.SpreadDegrees = 0.3f;
			Weapon.HipSpreadDegrees = 9.0f;
			Weapon.BloomShots = 8;
			SetKick(Weapon, 0.45f, 9.0f, 0.35f, 0.3f);
			Weapon.AimZoom = 0.6f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Carbine()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("carbine"), NSLOCTEXT("CheeseStrike", "Carbine", "Colby Carbine"), EWeaponSlot::Primary);
			// The spawn primary (the user's, October 2026): softer than the design's 18, three bursts to a kill rather than
			// two, and two magazines in reserve, so that a weapon on a stand is worth going for.
			Weapon.Damage = 15.0f;
			Weapon.HeadMultiplier = 1.6f;
			Weapon.FireInterval = 0.48f;
			Weapon.bAutomatic = false;
			Weapon.Burst = 3;
			Weapon.BurstInterval = 0.07f;
			Weapon.MagazineSize = 27;
			Weapon.ReloadTime = 1.7f;
			Weapon.Reserve = 54;
			Weapon.ReserveMax = 135;
			SetReach(Weapon, 140.0f, 35.0f, 80.0f, 0.6f);
			Weapon.SpreadDegrees = 0.25f;
			Weapon.HipSpreadDegrees = 7.0f;
			Weapon.BloomShots = 6;
			SetKick(Weapon, 0.45f, 6.0f, 0.2f, 0.3f);
			Weapon.AimZoom = 0.6f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Grater()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("grater"), NSLOCTEXT("CheeseStrike", "Grater", "Parmesan Grater"), EWeaponSlot::Primary);
			Weapon.Damage = 12.0f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.065f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 45;
			Weapon.ReloadTime = 1.5f;
			Weapon.Reserve = 135;
			Weapon.ReserveMax = 225;
			SetReach(Weapon, 80.0f, 15.0f, 45.0f, 0.4f);
			Weapon.SpreadDegrees = 2.5f;
			Weapon.HipSpreadDegrees = 12.0f;
			Weapon.BloomShots = 12;
			SetKick(Weapon, 0.28f, 12.0f, 0.5f, 0.25f);
			Weapon.AimZoom = 0.75f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Munster()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("munster"), NSLOCTEXT("CheeseStrike", "Munster", "Munster Machine Gun"), EWeaponSlot::Primary);
			Weapon.Damage = 16.0f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.085f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 75;
			Weapon.ReloadTime = 3.6f;
			Weapon.Reserve = 75;
			Weapon.ReserveMax = 150;
			SetReach(Weapon, 120.0f, 25.0f, 70.0f, 0.5f);
			Weapon.SpreadDegrees = 1.2f;
			Weapon.HipSpreadDegrees = 13.0f;
			Weapon.BloomShots = 14;
			SetKick(Weapon, 0.4f, 14.0f, 0.6f, 0.3f);
			Weapon.AimZoom = 0.75f;
			Weapon.DrawTime = 0.9f;
			Weapon.CarrySpeed = 0.85f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Shotgun()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("shotgun"), NSLOCTEXT("CheeseStrike", "Shotgun", "Swiss Shotgun"), EWeaponSlot::Primary);
			Weapon.Damage = 10.0f;
			Weapon.Pellets = 9;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.85f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 6;
			Weapon.ReloadTime = 2.2f;
			Weapon.Reserve = 18;
			Weapon.ReserveMax = 30;
			SetReach(Weapon, 40.0f, 8.0f, 25.0f, 0.3f);
			Weapon.SpreadDegrees = 14.0f;
			Weapon.HipSpreadDegrees = 32.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 3.0f, 2.0f, 0.6f, 1.0f);
			Weapon.AimZoom = 0.8f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Burratas()
	{
		static const FWeaponStats Stats = []
		{
			// Counter-Strike's Dual Berettas in cheese: a pistol in each hand, firing in turn, a shot a pull (as theirs are),
			// quick and loose, with middling damage, a long magazine and a long reload; a primary here, for the loadout and
			// the bots. Pulled as fast as a finger can, no more than ten a second.
			FWeaponStats Weapon = MakeWeapon(TEXT("burratas"), NSLOCTEXT("CheeseStrike", "Burratas", "Dual Burratas"), EWeaponSlot::Primary);
			Weapon.bDual = true;
			Weapon.Damage = 15.0f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.1f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 30;
			Weapon.ReloadTime = 3.0f;
			Weapon.Reserve = 90;
			Weapon.ReserveMax = 150;
			SetReach(Weapon, 90.0f, 20.0f, 55.0f, 0.5f);
			// No sights to aim down: from the hip only, a little tighter than a pistol's at full bloom.
			Weapon.SpreadDegrees = 1.0f;
			Weapon.HipSpreadDegrees = 6.0f;
			Weapon.BloomShots = 10;
			SetKick(Weapon, 0.5f, 7.0f, 0.45f, 0.3f);
			Weapon.AimZoom = 0.8f;
			Weapon.DrawTime = 0.45f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Roquefort()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("roquefort"), NSLOCTEXT("CheeseStrike", "Roquefort", "Roquefort Rifle"), EWeaponSlot::Primary);
			Weapon.Damage = 75.0f;
			Weapon.HeadMultiplier = 2.0f;
			Weapon.FireInterval = 1.3f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 4;
			Weapon.ReloadTime = 2.6f;
			Weapon.Reserve = 12;
			Weapon.ReserveMax = 24;
			// No falloff: the whole damage as far as it reaches.
			SetReach(Weapon, 250.0f, 0.0f, 0.0f, 1.0f);
			Weapon.SpreadDegrees = 0.0f;
			Weapon.HipSpreadDegrees = 10.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 2.5f, 2.0f, 0.3f, 0.5f);
			Weapon.AimZoom = 0.3f;
			Weapon.bScoped = true;
			Weapon.DrawTime = 0.5f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Pistol()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("pistol"), NSLOCTEXT("CheeseStrike", "Pistol", "Curd Pistol"), EWeaponSlot::Sidearm);
			Weapon.Damage = 12.0f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.25f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 10;
			Weapon.ReloadTime = 1.2f;
			// A fallback that never runs dry, as the Godot game had it.
			Weapon.Reserve = -1;
			Weapon.ReserveMax = -1;
			SetReach(Weapon, 90.0f, 25.0f, 60.0f, 0.5f);
			Weapon.SpreadDegrees = 0.6f;
			Weapon.HipSpreadDegrees = 7.0f;
			Weapon.BloomShots = 4;
			SetKick(Weapon, 0.9f, 5.0f, 0.2f, 0.3f);
			Weapon.AimZoom = 0.75f;
			Weapon.DrawTime = 0.3f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Knife()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Weapon = MakeWeapon(TEXT("knife"), NSLOCTEXT("CheeseStrike", "Knife", "Cheese Knife"), EWeaponSlot::Knife);
			Weapon.Damage = 55.0f;
			Weapon.BackstabDamage = 500.0f;
			Weapon.HeadMultiplier = 1.0f;
			Weapon.FireInterval = 0.75f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 0;
			Weapon.Reserve = 0;
			Weapon.ReserveMax = 0;
			Weapon.ReloadTime = 0.0f;
			// A swipe rather than the design's lunge: a little more reach (the design's 4.5 m and a step more) and a wide sweep.
			Weapon.Reach = 5.4f * ArsenalGodotMetre;
			Weapon.MaxRange = Weapon.Reach;
			Weapon.FalloffStart = 0.0f;
			Weapon.FalloffEnd = 0.0f;
			Weapon.SweepDegrees = 45.0f;
			Weapon.SwingTime = 0.25f;
			Weapon.SpreadDegrees = 0.0f;
			Weapon.HipSpreadDegrees = 0.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 0.0f, 1.0f, 0.0f, 0.3f);
			Weapon.AimZoom = 1.0f;
			Weapon.DrawTime = 0.2f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Gatling()
	{
		static const FWeaponStats Stats = []
		{
			// The Gouda Overload's (wave 14): a minigun of molten cheese, twenty a second once its barrels have spun up, a
			// belt of 200 and no reload; wide from the hip, tighter aimed; the caller's for half a minute.
			FWeaponStats Weapon = MakeWeapon(TEXT("gatling"), NSLOCTEXT("CheeseStrike", "Gatling", "Gouda Gatling"), EWeaponSlot::Primary);
			Weapon.bSpecial = true;
			Weapon.Damage = 12.0f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.05f;
			Weapon.bAutomatic = true;
			Weapon.SpinUp = 0.4f;
			Weapon.MagazineSize = 200;
			Weapon.ReloadTime = 0.0f;
			Weapon.Reserve = 0;
			Weapon.ReserveMax = 0;
			SetReach(Weapon, 100.0f, 25.0f, 60.0f, 0.5f);
			Weapon.SpreadDegrees = 3.0f;
			Weapon.HipSpreadDegrees = 14.0f;
			Weapon.BloomShots = 10;
			SetKick(Weapon, 0.12f, 20.0f, 0.3f, 0.3f);
			Weapon.AimZoom = 0.8f;
			Weapon.DrawTime = 0.3f;
			return Weapon;
		}();
		return Stats;
	}
}

namespace CheeseStrikeArsenal
{
	const TArray<const FWeaponStats*>& All()
	{
		namespace W = CheeseStrikeWeapons;
		static const TArray<const FWeaponStats*> Weapons = {&W::Blaster(), &W::Carbine(), &W::Grater(), &W::Munster(), &W::Shotgun(), &W::Burratas(), &W::Roquefort(), &W::Pistol(), &W::Knife(), &W::Gatling()};
		return Weapons;
	}

	const FWeaponStats* Find(FName Id)
	{
		for (const FWeaponStats* Weapon : All())
		{
			// FName compares without regard to case.
			if (Weapon->Id == Id)
			{
				return Weapon;
			}
		}
		return nullptr;
	}

	TArray<FName> InSlot(EWeaponSlot Slot)
	{
		TArray<FName> Ids;
		for (const FWeaponStats* Weapon : All())
		{
			if (Weapon->Slot == Slot && !Weapon->bSpecial)
			{
				Ids.Add(Weapon->Id);
			}
		}
		return Ids;
	}

	bool IsPickable(FName Id, EWeaponSlot Slot)
	{
		const FWeaponStats* Weapon = Find(Id);
		return Weapon != nullptr && Weapon->Slot == Slot && !Weapon->bSpecial;
	}

	FName DefaultPrimary()
	{
		// The weakest primary, so that the stands' weapons are worth taking (Rules/CheeseStrikePickups.h).
		return CheeseStrikeWeapons::Carbine().Id;
	}

	FName Sidearm()
	{
		return CheeseStrikeWeapons::Pistol().Id;
	}

	FName Knife()
	{
		return CheeseStrikeWeapons::Knife().Id;
	}

	FString ListIds(EWeaponSlot Slot)
	{
		TArray<FString> Names;
		for (const FName& Id : InSlot(Slot))
		{
			Names.Add(Id.ToString());
		}
		return FString::Join(Names, TEXT(", "));
	}

	const TArray<FName>& GunGameLadder()
	{
		namespace W = CheeseStrikeWeapons;
		static const TArray<FName> Ladder = {W::Blaster().Id, W::Shotgun().Id, W::Carbine().Id, W::Burratas().Id, W::Grater().Id, W::Munster().Id, W::Roquefort().Id, W::Pistol().Id, W::Knife().Id};
		return Ladder;
	}

	FName GunGameWeapon(int32 Kills)
	{
		const TArray<FName>& Ladder = GunGameLadder();
		return Ladder[FMath::Clamp(Kills, 0, Ladder.Num() - 1)];
	}
}
