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
			// Wave 16: a tenth less damage (20 to 18: six hits to a kill, not five) and a little less accurate aimed (0.3
			// degrees to 0.45), which tells at long range. Wave 17: a fifth less damage again (18 to 14.4: seven hits to a kill,
			// not six) and less accurate at long range (aimed, 0.45 degrees to 0.8: at 30 m a shot strays up to 21 cm, at 50 m
			// up to 35, as wide as a body).
			FWeaponStats Weapon = MakeWeapon(TEXT("blaster"), NSLOCTEXT("CheeseStrike", "Blaster", "Cheddar Blaster"), EWeaponSlot::Primary);
			Weapon.Damage = 14.4f;
			Weapon.HeadMultiplier = 1.75f;
			Weapon.FireInterval = 0.12f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 24;
			Weapon.ReloadTime = 1.6f;
			Weapon.Reserve = 72;
			Weapon.ReserveMax = 120;
			SetReach(Weapon, 150.0f, 40.0f, 90.0f, 0.6f);
			Weapon.SpreadDegrees = 0.8f;
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
			// Wave 14: a needler (Halo's, in cheese). Shards of grated cheese flying at 40 m/s, ten a second, each turning
			// toward the nearest enemy within 10 degrees of the aim it was fired along at up to 25 degrees a second (the
			// server's choice); sixteen in one player within two seconds burst for a kill. The id stays the Parmesan
			// Grater's it replaces. At 5 a shard sixteen (80) leave a full-health player standing, so the supercombine is
			// what kills. Wave 16: much less accurate at range, so that a kill at 40 m is next to impossible: a wider cone (3
			// degrees aimed, 7 from the hip), a shard's damage falling from 15 m to 0.4 of it at 35 m, homing only on someone
			// within 20 m, and only shards that struck within 25 m counting toward the supercombine. Wave 17: sure close and
			// hopeless far: homing much harder (60 degrees a second, from 25, so that a shard follows someone strafing across
			// 10 m) but only on someone within 16 m (from 20); past that its cone alone (4.5 degrees aimed, from 3) decides, and
			// only shards that struck within 22 m (from 25) count toward the supercombine. A magazine kills at 10 m nearly
			// always, at 20 m seldom (standing still, aimed) and at 30 m never (CheeseStrike.Weapons.TheGraterKillsCloseNotFar).
			FWeaponStats Weapon = MakeWeapon(TEXT("grater"), NSLOCTEXT("CheeseStrike", "Grater", "Cheese Grater"), EWeaponSlot::Primary);
			Weapon.Damage = 5.0f;
			Weapon.HeadMultiplier = 1.25f;
			Weapon.FireInterval = 0.1f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 20;
			Weapon.ReloadTime = 1.8f;
			Weapon.Reserve = 60;
			Weapon.ReserveMax = 100;
			Weapon.ProjectileSpeed = 4000.0f;
			Weapon.GravityShare = 0.0f;
			Weapon.ProjectileLife = 1.5f;
			Weapon.MaxRange = Weapon.ProjectileSpeed * Weapon.ProjectileLife;
			Weapon.FalloffStart = 1500.0f;
			Weapon.FalloffEnd = 3500.0f;
			Weapon.FalloffFloor = 0.4f;
			Weapon.HomingTurn = 60.0f;
			Weapon.HomingCone = 10.0f;
			Weapon.HomingReach = 1600.0f;
			Weapon.SuperShards = 16;
			Weapon.SuperWindow = 2.0f;
			Weapon.SuperReach = 2200.0f;
			Weapon.SpreadDegrees = 4.5f;
			Weapon.HipSpreadDegrees = 7.0f;
			Weapon.BloomShots = 8;
			SetKick(Weapon, 0.2f, 8.0f, 0.3f, 0.25f);
			Weapon.AimZoom = 0.8f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Munster()
	{
		static const FWeaponStats Stats = []
		{
			// Wave 14: the Munster Machine Gun made two submachine guns, one in each hand (the Dual Burratas' two guns),
			// automatic, firing in turn, a belt of 60 each; no sights, close and loose. Wave 16: less accurate at long range
			// (aimed, a cone of 2.5 degrees, not 1.5). Wave 17: too strong for a loadout weapon (eight body hits killed): a hit
			// of 6.25, not 13, so that it takes 16 body hits from the two guns together, firing a little faster (a shot every
			// 0.06 s, from 0.075) so that a kill with every hit (0.9 s, from 0.525) is still quicker than the Colby Carbine's
			// (0.96 s); and a magazine of 64 (32 a gun) in place of 120: four kills a magazine at every hit, about the Cheddar
			// Blaster's (3.4) and the Colby Carbine's (3.9), where 120 would be seven and a half; a reserve of two magazines,
			// four at most, as before.
			FWeaponStats Weapon = MakeWeapon(TEXT("munster"), NSLOCTEXT("CheeseStrike", "Munster", "Munster Twin SMGs"), EWeaponSlot::Primary);
			Weapon.bDual = true;
			Weapon.Damage = 6.25f;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 0.06f;
			Weapon.bAutomatic = true;
			Weapon.MagazineSize = 64;
			Weapon.ReloadTime = 3.2f;
			Weapon.Reserve = 128;
			Weapon.ReserveMax = 256;
			SetReach(Weapon, 80.0f, 15.0f, 45.0f, 0.45f);
			Weapon.SpreadDegrees = 2.5f;
			Weapon.HipSpreadDegrees = 8.0f;
			Weapon.BloomShots = 12;
			SetKick(Weapon, 0.3f, 10.0f, 0.5f, 0.3f);
			Weapon.AimZoom = 0.8f;
			Weapon.DrawTime = 0.5f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Shotgun()
	{
		static const FWeaponStats Stats = []
		{
			// Wave 14: twelve pellets of 14, the whole of them from 4 m in, falling to 0.15 of it at 12 m: a kill a shot up
			// to about 4 m, weak past 8. Wave 15: a wider cone (9 degrees aimed, 14 from the hip: most land on a body at
			// 4 m, few at 8) and a slower shot, 1.19 s between. Wave 17: a heavy kick each shot, as the Roquefort's (its shots,
			// each the first of its run, never had the recoil's climb): the view thrown up 7.5 degrees and eased back over 0.5 s,
			// 1.8 of them kept, the gun in view thrown back 11 cm and tipped up.
			FWeaponStats Weapon = MakeWeapon(TEXT("shotgun"), NSLOCTEXT("CheeseStrike", "Shotgun", "Swiss Shotgun"), EWeaponSlot::Primary);
			Weapon.Damage = 14.0f;
			Weapon.Pellets = 12;
			Weapon.HeadMultiplier = 1.5f;
			Weapon.FireInterval = 1.19f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 6;
			Weapon.ReloadTime = 2.2f;
			Weapon.Reserve = 18;
			Weapon.ReserveMax = 30;
			Weapon.MaxRange = 40.0f * ArsenalGodotMetre;
			Weapon.FalloffStart = 400.0f;
			Weapon.FalloffEnd = 1200.0f;
			Weapon.FalloffFloor = 0.15f;
			Weapon.SpreadDegrees = 9.0f;
			Weapon.HipSpreadDegrees = 14.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 3.0f, 2.0f, 0.6f, 1.0f);
			Weapon.KickPitch = 7.5f;
			Weapon.KickSeconds = 0.5f;
			Weapon.KickKept = 1.8f;
			Weapon.KickBack = 11.0f;
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
			// Aimed (wave 15: both guns up side by side before the eyes) a degree; from the hip a little tighter than a
			// pistol's at full bloom.
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
			// Wave 16: 40% longer between shots (1.3 s to 1.82) and a heavy kick each shot (KickPitch: the view thrown up 7
			// degrees and eased back over 0.55 s, 1.5 of them kept; the rifle in view thrown back 9 cm and tipped up).
			FWeaponStats Weapon = MakeWeapon(TEXT("roquefort"), NSLOCTEXT("CheeseStrike", "Roquefort", "Roquefort Rifle"), EWeaponSlot::Primary);
			Weapon.Damage = 75.0f;
			Weapon.HeadMultiplier = 2.0f;
			Weapon.FireInterval = 1.82f;
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
			Weapon.KickPitch = 7.0f;
			Weapon.KickSeconds = 0.55f;
			Weapon.KickKept = 1.5f;
			Weapon.KickBack = 9.0f;
			Weapon.AimZoom = 0.3f;
			Weapon.bScoped = true;
			Weapon.DrawTime = 0.5f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& BabyBomb()
	{
		static const FWeaponStats Stats = []
		{
			// Wave 14's power weapon: a launcher of red-waxed wheels, two in the tube and two more, a wheel a second; a direct
			// hit 150, its burst 110 within 3 m falling to 30 at 5 m, its shooter not spared. Wave 15: a slow, lobbed wheel at
			// 12.5 m/s, falling at a twelfth of gravity so that, fired level from the eyes, it comes down about 25 m out
			// (CheeseStrikeWeapons::LevelReach), as the faster wheel with more drop did. Wave 16: half as fast again (18.75 m/s)
			// with a heavier drop (0.35 of gravity), so that one fired level comes down about 18 m out and anything further is
			// lobbed, and a shorter life (1.6 s: 30 m at most, from 37.5).
			FWeaponStats Weapon = MakeWeapon(TEXT("babybomb"), NSLOCTEXT("CheeseStrike", "BabyBomb", "BabyBomb"), EWeaponSlot::Primary);
			Weapon.Damage = 150.0f;
			Weapon.HeadMultiplier = 1.0f;
			Weapon.FireInterval = 1.0f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 2;
			Weapon.ReloadTime = 2.5f;
			Weapon.Reserve = 2;
			Weapon.ReserveMax = 4;
			Weapon.ProjectileSpeed = 1875.0f;
			Weapon.GravityShare = 0.35f;
			Weapon.ProjectileLife = 1.6f;
			Weapon.MaxRange = Weapon.ProjectileSpeed * Weapon.ProjectileLife;
			Weapon.FalloffStart = 0.0f;
			Weapon.FalloffEnd = 0.0f;
			Weapon.FalloffFloor = 1.0f;
			Weapon.SplashRadius = 500.0f;
			Weapon.SplashInner = 300.0f;
			Weapon.SplashDamage = 110.0f;
			Weapon.SplashEdgeDamage = 30.0f;
			Weapon.BotLeast = 600.0f;
			Weapon.SpreadDegrees = 0.0f;
			Weapon.HipSpreadDegrees = 2.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 4.0f, 2.0f, 0.5f, 0.9f);
			Weapon.AimZoom = 0.75f;
			Weapon.DrawTime = 0.7f;
			Weapon.CarrySpeed = 0.85f;
			return Weapon;
		}();
		return Stats;
	}

	const FWeaponStats& Stick()
	{
		static const FWeaponStats Stats = []
		{
			// Wave 14's melee power weapon: a 1.2 m baton of string cheese, a slow heavy swing that fells everyone within 3 m
			// in a 120-degree arc in front (down in Execution), never a teammate; no rounds; a little slow to carry. Wave 15:
			// the swing shown 1.6 times slower (1.28 s, the whoosh's length), so a swing every 1.4 s. Wave 16: the blow lands
			// as the head comes round to the aim, 0.49 s into the swing (CheeseStrikeWeapons::ContactDelay), on whoever is in
			// the arc then, not as the swing starts. Wave 17: twelve swings (FWeaponStats::HasSwings), shown on the HUD as its
			// rounds and topped up by a stand's or a dropped stick's as a gun's rounds are, never by a kill; out of them, it
			// is put away for the sidearm (ACheeseStrikeWeapon::Strike). Twelve, the fewest the user asked for: every one may
			// fell everyone in its arc.
			FWeaponStats Weapon = MakeWeapon(TEXT("stick"), NSLOCTEXT("CheeseStrike", "Stick", "Cheese Stick"), EWeaponSlot::Primary);
			Weapon.Damage = 500.0f;
			Weapon.BackstabDamage = 500.0f;
			Weapon.HeadMultiplier = 1.0f;
			Weapon.FireInterval = 1.4f;
			Weapon.bAutomatic = false;
			Weapon.MagazineSize = 12;
			Weapon.Reserve = 0;
			Weapon.ReserveMax = 0;
			Weapon.ReloadTime = 0.0f;
			Weapon.Reach = 300.0f;
			Weapon.MaxRange = Weapon.Reach;
			Weapon.FalloffStart = 0.0f;
			Weapon.FalloffEnd = 0.0f;
			Weapon.ArcDegrees = 120.0f;
			Weapon.SwingTime = 0.5f;
			Weapon.SwipeSeconds = 1.28f;
			Weapon.SwipeSize = 1.25f;
			Weapon.SpreadDegrees = 0.0f;
			Weapon.HipSpreadDegrees = 0.0f;
			Weapon.BloomShots = 1;
			SetKick(Weapon, 0.0f, 1.0f, 0.0f, 0.3f);
			Weapon.AimZoom = 1.0f;
			Weapon.DrawTime = 0.6f;
			Weapon.CarrySpeed = 0.9f;
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
			// Wave 16: no longer endless. A sidearm's twelve and three magazines besides (72 at most), topped up by any
			// press for a weapon or its rounds at a stand or on the floor (CheeseStrikePickups::SidearmTopUp).
			Weapon.MagazineSize = 12;
			Weapon.ReloadTime = 1.2f;
			Weapon.Reserve = 36;
			Weapon.ReserveMax = 72;
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
			Weapon.Barrels = 6;
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
		static const TArray<const FWeaponStats*> Weapons = {&W::Blaster(), &W::Carbine(), &W::Grater(), &W::Munster(), &W::Shotgun(), &W::Burratas(), &W::Roquefort(), &W::BabyBomb(), &W::Stick(),
			&W::Pistol(), &W::Knife(), &W::Gatling()};
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
		// The BabyBomb, which a near miss still kills with, after the Roquefort; the Cheese Stick, a swing up close, before the
		// pistol and the knife.
		static const TArray<FName> Ladder = {W::Blaster().Id, W::Shotgun().Id, W::Carbine().Id, W::Burratas().Id, W::Munster().Id, W::Grater().Id, W::Roquefort().Id, W::BabyBomb().Id,
			W::Stick().Id, W::Pistol().Id, W::Knife().Id};
		return Ladder;
	}

	FName GunGameWeapon(int32 Kills)
	{
		const TArray<FName>& Ladder = GunGameLadder();
		return Ladder[FMath::Clamp(Kills, 0, Ladder.Num() - 1)];
	}
}
