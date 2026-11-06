// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The arsenal as arithmetic: every weapon's numbers against the design's
// table (time to kill, bursts, pellets, the knife's reach and its stroke in
// the back, draw times, life-size lengths), the lists the game is played by
// (loadouts, Gun Game's ladder), and how far a shot carries to the ear.

#include "Misc/AutomationTest.h"
#include "Weapons/CheeseStrikeBeat.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "Animation/CheeseStrikeDualHold.h"
#include "Weapons/CheeseStrikeWeaponLooks.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeGunfire.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Rules/CheeseStrikePickups.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace WeaponTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
	/** A Godot metre at life size. */
	constexpr float Metre = 100.0f / 3.0f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryWeaponKillsInTheDesignsTime, "CheeseStrike.Weapons.EveryWeaponKillsInTheDesignsTime", WeaponTests::Flags)

bool FEveryWeaponKillsInTheDesignsTime::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	struct FWanted
	{
		const FWeaponStats& Weapon;
		int32 Shots;
		float Seconds;
	};
	// The design's TTK column (docs/unreal/design.md, "Weapons"): body shots, close, holding the trigger. The Colby
	// Carbine, the spawn primary, is softened from it: three bursts to a kill. Wave 14: the Cheese Grater's shards (before
	// the supercombine), the Munster Twin SMGs, the Swiss Shotgun's whole blast close, the BabyBomb's wheel and the Cheese
	// Stick's swing. Wave 16: the Cheddar Blaster a tenth softer (six hits, not five), the Roquefort 40% slower between shots.
	// Wave 17: the Cheddar Blaster a fifth softer again (seven hits), the Munster Twin SMGs sixteen hits, not eight.
	for (const FWanted& Wanted : {FWanted{Pistol(), 9, 2.0f}, FWanted{Blaster(), 7, 0.72f}, FWanted{Carbine(), 7, 0.96f}, FWanted{Grater(), 20, 1.9f},
		FWanted{Munster(), 16, 0.9f}, FWanted{Shotgun(), 1, 0.0f}, FWanted{Roquefort(), 2, 1.82f}, FWanted{Knife(), 2, 0.75f}, FWanted{Burratas(), 7, 0.6f},
		FWanted{BabyBomb(), 1, 0.0f}, FWanted{Stick(), 1, 0.0f}})
	{
		const FString Name = Wanted.Weapon.DisplayName.ToString();
		TestEqual(FString::Printf(TEXT("%s kills in %d"), *Name, Wanted.Shots), ShotsToKill(Wanted.Weapon), Wanted.Shots);
		TestTrue(FString::Printf(TEXT("in %.2f s (%.2f)"), Wanted.Seconds, TimeToKill(Wanted.Weapon)), FMath::IsNearlyEqual(TimeToKill(Wanted.Weapon), Wanted.Seconds, 0.01f));
	}
	TestTrue(TEXT("the Roquefort kills with one shot to the head"), DamageAt(Roquefort(), Roquefort().MaxRange, true) >= MaxHealth);
	TestTrue(TEXT("however far: it has no falloff"), DamageAt(Roquefort(), Roquefort().MaxRange, false) == Roquefort().Damage);
	{
		// At every range it reaches, a shot to the head takes a full-health player's whole health: killed, or down in Execution.
		float Least = BIG_NUMBER;
		for (float Distance = 0.0f; Distance <= Roquefort().MaxRange; Distance += 100.0f)
		{
			Least = FMath::Min(Least, DamageAt(Roquefort(), Distance, true));
		}
		TestTrue(FString::Printf(TEXT("a Roquefort headshot takes a full health at any range (the least %.0f)"), Least), Least >= MaxHealth);
	}
	TestTrue(TEXT("the Curd Pistol is the weakest gun"), Pistol().Damage / Pistol().FireInterval < Blaster().Damage / Blaster().FireInterval);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWave16TunesTheWeapons, "CheeseStrike.Weapons.Wave16TunesTheWeapons", WeaponTests::Flags)

bool FWave16TunesTheWeapons::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	// The Cheddar Blaster: less damage and less accurate aimed (wave 17 took it further: CheeseStrike.Weapons.Wave17TunesTheWeapons).
	TestTrue(TEXT("the Cheddar Blaster's hit under its 20"), Blaster().Damage < 20.0f);
	TestTrue(FString::Printf(TEXT("and its aimed cone wider (%.2f degrees, from 0.3)"), Blaster().SpreadDegrees), Blaster().SpreadDegrees > 0.3f);
	// The Munster Twin SMGs: less accurate at long range.
	TestTrue(FString::Printf(TEXT("the Munster Twin SMGs' aimed cone wider (%.1f degrees, from 1.5)"), Munster().SpreadDegrees), Munster().SpreadDegrees >= 2.5f);
	// The Roquefort: 40% longer between shots, and a heavy kick each shot.
	TestTrue(FString::Printf(TEXT("the Roquefort's shots 40 percent further apart (%.2f s)"), Roquefort().FireInterval), FMath::IsNearlyEqual(Roquefort().FireInterval, 1.3f * 1.4f, 0.01f));
	TestTrue(TEXT("a heavy kick: the view thrown up several degrees"), Roquefort().KickPitch >= 5.0f && Roquefort().KickKept > 0.0f && Roquefort().KickKept < Roquefort().KickPitch && Roquefort().KickBack > 0.0f);
	TestEqual(TEXT("no kick before the shot"), KickAt(Roquefort(), -0.01f), 0.0f);
	TestTrue(FString::Printf(TEXT("all of it at once (%.2f at 40 ms)"), KickAt(Roquefort(), KickRise)), FMath::IsNearlyEqual(KickAt(Roquefort(), KickRise), 1.0f, 0.001f) && KickAt(Roquefort(), 0.02f) > 0.5f);
	TestTrue(TEXT("eased back"), KickAt(Roquefort(), 0.2f) < KickAt(Roquefort(), 0.1f) && KickAt(Roquefort(), 0.1f) < 1.0f);
	TestEqual(TEXT("gone by its seconds"), KickAt(Roquefort(), Roquefort().KickSeconds), 0.0f);
	TestTrue(TEXT("long before the next shot"), Roquefort().KickSeconds < Roquefort().FireInterval / 2.0f);
	TestEqual(TEXT("a gun without one never kicks so"), KickAt(Blaster(), 0.02f), 0.0f);
	// The Curd Pistol: a magazine and a reserve, as the other guns have.
	TestFalse(TEXT("the Curd Pistol's rounds run out now"), Pistol().HasEndlessReserve());
	TestTrue(FString::Printf(TEXT("a sidearm's magazine (%d) and three more (%d, %d at most)"), Pistol().MagazineSize, Pistol().Reserve, Pistol().ReserveMax),
		Pistol().MagazineSize >= 10 && Pistol().MagazineSize <= 15 && Pistol().Reserve == 3 * Pistol().MagazineSize && Pistol().ReserveMax == 6 * Pistol().MagazineSize);
	// The Cheese Stick: the blow lands as the head comes round to the aim, not as the swing starts.
	const float Contact = ContactDelay(Stick());
	TestTrue(FString::Printf(TEXT("the stick's blow lands %.2f s into its swing"), Contact), Contact > 0.3f && Contact < SwipeSecondsOf(Stick()) * 0.5f && Contact < Stick().FireInterval);
	const CheeseStrikeArms::FKnifeAt AtContact = CheeseStrikeArms::KnifeSwipe(CheeseStrikeArms::SwipeContactPhase);
	TestTrue(TEXT("when its head points straight ahead"), AtContact.Blade.X > 0.9f && FMath::Abs(AtContact.Blade.Y) < 0.05f);
	TestTrue(TEXT("having come across from the right"), CheeseStrikeArms::KnifeSwipe(0.25f).Blade.Y > 0.3f && CheeseStrikeArms::KnifeSwipe(0.5f).Blade.Y < -0.3f);
	TestEqual(TEXT("the knife's stroke lands as it is made"), ContactDelay(Knife()), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWave17TunesTheWeapons, "CheeseStrike.Weapons.Wave17TunesTheWeapons", WeaponTests::Flags)

bool FWave17TunesTheWeapons::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	// The Cheddar Blaster: a fifth less damage (18 to 14.4), and less accurate at long range.
	TestTrue(FString::Printf(TEXT("the Cheddar Blaster's hit a fifth under wave 16's 18 (%.1f)"), Blaster().Damage), FMath::IsNearlyEqual(Blaster().Damage, 18.0f * 0.8f, 0.001f));
	TestEqual(TEXT("seven hits to a kill"), ShotsToKill(Blaster()), 7);
	const float BlasterAt30 = FMath::Tan(FMath::DegreesToRadians(Blaster().SpreadDegrees / 2.0f)) * 3000.0f;
	const float BlasterAt50 = FMath::Tan(FMath::DegreesToRadians(Blaster().SpreadDegrees / 2.0f)) * 5000.0f;
	TestTrue(FString::Printf(TEXT("its aimed cone wider than wave 16's 0.45 degrees (%.2f)"), Blaster().SpreadDegrees), Blaster().SpreadDegrees >= 0.7f);
	TestTrue(FString::Printf(TEXT("at 30 m a shot strays up to %.0f cm, within a body"), BlasterAt30), BlasterAt30 < 25.0f);
	TestTrue(FString::Printf(TEXT("at 50 m up to %.0f cm, a body's half-width and more"), BlasterAt50), BlasterAt50 >= 34.0f);
	// The Munster Twin SMGs: 16 to 20 body hits a kill from the two guns together, a smaller magazine, still quicker than
	// the spawn primary with every hit.
	const int32 MunsterHits = ShotsToKill(Munster());
	TestTrue(FString::Printf(TEXT("the Munster Twin SMGs kill in %d body hits"), MunsterHits), MunsterHits >= 16 && MunsterHits <= 20);
	TestTrue(FString::Printf(TEXT("a smaller magazine (%d, from 120), a whole number for each gun"), Munster().MagazineSize), Munster().MagazineSize < 120 && Munster().MagazineSize % 2 == 0);
	const float MunsterKills = float(Munster().MagazineSize) / MunsterHits;
	const float BlasterKills = float(Blaster().MagazineSize) / ShotsToKill(Blaster());
	const float CarbineKills = float(Carbine().MagazineSize) / ShotsToKill(Carbine());
	TestTrue(FString::Printf(TEXT("about as many kills a magazine as the other loadout guns (%.1f; the Blaster %.1f, the Carbine %.1f)"), MunsterKills, BlasterKills, CarbineKills),
		MunsterKills <= FMath::Max(BlasterKills, CarbineKills) * 1.1f && MunsterKills >= FMath::Min(BlasterKills, CarbineKills) * 0.9f);
	TestTrue(FString::Printf(TEXT("a kill with every hit quicker than the Colby Carbine's (%.2f s, %.2f s)"), TimeToKill(Munster()), TimeToKill(Carbine())), TimeToKill(Munster()) < TimeToKill(Carbine()));
	// The Swiss Shotgun: a heavy kick each shot, as the Roquefort's.
	const FWeaponStats& Shotgun = CheeseStrikeWeapons::Shotgun();
	TestTrue(FString::Printf(TEXT("the shotgun's view thrown up %.1f degrees a shot"), Shotgun.KickPitch), Shotgun.KickPitch >= 6.0f && Shotgun.KickKept > 0.0f && Shotgun.KickKept < Shotgun.KickPitch);
	TestTrue(FString::Printf(TEXT("and the gun in view thrown back %.0f cm, further than the Roquefort's"), Shotgun.KickBack), Shotgun.KickBack > Roquefort().KickBack);
	TestTrue(TEXT("all of it at once, eased back long before the next shot"), FMath::IsNearlyEqual(KickAt(Shotgun, KickRise), 1.0f, 0.001f) && KickAt(Shotgun, Shotgun.KickSeconds) == 0.0f && Shotgun.KickSeconds < Shotgun.FireInterval / 2.0f);
	TestTrue(TEXT("each shot the first of its run (so the kick, not the climb, is its recoil)"), Shotgun.FireInterval > Shotgun.RecoilReset && NextInRun(Shotgun, 0, Shotgun.FireInterval) == 0);
	// The Cheese Stick: swings, topped up by a stand or the floor, never by a kill; spent, the sidearm or the knife.
	const FWeaponStats& Stick = CheeseStrikeWeapons::Stick();
	TestEqual(TEXT("a stand tops the stick up with a whole set of swings"), CheeseStrikePickups::StandRounds(Stick), Stick.MagazineSize);
	TestEqual(TEXT("a gun's stand with its usual reserve"), CheeseStrikePickups::StandRounds(Blaster()), Blaster().Reserve);
	TestTrue(TEXT("a stick with swings to spare isn't full"), CheeseStrikePickups::PrimaryRounds(Stick, 5, 0) == FIntPoint(5, Stick.MagazineSize));
	const FIntPoint Whole = CheeseStrikePickups::PrimaryRounds(Stick, Stick.MagazineSize, 0);
	TestTrue(TEXT("whole, it is"), CheeseStrikePickups::RoundsFull(Whole.X, Whole.Y, -1, 0));
	const FIntPoint Endless = CheeseStrikePickups::PrimaryRounds(Stick, 0, -1);
	TestTrue(TEXT("endless (Gun Game), it is full"), CheeseStrikePickups::RoundsFull(Endless.X, Endless.Y, -1, 0));
	TestTrue(TEXT("a gun counts its reserve"), CheeseStrikePickups::PrimaryRounds(Blaster(), 3, 40) == FIntPoint(40, Blaster().ReserveMax));
	EWeaponSlot Slot = EWeaponSlot::Primary;
	TestTrue(TEXT("spent, the sidearm"), DrawWhenSpent(true, false, true, Slot) && Slot == EWeaponSlot::Sidearm);
	TestTrue(TEXT("the knife if the sidearm is dry"), DrawWhenSpent(true, true, true, Slot) && Slot == EWeaponSlot::Knife);
	TestTrue(TEXT("or may not be drawn"), DrawWhenSpent(false, false, true, Slot) && Slot == EWeaponSlot::Knife);
	TestFalse(TEXT("nothing if neither may be"), DrawWhenSpent(false, false, false, Slot));
	return true;
}

namespace WeaponTests
{
	/**
	 * A magazine of the Cheese Grater's held on someone `Distance` cm off, its shards flown as the game flies them (the
	 * arsenal's spread, recoil, homing pick and turn, falloff and supercombine tally): the shooter's eyes at the origin keep
	 * their aim on the target's chest where it is (no lead, the recoil left as it climbs); the target stands, or strafes at
	 * `Strafe` cm/s, turning back every 0.4 to 0.9 s. True if it kills.
	 */
	bool GraterMagazineKills(float Distance, bool bAimed, float Strafe, FRandomStream& Random)
	{
		using namespace CheeseStrikeWeapons;
		const FWeaponStats& Grater = CheeseStrikeWeapons::Grater();
		constexpr float Step = 1.0f / 120.0f;
		constexpr float Radius = 34.0f;
		constexpr float Height = 192.0f;
		// The eyes 160 cm over the floor; ACheeseStrikeCharacter::ChestAt is 20 cm over the capsule's middle.
		constexpr float FeetZ = -160.0f;
		constexpr float ChestZ = FeetZ + Height * 0.5f + 20.0f;
		float Y = 0.0f;
		float Velocity = Random.FRand() < 0.5f ? Strafe : -Strafe;
		float TurnAt = Random.FRandRange(0.4f, 0.9f);
		float Health = MaxHealth;
		FShardTally Tally;
		struct FShard
		{
			FVector At;
			FVector Flight;
			float FiredAt;
			bool bHoming;
		};
		TArray<FShard> Shards;
		int32 Fired = 0;
		const float End = Grater.MagazineSize * Grater.FireInterval + Grater.ProjectileLife + 0.1f;
		for (float Time = 0.0f; Time < End; Time += Step)
		{
			if (Strafe > 0.0f && Time >= TurnAt)
			{
				Velocity = -Velocity;
				TurnAt = Time + Random.FRandRange(0.4f, 0.9f);
			}
			Y += Velocity * Step;
			const FVector Feet(Distance, Y, FeetZ);
			const FVector Chest(Distance, Y, ChestZ);
			while (Fired < Grater.MagazineSize && Time >= Fired * Grater.FireInterval)
			{
				const FVector Aim = Turned(Chest.GetSafeNormal(), RecoilFor(Grater, Fired, bAimed));
				const FVector Way = Spread(Aim, SpreadFor(Grater, Fired, bAimed), Random);
				const bool bHoming = HomingPick(FVector::ZeroVector, Aim, {Chest}, Grater.HomingCone, Grater.HomingReach) == 0;
				Shards.Add({FVector::ZeroVector, Way * Grater.ProjectileSpeed, Time, bHoming});
				++Fired;
			}
			for (int32 Index = Shards.Num() - 1; Index >= 0; --Index)
			{
				FShard& Shard = Shards[Index];
				if (Time - Shard.FiredAt > Grater.ProjectileLife)
				{
					Shards.RemoveAt(Index);
					continue;
				}
				if (Shard.bHoming)
				{
					Shard.Flight = TurnToward(Shard.Flight, Chest - Shard.At, Grater.HomingTurn * Step);
				}
				const float Length = Shard.Flight.Size() * Step;
				const FVector Way = Shard.Flight.GetSafeNormal();
				const float Along = RayHitsCapsule(Shard.At, Way, Feet, Radius, Height);
				if (Along < 0.0f || Along > Length)
				{
					Shard.At += Shard.Flight * Step;
					continue;
				}
				const FVector Hit = Shard.At + Way * Along;
				const float Flown = Hit.Size();
				Health -= DamageAt(Grater, Flown, Hit.Z > FeetZ + Height - 30.0f);
				Shards.RemoveAt(Index);
				if ((CountsForSuper(Grater, Flown) && Tally.Add(Grater, Time)) || Health <= 0.0f)
				{
					return true;
				}
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheGraterKillsCloseNotFar, "CheeseStrike.Weapons.TheGraterKillsCloseNotFar", WeaponTests::Flags)

bool FTheGraterKillsCloseNotFar::RunTest(const FString& Parameters)
{
	// Wave 17 (the user's): easy to kill with at about 10 m, hard at about 20 m, essentially impossible beyond. A magazine
	// held on someone, a hundred times each way: standing or strafing at walking speed, from the hip and aimed.
	constexpr int32 Trials = 100;
	const auto Chance = [](float Distance, bool bAimed, float Strafe)
	{
		FRandomStream Random(1700 + int32(Distance) + (bAimed ? 7 : 0) + int32(Strafe));
		int32 Kills = 0;
		for (int32 Trial = 0; Trial < Trials; ++Trial)
		{
			Kills += WeaponTests::GraterMagazineKills(Distance, bAimed, Strafe, Random) ? 1 : 0;
		}
		return float(Kills) / Trials;
	};
	const float Walk = CheeseStrikeMovement::WalkSpeed;
	struct FCase
	{
		const TCHAR* Name;
		bool bAimed;
		float Strafe;
	};
	const FCase Cases[] = {{TEXT("from the hip at someone standing"), false, 0.0f}, {TEXT("aimed at someone standing"), true, 0.0f},
		{TEXT("from the hip at someone strafing"), false, Walk}, {TEXT("aimed at someone strafing"), true, Walk}};
	float BestAt20 = 0.0f;
	float MeanAt20 = 0.0f;
	for (const FCase& Case : Cases)
	{
		const float At10 = Chance(1000.0f, Case.bAimed, Case.Strafe);
		const float At20 = Chance(2000.0f, Case.bAimed, Case.Strafe);
		const float At30 = Chance(3000.0f, Case.bAimed, Case.Strafe);
		AddInfo(FString::Printf(TEXT("A magazine %s kills at 10 m %.0f%%, at 20 m %.0f%%, at 30 m %.0f%%"), Case.Name, At10 * 100.0f, At20 * 100.0f, At30 * 100.0f));
		TestTrue(FString::Printf(TEXT("%s at 10 m: nearly always a kill (%.0f%%)"), Case.Name, At10 * 100.0f), At10 >= 0.9f);
		TestTrue(FString::Printf(TEXT("%s at 30 m: never (%.0f%%)"), Case.Name, At30 * 100.0f), At30 == 0.0f);
		BestAt20 = FMath::Max(BestAt20, At20);
		MeanAt20 += At20 / UE_ARRAY_COUNT(Cases);
	}
	TestTrue(FString::Printf(TEXT("at 20 m hard: at best (aimed, at someone standing) %.0f%%, %.0f%% over all four"), BestAt20 * 100.0f, MeanAt20 * 100.0f), BestAt20 <= 0.5f && MeanAt20 <= 0.15f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCarbineFiresInBurstsOfThree, "CheeseStrike.Weapons.TheCarbineFiresInBurstsOfThree", WeaponTests::Flags)

bool FTheCarbineFiresInBurstsOfThree::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Carbine = CheeseStrikeWeapons::Carbine();
	TestEqual(TEXT("three shots a pull"), Carbine.Burst, 3);
	TestFalse(TEXT("a pull at a time"), Carbine.bAutomatic);
	TestTrue(TEXT("the second follows the first by the burst's gap"), FMath::IsNearlyEqual(ShotTime(Carbine, 1), 0.07f));
	TestTrue(TEXT("the third by another"), FMath::IsNearlyEqual(ShotTime(Carbine, 2), 0.14f));
	TestTrue(TEXT("the next burst comes at the interval from the first"), FMath::IsNearlyEqual(ShotTime(Carbine, 3), 0.48f));
	TestTrue(TEXT("in a burst, the gap"), FMath::IsNearlyEqual(Cooldown(Carbine, 0), 0.07f) && FMath::IsNearlyEqual(Cooldown(Carbine, 1), 0.07f));
	TestTrue(TEXT("after it, the rest of the interval"), FMath::IsNearlyEqual(Cooldown(Carbine, 2), 0.48f - 0.14f));
	TestTrue(TEXT("the server holds it to three a pull"), FMath::IsNearlyEqual(ShotsPerSecond(Carbine), 3.0f / 0.48f));
	TestTrue(TEXT("a weapon without bursts waits its interval"), FMath::IsNearlyEqual(Cooldown(Blaster(), 0), Blaster().FireInterval) && FMath::IsNearlyEqual(ShotTime(Blaster(), 4), 4 * Blaster().FireInterval));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomaticFireKeepsAnExactBeat, "CheeseStrike.Weapons.AutomaticFireKeepsAnExactBeat", WeaponTests::Flags)

bool FAutomaticFireKeepsAnExactBeat::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	// Frames of every length from a fast PC's to a slow one's: over two seconds the shots are exactly as many as the beat has.
	for (const float Frame : {0.004f, 0.0167f, 0.033f, 0.05f, 0.11f})
	{
		for (const FWeaponStats* Weapon : {&Blaster(), &Munster()})
		{
			const float Interval = Weapon->FireInterval;
			float NextAt = Interval;
			int32 Shots = 0;
			int32 MostInAFrame = 0;
			float Now = 0.0f;
			while (Now < 2.0f)
			{
				Now += Frame;
				const int32 Due = ShotsDue(Now, NextAt, Interval);
				Shots += Due;
				MostInAFrame = FMath::Max(MostInAFrame, Due);
			}
			TestTrue(FString::Printf(TEXT("%s at %.0f ms frames fires %d shots in 2 s (%d)"), *Weapon->Id.ToString(), Frame * 1000.0f, FMath::FloorToInt(Now / Interval), Shots), FMath::Abs(Shots - FMath::FloorToInt(Now / Interval)) <= 1);
			if (Frame < Interval)
			{
				TestTrue(TEXT("and never two in a frame shorter than the interval, one past it"), MostInAFrame <= 1);
			}
		}
	}
	float NextAt = 0.1f;
	TestEqual(TEXT("a long frame owes two shots"), ShotsDue(0.1f + 2.5f * Blaster().FireInterval, NextAt, Blaster().FireInterval), 3);
	NextAt = 0.0f;
	TestEqual(TEXT("a hitch is caught up no further than four"), ShotsDue(5.0f, NextAt, Blaster().FireInterval), 4);
	TestTrue(TEXT("and the beat starts over from there"), NextAt > 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotsToldOfInClumpsAreShownOnTheBeat, "CheeseStrike.Weapons.ShotsToldOfInClumpsAreShownOnTheBeat", WeaponTests::Flags)

bool FShotsToldOfInClumpsAreShownOnTheBeat::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBeat;
	// A Munster's shots fired every 85 ms, told of in clumps at a network tick of 33 ms (three at a time, then none), as the net does.
	const double Interval = 0.085;
	TArray<double> Shown;
	double Last = -1000.0;
	for (int32 Shot = 0; Shot < 40; ++Shot)
	{
		const double Fired = Shot * Interval;
		const double Arrival = FMath::CeilToDouble(Fired / 0.033) * 0.033 + (Shot % 5 == 0 ? 0.04 : 0.0);
		Last = PaceShow(Arrival, Last, Interval, Interval, Interval * 4.0);
		Shown.Add(Last);
	}
	const FEvenness Even = Evenness(Shown);
	TestTrue(FString::Printf(TEXT("clumps and a late shot in every five are shown on the beat (%.1f to %.1f ms of 85)"), Even.Shortest * 1000.0, Even.Longest * 1000.0), Even.Shortest > Interval * 0.95 && Even.Longest < Interval * 1.05);
	TestTrue(TEXT("the first of a run a lead behind its arrival"), FMath::IsNearlyEqual(PaceShow(2.0, 0.5, Interval, Interval, Interval * 4.0), 2.0 + Interval));
	TestTrue(TEXT("one that arrives after a gap of more than two intervals starts a run again"), FMath::IsNearlyEqual(PaceShow(1.3, 1.1, Interval, Interval, Interval * 4.0), 1.3 + Interval));
	TestTrue(TEXT("one that arrives early is held to an interval after the last"), FMath::IsNearlyEqual(PaceShow(1.12, 1.1, Interval, Interval, Interval * 4.0), 1.1 + Interval));
	// A server that sends faster than the beat doesn't push the shown shots ever further behind: the run starts over.
	double Behind = -1000.0;
	double Furthest = 0.0;
	for (int32 Shot = 0; Shot < 200; ++Shot)
	{
		const double Arrival = Shot * Interval * 0.8;
		Behind = PaceShow(Arrival, Behind, Interval, Interval, Interval * 4.0);
		Furthest = FMath::Max(Furthest, Behind - Arrival);
	}
	TestTrue(FString::Printf(TEXT("and no more than four intervals behind (%.0f ms)"), Furthest * 1000.0), Furthest <= Interval * 4.0 + 0.0001);
	const FEvenness Single = Evenness({0.0, 0.1, 0.2});
	TestTrue(TEXT("evenness of a steady run"), Single.Intervals == 2 && FMath::IsNearlyEqual(Single.Mean, 0.1) && Single.Deviation < 0.0001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotSoundsFallOnTheSoundBeat, "CheeseStrike.Weapons.ShotSoundsFallOnTheSoundBeat", WeaponTests::Flags)

bool FShotSoundsFallOnTheSoundBeat::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeBeat;
	// Automatic fire held for 4 s, the shots shown on the game's frames (a long frame now and then), each shot's sound
	// given a tick (FSoundBeat::Take, as the weapon does), and the audio engine modelled as it was measured: it takes a
	// queued sound at the start of an audio buffer (1024 samples at 48 kHz), 15 ms to 20 ms and a frame after it was
	// handed over (22 to 54 ms at 60 frames a second, 35 to 65 ms at 30), and plays it on the tick named if that is
	// still ahead (else on a later multiple of it, a stray). Every shot's sound must be heard, each on a tick of its
	// own and the one planned: two on one tick are heard as one shot, the Munster's and the Blaster's skipped shots.
	struct FCase
	{
		const TCHAR* Name;
		double Interval;
		double Frame;
	};
	const FCase Cases[] = {
		{TEXT("the Munster at 120 fps"), 0.085, 1.0 / 120.0},
		{TEXT("the Munster at 60 fps"), 0.085, 1.0 / 60.0},
		{TEXT("the Munster at 30 fps"), 0.085, 1.0 / 30.0},
		{TEXT("the Munster at 20 fps"), 0.085, 1.0 / 20.0},
		{TEXT("the Cheddar Blaster at 60 fps"), 0.12, 1.0 / 60.0},
		{TEXT("the Cheddar Blaster at 30 fps"), 0.12, 1.0 / 30.0},
		{TEXT("the Grater at 30 fps"), 0.065, 1.0 / 30.0},
	};
	const double Buffer = 1024.0 / 48000.0;
	for (const FCase& Case : Cases)
	{
		for (int32 Seed = 1; Seed <= 4; ++Seed)
		{
			FRandomStream Random(Seed * 7919);
			FSoundBeat Beat;
			const double Interval = Case.Interval;
			// The clock was set a while ago; the game's estimate of its tick 0 is off by a few milliseconds.
			const double ClockOrigin = 3.0 + Random.FRandRange(0.0, Buffer);
			Beat.SetGrid(ClockOrigin + Random.FRandRange(-0.004, 0.004), Interval);
			const int32 Shots = FMath::FloorToInt(4.0 / Interval);
			TArray<int64> Planned;
			TArray<int64> Heard;
			double Furthest = 0.0;
			int32 Fired = 0;
			int32 Frames = 0;
			double Time = 10.0;
			while (Fired < Shots && Frames < 100000)
			{
				const double Frame = (++Frames % 11 == 0) ? Case.Frame * 2.5 : Case.Frame;
				Time += Frame;
				while (Fired < Shots && 10.0 + Fired * Interval <= Time)
				{
					++Fired;
					Beat.SetGrid(ClockOrigin + Random.FRandRange(-0.004, 0.004), Interval);
					const int64 Slot = Beat.Take(Time, FSoundBeat::LeadFor(Frame));
					Planned.Add(Slot);
					// When the audio thread takes it, and where Quartz puts it: tick Slot, or if that has gone, the next multiple of Slot + 1.
					const double Taken = FMath::CeilToDouble((Time + Random.FRandRange(0.015, 0.02 + Frame) - ClockOrigin) / Buffer) * Buffer + ClockOrigin;
					const int64 Come = static_cast<int64>(FMath::FloorToDouble((Taken - ClockOrigin) / Interval));
					const int64 Tick = Come < Slot ? Slot : (Come / (Slot + 1) + 1) * (Slot + 1) + Slot;
					Heard.Add(Tick);
					Furthest = FMath::Max(Furthest, ClockOrigin + Tick * Interval - Time);
				}
			}
			const FString Name = FString::Printf(TEXT("%s (run %d)"), Case.Name, Seed);
			TestEqual(FString::Printf(TEXT("%s: every shot's sound is heard, none dropped"), *Name), Heard.Num(), Shots);
			TestTrue(FString::Printf(TEXT("%s: each on the tick planned"), *Name), Heard == Planned);
			int32 Shared = 0;
			int32 OneBeat = 0;
			for (int32 Index = 1; Index < Heard.Num(); ++Index)
			{
				Shared += Heard[Index] <= Heard[Index - 1] ? 1 : 0;
				OneBeat += Heard[Index] - Heard[Index - 1] == 1 ? 1 : 0;
			}
			TestEqual(FString::Printf(TEXT("%s: no two shots on one tick"), *Name), Shared, 0);
			TestTrue(FString::Printf(TEXT("%s: nearly all one beat apart (%d of %d)"), *Name, OneBeat, Heard.Num() - 1), OneBeat >= FMath::CeilToInt((Heard.Num() - 1) * 0.85));
			TestTrue(FString::Printf(TEXT("%s: none heard long after it was shown (%.0f ms)"), *Name, Furthest * 1000.0), Furthest < FSoundBeat::LeadFor(Case.Frame * 2.5) + 2.0 * Interval + Case.Frame * 2.5);
		}
	}
	{
		FSoundBeat Beat;
		Beat.SetGrid(10.0, 0.1);
		TestEqual(TEXT("a sound takes the first tick far enough ahead"), Beat.Take(10.0, 0.05), static_cast<int64>(1));
		TestEqual(TEXT("and the next the one after, however soon it comes"), Beat.Take(10.001, 0.05), static_cast<int64>(2));
		TestEqual(TEXT("or a later one"), Beat.Take(10.6, 0.05), static_cast<int64>(7));
		Beat.SetGrid(10.6, 0.085);
		TestEqual(TEXT("a new interval counts afresh"), Beat.Take(10.6, 0.05), static_cast<int64>(1));
		TestTrue(TEXT("the lead grows with the frame"), FSoundBeat::LeadFor(1.0 / 30.0) > FSoundBeat::LeadFor(1.0 / 60.0) && FSoundBeat::LeadFor(1.0 / 60.0) >= 0.055);
	}
	// Quartz is told the tick by its count from when the clock was set (transport-relative: the next multiple of Slot + 1,
	// which is tick Slot while it is ahead), not the next tick from whenever the audio thread takes it, nor the bar's
	// first tick (the next bar line, 32 ticks off); and the clock's count starts again at once, not at the next bar.
	const FQuartzQuantizationBoundary Boundary = AtTick(5);
	TestTrue(TEXT("a sound is queued for its tick by number"), Boundary.Quantization == EQuartzCommandQuantization::Tick && Boundary.Multiplier == 6.0f
		&& Boundary.CountingReferencePoint == EQuarztQuantizationReference::TransportRelative && !Boundary.bFireOnClockStart);
	TestTrue(TEXT("the clock is set at once"), AtOnce().Quantization == EQuartzCommandQuantization::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheShotgunFiresTwelvePellets, "CheeseStrike.Weapons.TheShotgunFiresTwelvePellets", WeaponTests::Flags)

bool FTheShotgunFiresTwelvePellets::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Shotgun = CheeseStrikeWeapons::Shotgun();
	TestEqual(TEXT("twelve pellets"), Shotgun.Pellets, 12);
	TestEqual(TEXT("14 each, close"), DamageAt(Shotgun, 100.0f, false), 14.0f);
	TestEqual(TEXT("and still at 4 m"), DamageAt(Shotgun, 400.0f, false), 14.0f);
	TestTrue(TEXT("all twelve kill at 4 m"), DamageAt(Shotgun, 400.0f, false) * Shotgun.Pellets >= MaxHealth);
	// Wave 15: a wider cone (9 degrees aimed, 14 from the hip) and a slower shot (1.19 s, 1.4 times what it was). At 4 m
	// its cone is still about a body wide from the hip, so nearly all land: a kill a shot.
	TestTrue(TEXT("9 degrees aimed, 14 from the hip"), Shotgun.SpreadDegrees == 9.0f && Shotgun.HipSpreadDegrees == 14.0f);
	TestTrue(FString::Printf(TEXT("a shot every 1.19 s (%.2f)"), Shotgun.FireInterval), FMath::IsNearlyEqual(Shotgun.FireInterval, 0.85f * 1.4f, 0.001f));
	const float Across4 = FMath::Tan(FMath::DegreesToRadians(Shotgun.HipSpreadDegrees * 0.5f)) * 400.0f;
	TestTrue(FString::Printf(TEXT("from the hip its pellets are within %.0f cm of the aim at 4 m: about a body's width"), Across4), Across4 <= CheeseStrikeMovement::CapsuleRadius * 1.5f);
	TestTrue(FString::Printf(TEXT("not a kill past 8 m even if all twelve land (%.0f)"), DamageAt(Shotgun, 800.0f, false) * Shotgun.Pellets), DamageAt(Shotgun, 800.0f, false) * Shotgun.Pellets < MaxHealth);
	const float Across8 = FMath::Tan(FMath::DegreesToRadians(Shotgun.HipSpreadDegrees * 0.5f)) * 800.0f;
	TestTrue(FString::Printf(TEXT("and many miss there (%.0f cm across a body %.0f wide)"), Across8, CheeseStrikeMovement::CapsuleRadius), Across8 > CheeseStrikeMovement::CapsuleRadius * 1.5f);
	TestTrue(TEXT("0.15 of it from 12 m"), FMath::IsNearlyEqual(DamageAt(Shotgun, 1200.0f, false), 14.0f * 0.15f, 0.01f) && FMath::IsNearlyEqual(DamageAt(Shotgun, 1300.0f, false), 14.0f * 0.15f, 0.01f));
	// The pellets are spread by the shooter's seed, so the server makes the same twelve.
	FRandomStream Shooter(1234);
	FRandomStream Server(1234);
	const float Cone = SpreadFor(Shotgun, 0, false);
	TestEqual(TEXT("from the hip, the whole cone at once (no bloom)"), Cone, Shotgun.HipSpreadDegrees);
	int32 Apart = 0;
	FVector Last = FVector::ZeroVector;
	for (int32 Pellet = 0; Pellet < Shotgun.Pellets; ++Pellet)
	{
		const FVector Mine = Spread(FVector::ForwardVector, Cone, Shooter);
		const FVector Theirs = Spread(FVector::ForwardVector, Cone, Server);
		TestTrue(FString::Printf(TEXT("pellet %d goes the same way for both"), Pellet), Mine.Equals(Theirs, 0.0001f));
		Apart += Pellet > 0 && !Mine.Equals(Last, 0.001f) ? 1 : 0;
		Last = Mine;
	}
	TestEqual(TEXT("each its own way"), Apart, Shotgun.Pellets - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheKnifeReachesAndStabsInTheBack, "CheeseStrike.Weapons.TheKnifeReachesAndStabsInTheBack", WeaponTests::Flags)

bool FTheKnifeReachesAndStabsInTheBack::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Knife = CheeseStrikeWeapons::Knife();
	TestTrue(TEXT("it is the knife's slot"), Knife.IsMelee() && Knife.Slot == EWeaponSlot::Knife);
	TestTrue(FString::Printf(TEXT("it reaches the design's 4.5 metres and a step (it swipes rather than lunges): 180 cm (%.0f)"), Knife.Reach), FMath::IsNearlyEqual(Knife.Reach, 180.0f, 0.5f));
	TestEqual(TEXT("55 from the front"), Knife.Damage, 55.0f);
	TestFalse(TEXT("it fires no rounds"), Knife.UsesRounds());

	const TArray<FVector> Fan = SweepDirections(Knife, FVector::ForwardVector);
	TestTrue(TEXT("straight first"), Fan.Num() > 0 && Fan[0].Equals(FVector::ForwardVector, 0.001f));
	// The swipe's ways, by how far round each is (right positive): out to the sweep either side, nearer ones first, none far apart.
	TArray<float> Angles;
	float Last = 0.0f;
	bool bNearerFirst = true;
	for (const FVector& Way : Fan)
	{
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Way.Y, Way.X));
		bNearerFirst &= FMath::Abs(Angle) >= Last - 0.01f;
		Last = FMath::Abs(Angle);
		Angles.Add(Angle);
		TestTrue(TEXT("flat"), FMath::IsNearlyZero(Way.Z, 0.001f));
	}
	Angles.Sort();
	TestTrue(TEXT("nearer ways are tried first"), bNearerFirst);
	TestTrue(FString::Printf(TEXT("sweeping 45 degrees either side (%.1f to %.1f)"), Angles[0], Angles.Last()), FMath::IsNearlyEqual(Angles[0], -45.0f, 0.1f) && FMath::IsNearlyEqual(Angles.Last(), 45.0f, 0.1f));
	float Gap = 0.0f;
	for (int32 Each = 1; Each < Angles.Num(); ++Each)
	{
		Gap = FMath::Max(Gap, Angles[Each] - Angles[Each - 1]);
	}
	// At its reach, a gap no wider than a body (the capsule's 68 cm) between the ways tried.
	const float Between = 2.0f * Knife.Reach * FMath::Sin(FMath::DegreesToRadians(Gap) * 0.5f);
	TestTrue(FString::Printf(TEXT("with no gap a body could stand in (%.0f degrees, %.0f cm at its reach)"), Gap, Between), Between < 2.0f * CheeseStrikeMovement::CapsuleRadius);

	// Someone facing +X, struck going +X, is struck in the back.
	TestTrue(TEXT("a stroke the way they face is in the back"), FromBehind(FVector::ForwardVector, 0.0f));
	TestTrue(TEXT("from a little to one side too"), FromBehind(FVector(1.0f, 0.5f, 0.0f).GetSafeNormal(), 0.0f));
	TestFalse(TEXT("but not from the side"), FromBehind(FVector::RightVector, 0.0f));
	TestFalse(TEXT("nor the front"), FromBehind(FVector::BackwardVector, 0.0f));
	TestTrue(TEXT("whichever way they face"), FromBehind(FVector::RightVector, 90.0f));
	TestTrue(TEXT("in the back, it kills at a stroke"), StrokeDamage(Knife, FVector::ForwardVector, 0.0f) >= MaxHealth);
	TestEqual(TEXT("from the front, two strokes"), StrokeDamage(Knife, FVector::BackwardVector, 0.0f), 55.0f);
	TestTrue(TEXT("in a quarter of a second"), FMath::IsNearlyEqual(Knife.SwingTime, 0.25f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponsAreLifeSizeAndComeUpInTime, "CheeseStrike.Weapons.WeaponsAreLifeSizeAndComeUpInTime", WeaponTests::Flags)

bool FWeaponsAreLifeSizeAndComeUpInTime::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	struct FReach
	{
		const FWeaponStats& Weapon;
		float Range;
		float FalloffStart;
		float FalloffEnd;
	};
	// The design's Godot metres, a third of a metre each here.
	for (const FReach& Wanted : {FReach{Pistol(), 90, 25, 60}, FReach{Blaster(), 150, 40, 90}, FReach{Carbine(), 140, 35, 80},
		FReach{Munster(), 80, 15, 45}, FReach{Roquefort(), 250, 0, 0}, FReach{Burratas(), 90, 20, 55}})
	{
		const FString Name = Wanted.Weapon.DisplayName.ToString();
		TestTrue(FString::Printf(TEXT("%s reaches %.0f m (%.0f cm)"), *Name, Wanted.Range / 3.0f, Wanted.Weapon.MaxRange), FMath::IsNearlyEqual(Wanted.Weapon.MaxRange, Wanted.Range * WeaponTests::Metre, 1.0f));
		TestTrue(FString::Printf(TEXT("%s falls off from %.0f to %.0f m"), *Name, Wanted.FalloffStart / 3.0f, Wanted.FalloffEnd / 3.0f),
			FMath::IsNearlyEqual(Wanted.Weapon.FalloffStart, Wanted.FalloffStart * WeaponTests::Metre, 1.0f) && FMath::IsNearlyEqual(Wanted.Weapon.FalloffEnd, Wanted.FalloffEnd * WeaponTests::Metre, 1.0f));
	}
	// The Swiss Shotgun: the whole of each pellet to 4 m, 0.15 of it from 12 m, nothing past 13 m (the design's 40 Godot metres).
	TestTrue(TEXT("the Swiss Shotgun falls off from 4 m to 12 m"), Shotgun().FalloffStart == 400.0f && Shotgun().FalloffEnd == 1200.0f && Shotgun().FalloffFloor == 0.15f && FMath::IsNearlyEqual(Shotgun().MaxRange, 40.0f * WeaponTests::Metre, 1.0f));
	// Coming up: the usual third of a second, the BabyBomb slow and the sniper's rifle slower than most.
	TestEqual(TEXT("the BabyBomb takes 0.7 s to draw"), BabyBomb().DrawTime, 0.7f);
	TestEqual(TEXT("and slows its carrier to 0.85"), BabyBomb().CarrySpeed, CheeseStrikeMovement::HeavyFactor);
	TestEqual(TEXT("the Cheese Stick to 0.9"), Stick().CarrySpeed, CheeseStrikeMovement::LongFactor);
	TestTrue(TEXT("which the moves carry as heavy and long"), CheeseStrikeMovement::IsHeavy(BabyBomb().CarrySpeed) && !CheeseStrikeMovement::IsLong(BabyBomb().CarrySpeed)
		&& CheeseStrikeMovement::IsLong(Stick().CarrySpeed) && !CheeseStrikeMovement::IsHeavy(Stick().CarrySpeed) && !CheeseStrikeMovement::IsHeavy(1.0f) && !CheeseStrikeMovement::IsLong(1.0f));
	TestEqual(TEXT("the Roquefort 0.5 s"), Roquefort().DrawTime, 0.5f);
	TestTrue(TEXT("and is scoped"), Roquefort().bScoped);
	TestFalse(TEXT("drawn just now, it can't fire"), IsDrawn(BabyBomb(), 10.0f, 10.5f));
	TestTrue(TEXT("once the draw time is past, it can"), IsDrawn(BabyBomb(), 10.0f, 10.75f));
	TestFalse(TEXT("not a little before"), IsDrawn(BabyBomb(), 10.0f, 10.6f));
	TestTrue(TEXT("though the server allows a shot a little early, for the network"), IsDrawn(BabyBomb(), 10.0f, 10.6f, DrawAllowance));
	TestFalse(TEXT("but not much"), IsDrawn(BabyBomb(), 10.0f, 10.5f, DrawAllowance));
	TestTrue(TEXT("put in hand at once, it can fire at once"), IsDrawn(BabyBomb(), -1000.0f, 0.0f));
	// Carrying.
	TestTrue(TEXT("a heavy weapon slows its carrier"), FMath::IsNearlyEqual(CheeseStrikeMovement::WithWeapon(450.0f, true), 450.0f * 0.85f));
	TestTrue(TEXT("a long one less"), FMath::IsNearlyEqual(CheeseStrikeMovement::WithWeapon(450.0f, false, true), 450.0f * 0.9f));
	TestEqual(TEXT("another doesn't"), CheeseStrikeMovement::WithWeapon(675.0f, false), 675.0f);
	// Rounds (wave 16: the Curd Pistol too).
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		// Not the Cheese Stick's swings (wave 17), which have no reserve.
		if ((Weapon->Slot == EWeaponSlot::Primary || Weapon->Slot == EWeaponSlot::Sidearm) && !Weapon->bSpecial && !Weapon->HasSwings())
		{
			TestTrue(FString::Printf(TEXT("the %s carries a magazine or three besides and holds at most about five"), *Weapon->DisplayName.ToString()),
				Weapon->Reserve >= Weapon->MagazineSize && Weapon->Reserve <= 3 * Weapon->MagazineSize && Weapon->ReserveMax <= 6 * Weapon->MagazineSize);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheArsenalIsListedAndFound, "CheeseStrike.Weapons.TheArsenalIsListedAndFound", WeaponTests::Flags)

bool FTheArsenalIsListedAndFound::RunTest(const FString& Parameters)
{
	const TArray<const FWeaponStats*>& All = CheeseStrikeArsenal::All();
	TestEqual(TEXT("twelve weapons, the Gouda Gatling among them"), All.Num(), 12);
	TSet<FName> Ids;
	for (const FWeaponStats* Weapon : All)
	{
		TestFalse(FString::Printf(TEXT("%s has an id of its own"), *Weapon->Id.ToString()), Ids.Contains(Weapon->Id));
		Ids.Add(Weapon->Id);
		TestTrue(FString::Printf(TEXT("%s is a cheese"), *Weapon->DisplayName.ToString()), !Weapon->DisplayName.IsEmpty());
		TestTrue(TEXT("and is found by its id"), CheeseStrikeArsenal::Find(Weapon->Id) == Weapon);
	}
	TestTrue(TEXT("found whatever the case"), CheeseStrikeArsenal::Find(FName(TEXT("ShotGun"))) == &CheeseStrikeWeapons::Shotgun());
	TestNull(TEXT("and nothing that isn't one"), CheeseStrikeArsenal::Find(FName(TEXT("bazooka"))));
	TestEqual(TEXT("nine primaries to pick, the streak's Gatling not among them"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary).Num(), 9);
	TestTrue(TEXT("the BabyBomb and the Cheese Stick among them"), CheeseStrikeArsenal::IsPickable(CheeseStrikeWeapons::BabyBomb().Id, EWeaponSlot::Primary) && CheeseStrikeArsenal::IsPickable(CheeseStrikeWeapons::Stick().Id, EWeaponSlot::Primary));
	TestEqual(TEXT("the Grater keeps its id"), CheeseStrikeWeapons::Grater().Id, FName(TEXT("grater")));
	TestEqual(TEXT("the Curd Pistol is the sidearm"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Sidearm), TArray<FName>{CheeseStrikeArsenal::Sidearm()});
	TestEqual(TEXT("everyone comes in with the Colby Carbine"), CheeseStrikeArsenal::DefaultPrimary(), CheeseStrikeWeapons::Carbine().Id);

	// Gun Game: through the guns a kill at a time, the knife last.
	const TArray<FName>& Ladder = CheeseStrikeArsenal::GunGameLadder();
	TestEqual(TEXT("Gun Game works through every weapon but the streaks'"), Ladder.Num(), All.FilterByPredicate([](const FWeaponStats* Weapon) { return !Weapon->bSpecial; }).Num());
	TestEqual(TEXT("from the Cheddar Blaster"), CheeseStrikeArsenal::GunGameWeapon(0), CheeseStrikeWeapons::Blaster().Id);
	TestEqual(TEXT("to the Curd Pistol"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 2), CheeseStrikeWeapons::Pistol().Id);
	TestEqual(TEXT("and last the knife"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 1), CheeseStrikeWeapons::Knife().Id);
	TestEqual(TEXT("which it keeps"), CheeseStrikeArsenal::GunGameWeapon(100), CheeseStrikeWeapons::Knife().Id);
	TestTrue(TEXT("the Dual Burratas are on it, after the Colby Carbine"), Ladder.IndexOfByKey(CheeseStrikeWeapons::Burratas().Id) == Ladder.IndexOfByKey(CheeseStrikeWeapons::Carbine().Id) + 1);
	TestTrue(TEXT("the BabyBomb after the Roquefort, the Cheese Stick before the pistol"), Ladder.IndexOfByKey(CheeseStrikeWeapons::BabyBomb().Id) == Ladder.IndexOfByKey(CheeseStrikeWeapons::Roquefort().Id) + 1
		&& Ladder.IndexOfByKey(CheeseStrikeWeapons::Stick().Id) == Ladder.IndexOfByKey(CheeseStrikeWeapons::Pistol().Id) - 1);
	for (const FName& Id : Ladder)
	{
		TestNotNull(FString::Printf(TEXT("%s is a weapon"), *Id.ToString()), CheeseStrikeArsenal::Find(Id));
	}

	// Loadouts.
	const FCheeseStrikeLoadout Usual = FCheeseStrikeLoadout::Usual();
	TestEqual(TEXT("everyone carries a primary"), Usual.Primary, CheeseStrikeArsenal::DefaultPrimary());
	TestEqual(TEXT("the sidearm"), Usual.Sidearm, CheeseStrikeArsenal::Sidearm());
	TestEqual(TEXT("and the knife"), Usual.Knife, CheeseStrikeArsenal::Knife());
	TestEqual(TEXT("the primary chosen"), FCheeseStrikeLoadout::Usual(TEXT("roquefort")).Primary, CheeseStrikeWeapons::Roquefort().Id);
	TestEqual(TEXT("which must be a primary"), FCheeseStrikeLoadout::Usual(TEXT("pistol")).Primary, CheeseStrikeArsenal::DefaultPrimary());
	TestEqual(TEXT("one a player may pick"), FCheeseStrikeLoadout::Usual(TEXT("gatling")).Primary, CheeseStrikeArsenal::DefaultPrimary());
	const FCheeseStrikeLoadout Only = FCheeseStrikeLoadout::Only(TEXT("knife"));
	TestTrue(TEXT("one weapon alone, in its slot"), Only.Knife == CheeseStrikeArsenal::Knife() && Only.Primary.IsNone() && Only.Sidearm.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFarShotIsQuieterAndDuller, "CheeseStrike.Weapons.AFarShotIsQuieterAndDuller", WeaponTests::Flags)

bool FAFarShotIsQuieterAndDuller::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeGunfire;
	// Heard through the engine's own sums for the settings the shots play with.
	const float Close = Volume(200.0f);
	const float Ten = Volume(1000.0f);
	const float Thirty = Volume(3000.0f);
	const float Fifty = Volume(5000.0f);
	const float Beyond = Volume(FullWithin + FalloffDistance + 100.0f);
	AddInfo(FString::Printf(TEXT("volume at 2 m %.2f, 10 m %.2f, 30 m %.2f, 50 m %.3f, past the end %.3f; cut above %.0f Hz at 10 m, %.0f at 30 m, %.0f at 50 m"), Close, Ten, Thirty, Fifty, Beyond, LowPass(1000.0f), LowPass(3000.0f), LowPass(5000.0f)));
	TestEqual(TEXT("a shot close by is whole"), Close, 1.0f);
	TestTrue(TEXT("one 10 m off is quieter"), Ten < Close && Ten > 0.4f);
	TestTrue(TEXT("one 30 m off quieter still"), Thirty < Ten * 0.6f);
	TestTrue(TEXT("one 50 m off faint"), Fifty < 0.1f && Fifty > 0.0f);
	TestEqual(TEXT("and past the falloff, silent"), Beyond, 0.0f);
	TestEqual(TEXT("close by, nothing is cut"), LowPass(500.0f), BrightHertz);
	TestTrue(TEXT("further off it is duller"), LowPass(3000.0f) < 8000.0f && LowPass(3000.0f) > DullestHertz);
	TestEqual(TEXT("and dullest far away"), LowPass(10000.0f), DullestHertz);
	const FSoundAttenuationSettings Settings = Attenuation();
	TestTrue(TEXT("it is heard from where it was fired"), Settings.bAttenuate && Settings.bSpatialize && Settings.bAttenuateWithLPF);
	if (TestNotNull(TEXT("the shots' attenuation"), AttenuationAsset()) && TestNotNull(TEXT("and their limit"), ConcurrencyAsset()))
	{
		TestEqual(TEXT("so many shots at once at most"), ConcurrencyAsset()->Concurrency.GetMaxCount(), MostAtOnce);
		TestTrue(TEXT("the furthest giving way"), ConcurrencyAsset()->Concurrency.ResolutionRule == EMaxConcurrentResolutionRule::StopFarthestThenOldest);
		TestTrue(TEXT("one and the same for every weapon"), AttenuationAsset() == AttenuationAsset() && ConcurrencyAsset() == ConcurrencyAsset());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDualBurratasFireInTurn, "CheeseStrike.Weapons.TheDualBurratasFireInTurn", WeaponTests::Flags)

bool FTheDualBurratasFireInTurn::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Dual = Burratas();
	TestTrue(TEXT("two guns"), Dual.bDual);
	TestFalse(TEXT("which no other weapon has"), Blaster().bDual || Pistol().bDual || Grater().bDual);
	TestEqual(TEXT("a primary"), Dual.Slot, EWeaponSlot::Primary);
	// The right, then the left, in turn; one gun is always the right.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestEqual(FString::Printf(TEXT("shot %d from the %s"), Index, Index % 2 == 0 ? TEXT("right") : TEXT("left")), DualHand(Dual, Index), Index % 2);
	}
	TestEqual(TEXT("a pistol fires from the right"), DualHand(Pistol(), 1), 0);
	// Each gun kicks to its own side.
	TestTrue(TEXT("the right gun's kick goes right"), RecoilFor(Dual, 4, false).X > 0.0f);
	TestTrue(TEXT("the left gun's left"), RecoilFor(Dual, 5, false).X < 0.0f);
	TestTrue(TEXT("and both climb"), RecoilFor(Dual, 5, false).Y > RecoilFor(Dual, 4, false).Y);
	// Aimed (wave 15: both guns up side by side before the eyes, a little converged), a tighter cone, as other weapons get;
	// no scope, and the eye between the two (the look's Sight half the apart to the right gun's left).
	for (const FWeaponStats* Two : {&Dual, &Munster()})
	{
		const FString Name = Two->DisplayName.ToString();
		TestTrue(FString::Printf(TEXT("%s: aimed, a tighter cone than from the hip"), *Name), Two->SpreadDegrees > 0.0f && Two->SpreadDegrees < Two->HipSpreadDegrees / 3.0f);
		TestTrue(FString::Printf(TEXT("%s: no scope, a mild zoom"), *Name), !Two->bScoped && Two->AimZoom < 1.0f && Two->AimZoom >= 0.75f);
		const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Two->Id);
		TestTrue(FString::Printf(TEXT("%s: the eye between the two guns"), *Name), Look.Sight.IsSet() && FMath::IsNearlyEqual(Look.Sight->X, CheeseStrikeDual::AimHalfApart) && !Look.LeftHand.IsSet());
		TestTrue(FString::Printf(TEXT("%s: aimed, shot 3 within %.1f degrees"), *Name, Two->SpreadDegrees), SpreadFor(*Two, 3, true) < SpreadFor(*Two, 3, false) && SpreadFor(*Two, Two->BloomShots, true) == Two->SpreadDegrees);
	}
	TestTrue(TEXT("a weapon of one gun aims down its middle"), FMath::IsNearlyZero(CheeseStrikeWeaponLooks::For(Blaster().Id).Sight->X, 0.5f));
	TestTrue(TEXT("aimed, the two either side of the crosshair, the hands clear of it, turned in a few degrees; at the hip about a shoulder's width apart"), CheeseStrikeDual::AimHalfApart >= 9.0f && CheeseStrikeDual::AimHalfApart <= 14.0f && CheeseStrikeDual::Converge >= 1.0f && CheeseStrikeDual::Converge <= 6.0f
		&& CheeseStrikeDual::AimSpread < CheeseStrikeDual::HipHalfApart && CheeseStrikeDual::HipHalfApart >= 15.0f && CheeseStrikeDual::HipHalfApart <= 25.0f);
	// Aligned: the body faces +Y (its right is -X); a hand whose gun lies along the hand's Y (as a weapon rides the hand:
	// the mirror above keeps a frame's Y and Z and turns its X back) points 15 degrees right of that and 10 up; it is
	// turned, in place, so that the gun points the forward's way turned in to the left by the angle, still 10 up; mirrored
	// across the body's middle, the left hand's gun points as much to the right.
	const FVector SkewAlong = FRotator(10.0f, 105.0f, 0.0f).Vector();
	const FTransform Askew(FRotationMatrix::MakeFromY(SkewAlong).ToQuat(), FVector(-10.0f, 40.0f, 130.0f));
	TestTrue(TEXT("the test's hand holds its gun 15 right of forward"), Askew.GetUnitAxis(EAxis::Y).Equals(SkewAlong, 0.001f));
	const FTransform In = CheeseStrikeDual::Aligned(Askew, FVector::YAxisVector, FVector::YAxisVector, FVector::UpVector, CheeseStrikeDual::Converge);
	const FVector InAlong = In.GetUnitAxis(EAxis::Y);
	const float InYaw = FMath::RadiansToDegrees(FMath::Atan2(InAlong.Y, InAlong.X));
	const float InPitch = FMath::RadiansToDegrees(FMath::Asin(InAlong.Z));
	TestTrue(TEXT("aligned in place"), In.GetLocation().Equals(Askew.GetLocation()));
	TestTrue(FString::Printf(TEXT("the right gun laid %.0f degrees left of forward (yaw %.1f), still 10 up (%.1f)"), CheeseStrikeDual::Converge, InYaw, InPitch),
		InAlong.X > 0.0f && FMath::IsNearlyEqual(InYaw, 90.0f - CheeseStrikeDual::Converge, 0.05f) && FMath::IsNearlyEqual(InPitch, 10.0f, 0.05f));
	const FVector LeftAlong = CheeseStrikeDual::Mirrored(In, FVector::ZeroVector, FVector::XAxisVector).GetUnitAxis(EAxis::Y);
	TestTrue(TEXT("and the left gun, mirrored, as much to the right"), LeftAlong.X < 0.0f && FMath::IsNearlyEqual(LeftAlong.X, -InAlong.X, 0.001f) && FMath::IsNearlyEqual(LeftAlong.Y, InAlong.Y, 0.001f) && FMath::IsNearlyEqual(LeftAlong.Z, InAlong.Z, 0.001f));
	TestTrue(TEXT("a hand already aligned is left as it is"), CheeseStrikeDual::Aligned(In, FVector::YAxisVector, FVector::YAxisVector, FVector::UpVector, CheeseStrikeDual::Converge).GetRotation().Equals(In.GetRotation(), 0.0001f));
	// A shot a pull (as Counter-Strike's are), no more than ten a second however fast the pulls; a long magazine, a long
	// reload, middling damage.
	TestFalse(TEXT("not automatic: a shot a pull"), Dual.bAutomatic);
	TestEqual(TEXT("one shot to a pull, no burst"), Dual.Burst, 1);
	TestTrue(FString::Printf(TEXT("pulled as fast as can be, %.0f a second"), ShotsPerSecond(Dual)), ShotsPerSecond(Dual) <= 10.0f + KINDA_SMALL_NUMBER && ShotsPerSecond(Dual) >= 8.0f);
	TestTrue(TEXT("faster than the Cheddar Blaster"), ShotsPerSecond(Dual) > ShotsPerSecond(Blaster()));
	TestEqual(TEXT("thirty rounds"), Dual.MagazineSize, 30);
	TestTrue(TEXT("a longer magazine than the Cheddar Blaster's"), Dual.MagazineSize > Blaster().MagazineSize);
	TestTrue(TEXT("a reload longer than any but the Munster's"), Dual.ReloadTime > Roquefort().ReloadTime && Dual.ReloadTime < Munster().ReloadTime);
	TestTrue(TEXT("a hit over the pistol's, about the Cheddar Blaster's"), Dual.Damage > Pistol().Damage && FMath::Abs(Dual.Damage - Blaster().Damage) <= 1.0f);
	// The left hand held the mirror way round: across the body's middle, a hand's frame still a hand's (its axes along
	// and up mirrored, its axis across turned back), so the gun it holds keeps its handedness.
	const FTransform Right(FRotator(10.0f, 30.0f, 5.0f), FVector(-20.0f, 40.0f, 130.0f));
	const FTransform Left = CheeseStrikeDual::Mirrored(Right, FVector::ZeroVector, FVector::XAxisVector);
	const auto Reflect = [](const FVector& V) { return FVector(-V.X, V.Y, V.Z); };
	TestTrue(TEXT("the other side of the body"), Left.GetLocation().Equals(FVector(20.0f, 40.0f, 130.0f), 0.01f));
	TestTrue(TEXT("its along and up mirrored"), Left.GetUnitAxis(EAxis::Y).Equals(Reflect(Right.GetUnitAxis(EAxis::Y)), 0.001f) && Left.GetUnitAxis(EAxis::Z).Equals(Reflect(Right.GetUnitAxis(EAxis::Z)), 0.001f));
	TestTrue(TEXT("and still a turn, not a mirror"), Left.GetUnitAxis(EAxis::X).Equals(-Reflect(Right.GetUnitAxis(EAxis::X)), 0.001f));
	TestTrue(TEXT("across a plane off the middle too"), CheeseStrikeDual::Mirrored(Right, FVector(10.0f, 0.0f, 0.0f), FVector::XAxisVector).GetLocation().Equals(FVector(40.0f, 40.0f, 130.0f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheBabyBombBurstsAndHurtsItsShooter, "CheeseStrike.Weapons.TheBabyBombBurstsAndHurtsItsShooter", WeaponTests::Flags)

bool FTheBabyBombBurstsAndHurtsItsShooter::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Bomb = BabyBomb();
	TestTrue(TEXT("a projectile"), Bomb.IsProjectile() && !Blaster().IsProjectile());
	TestEqual(TEXT("two in the tube"), Bomb.MagazineSize, 2);
	TestEqual(TEXT("two more"), Bomb.Reserve, 2);
	TestEqual(TEXT("a wheel a second"), Bomb.FireInterval, 1.0f);
	TestEqual(TEXT("a 2.5 s reload"), Bomb.ReloadTime, 2.5f);
	// Wave 16: half as fast again as wave 15's 12.5 m/s, a heavier drop and a shorter reach, so that it must be lobbed.
	TestEqual(TEXT("at 18.75 m/s, half as fast again"), Bomb.ProjectileSpeed, 1250.0f * 1.5f);
	TestTrue(FString::Printf(TEXT("with a heavy drop (%.2f of gravity, from 0.08)"), Bomb.GravityShare), Bomb.GravityShare >= 0.3f && Bomb.GravityShare < 0.5f);
	// Fired level from the eyes it comes down to the floor about 18 m out (reach = v sqrt(2h / g')), short of wave 15's 25.
	const float Reach = LevelReach(Bomb, ReachEyeHeight);
	TestTrue(FString::Printf(TEXT("a level wheel comes down about 18 m out (%.1f m)"), Reach / 100.0f), FMath::IsNearlyEqual(Reach, 1800.0f, 150.0f));
	FWeaponStats Wave15 = Bomb;
	Wave15.ProjectileSpeed = 1250.0f;
	Wave15.GravityShare = 0.08f;
	Wave15.ProjectileLife = 3.0f;
	TestTrue(FString::Printf(TEXT("short of wave 15's wheel (%.1f m)"), LevelReach(Wave15, ReachEyeHeight) / 100.0f), Reach < LevelReach(Wave15, ReachEyeHeight) - 500.0f);
	TestTrue(FString::Printf(TEXT("and a shorter reach at all (%.1f m, from %.1f)"), Bomb.ProjectileSpeed * Bomb.ProjectileLife / 100.0f, Wave15.ProjectileSpeed * Wave15.ProjectileLife / 100.0f),
		Bomb.ProjectileSpeed * Bomb.ProjectileLife < Wave15.ProjectileSpeed * Wave15.ProjectileLife - 500.0f);
	// Lobbed 30 degrees up from the eyes it carries further than fired level, before its life is out.
	{
		const float Up = FMath::DegreesToRadians(30.0f);
		const float Fall = 980.0f * Bomb.GravityShare;
		const float Vz = Bomb.ProjectileSpeed * FMath::Sin(Up);
		const float Down = (Vz + FMath::Sqrt(Vz * Vz + 2.0f * Fall * ReachEyeHeight)) / Fall;
		const float Lobbed = Bomb.ProjectileSpeed * FMath::Cos(Up) * FMath::Min(Down, Bomb.ProjectileLife);
		TestTrue(FString::Printf(TEXT("lobbed 30 degrees up it carries further (%.1f m)"), Lobbed / 100.0f), Lobbed > Reach + 500.0f);
	}
	TestTrue(TEXT("within its life"), Reach / Bomb.ProjectileSpeed < Bomb.ProjectileLife);
	TestTrue(TEXT("a round without gravity reaches as far as it flies"), FMath::IsNearlyEqual(LevelReach(Grater(), ReachEyeHeight), Grater().ProjectileSpeed * Grater().ProjectileLife));
	TestEqual(TEXT("a direct hit 150"), Bomb.Damage, 150.0f);
	TestEqual(TEXT("its burst 110 at its heart"), SplashAt(Bomb, 0.0f), 110.0f);
	TestEqual(TEXT("and to 3 m"), SplashAt(Bomb, 300.0f), 110.0f);
	TestEqual(TEXT("70 at 4 m"), SplashAt(Bomb, 400.0f), 70.0f);
	TestEqual(TEXT("30 at 5 m"), SplashAt(Bomb, 500.0f), 30.0f);
	TestEqual(TEXT("nothing past it"), SplashAt(Bomb, 510.0f), 0.0f);
	TestEqual(TEXT("and no burst for a gun without one"), SplashAt(Blaster(), 0.0f), 0.0f);
	TestTrue(TEXT("a power weapon: slow to carry"), Bomb.CarrySpeed < 1.0f);
	TestEqual(TEXT("bots fire it only from 6 m"), Bomb.BotLeast, 600.0f);
	TestTrue(TEXT("it reaches as far as it flies"), FMath::IsNearlyEqual(Bomb.MaxRange, Bomb.ProjectileSpeed * Bomb.ProjectileLife));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheGratersShardsHomeAndBurstTogether, "CheeseStrike.Weapons.TheGratersShardsHomeAndBurstTogether", WeaponTests::Flags)

bool FTheGratersShardsHomeAndBurstTogether::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Grater = CheeseStrikeWeapons::Grater();
	TestEqual(TEXT("the Cheese Grater"), Grater.DisplayName.ToString(), FString(TEXT("Cheese Grater")));
	TestTrue(TEXT("20 shards a magazine, ten a second, held"), Grater.MagazineSize == 20 && FMath::IsNearlyEqual(Grater.FireInterval, 0.1f) && Grater.bAutomatic);
	TestTrue(TEXT("5 a shard, at 40 m/s"), Grater.Damage == 5.0f && Grater.ProjectileSpeed == 4000.0f && Grater.GravityShare == 0.0f);
	TestTrue(TEXT("homing at up to 60 degrees a second (wave 17, from 25), on someone within 10 degrees of the aim"), Grater.HomingTurn == 60.0f && Grater.HomingCone == 10.0f);
	// Wave 16: much less accurate at range: homing only within 20 m, a wider cone, its damage falling off past 15 m, and only
	// shards that struck within 25 m counting toward the supercombine; close in, as it was.
	TestEqual(TEXT("homing on nobody 40 m off"), HomingPick(FVector::ZeroVector, FVector::ForwardVector, {FVector(4000.0f, 0.0f, 0.0f)}, Grater.HomingCone, Grater.HomingReach), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("but on someone 15 m off"), HomingPick(FVector::ZeroVector, FVector::ForwardVector, {FVector(1500.0f, 0.0f, 0.0f)}, Grater.HomingCone, Grater.HomingReach), 0);
	TestTrue(TEXT("its whole damage close in"), DamageAt(Grater, 1000.0f, false) == Grater.Damage && CountsForSuper(Grater, 1000.0f));
	TestTrue(FString::Printf(TEXT("less than half at 40 m (%.1f)"), DamageAt(Grater, 4000.0f, false)), DamageAt(Grater, 4000.0f, false) < Grater.Damage * 0.5f);
	TestFalse(TEXT("and a shard 40 m off never adds to a supercombine"), CountsForSuper(Grater, 4000.0f));
	{
		// Every round it carries (a magazine and its reserve) fired aimed, fully bloomed, at a body 40 m off: what lands
		// (its shards straight, as none homes that far) doesn't add up to a kill.
		FRandomStream Random(1616);
		const FVector Feet(4000.0f, 0.0f, -160.0f);
		int32 Hits = 0;
		const int32 Rounds = Grater.MagazineSize + Grater.Reserve;
		for (int32 Shot = 0; Shot < Rounds; ++Shot)
		{
			const FVector Way = Spread(FVector::ForwardVector, SpreadFor(Grater, Grater.BloomShots, true), Random);
			Hits += RayHitsCapsule(FVector::ZeroVector, Way, Feet, 34.0f, 180.0f) >= 0.0f ? 1 : 0;
		}
		const float Total = Hits * DamageAt(Grater, 4000.0f, false);
		TestTrue(FString::Printf(TEXT("all %d of its rounds at someone 40 m off: %d land, %.0f in all, no kill"), Rounds, Hits, Total), Total < MaxHealth);
	}
	// Whom it homes on: the nearest within the cone.
	const FVector Eye = FVector::ZeroVector;
	const TArray<FVector> Targets = {FVector(2000.0f, 100.0f, 0.0f), FVector(1000.0f, 120.0f, 0.0f), FVector(500.0f, 400.0f, 0.0f), FVector(-800.0f, 0.0f, 0.0f)};
	TestEqual(TEXT("the nearest within 10 degrees (not the nearer one 39 degrees off, nor the one behind)"), HomingPick(Eye, FVector::ForwardVector, Targets, Grater.HomingCone), 1);
	TestEqual(TEXT("nobody in the cone, nobody"), HomingPick(Eye, FVector::UpVector, Targets, Grater.HomingCone), static_cast<int32>(INDEX_NONE));
	// How it turns: by at most so much a frame, its speed kept, all the way once near enough.
	const FVector Turned = TurnToward(FVector(4000.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), 2.5f);
	const float Degrees = FMath::RadiansToDegrees(FMath::Acos(Turned.GetSafeNormal().X));
	TestTrue(FString::Printf(TEXT("a tenth of a second turns it 2.5 degrees (%.2f)"), Degrees), FMath::IsNearlyEqual(Degrees, 2.5f, 0.01f) && Turned.Y > 0.0f);
	TestTrue(TEXT("at the same speed"), FMath::IsNearlyEqual(Turned.Size(), 4000.0f, 0.1f));
	TestTrue(TEXT("straight at it once within the turn"), TurnToward(FVector(4000.0f, 0.0f, 0.0f), FVector(1.0f, 0.01f, 0.0f), 2.5f).GetSafeNormal().Equals(FVector(1.0f, 0.01f, 0.0f).GetSafeNormal(), 0.0001f));
	// The supercombine: 16 in one player within 2 s burst; slower, they don't.
	TestTrue(TEXT("16 shards (80% of a magazine) in two seconds"), Grater.SuperShards == 16 && Grater.SuperWindow == 2.0f && Grater.SuperShards * 5 == Grater.MagazineSize * 4);
	TestTrue(TEXT("sixteen shards' damage leaves a full-health player standing, even to the head: the supercombine kills"),
		Grater.SuperShards * Grater.Damage < MaxHealth && (Grater.SuperShards - 1) * Grater.Damage * Grater.HeadMultiplier < MaxHealth);
	FShardTally Tally;
	bool bBurst = false;
	for (int32 Shard = 0; Shard < 15; ++Shard)
	{
		bBurst |= Tally.Add(Grater, Shard * 0.1f);
	}
	TestFalse(TEXT("fifteen don't burst"), bBurst);
	TestEqual(TEXT("fifteen are stuck"), Tally.CountAt(Grater, 1.4f), 15);
	TestTrue(TEXT("the sixteenth bursts them"), Tally.Add(Grater, 1.5f));
	TestEqual(TEXT("and the tally starts again"), Tally.CountAt(Grater, 1.5f), 0);
	FShardTally Slow;
	bBurst = false;
	for (int32 Shard = 0; Shard < 30; ++Shard)
	{
		bBurst |= Slow.Add(Grater, Shard * 0.15f);
	}
	TestFalse(TEXT("one every 0.15 s never has sixteen within two seconds"), bBurst);
	TestEqual(TEXT("the oldest come out"), Slow.CountAt(Grater, 29 * 0.15f), 14);
	TestTrue(TEXT("a burst kills (or downs)"), SuperDamage >= MaxHealth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCheeseStickFellsAWideArc, "CheeseStrike.Weapons.TheCheeseStickFellsAWideArc", WeaponTests::Flags)

bool FTheCheeseStickFellsAWideArc::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Stick = CheeseStrikeWeapons::Stick();
	TestTrue(TEXT("a melee weapon in the primary's slot"), Stick.IsMelee() && Stick.Slot == EWeaponSlot::Primary);
	TestTrue(FString::Printf(TEXT("with %d swings (wave 17), no reserve and no reload"), Stick.MagazineSize), Stick.HasSwings() && Stick.MagazineSize >= 12 && Stick.MagazineSize <= 15 && Stick.Reserve == 0 && Stick.ReserveMax == 0);
	TestFalse(TEXT("the knife swings without end"), Knife().HasSwings() || Knife().UsesRounds());
	TestTrue(TEXT("a swing every 1.4 s, 3 m, 120 degrees"), Stick.FireInterval == 1.4f && Stick.Reach == 300.0f && Stick.ArcDegrees == 120.0f);
	TestTrue(TEXT("a blow that kills"), Stick.Damage >= MaxHealth);
	TestTrue(TEXT("a slower, wider swing than the knife's"), SwipeSecondsOf(Stick) > SwipeSecondsOf(Knife()) && Stick.SwipeSize > 1.0f && SwipeSecondsOf(Stick) < Stick.FireInterval);
	// Wave 15: the swing shown 1.6 times slower than wave 14's 0.8 s, the length of its heavy whoosh. Wave 16: held as a
	// sledgehammer in both fists, the left up the handle from the right, its head over the right shoulder at rest, and
	// swung from there: back over the shoulder, down onto straight ahead, on round to the lower left, and back.
	TestTrue(FString::Printf(TEXT("the swing takes 1.28 s (%.2f)"), SwipeSecondsOf(Stick)), FMath::IsNearlyEqual(SwipeSecondsOf(Stick), 0.8f * 1.6f, 0.001f));
	const FWeaponLook& StickLook = CheeseStrikeWeaponLooks::For(Stick.Id);
	TestTrue(TEXT("a sledgehammer, held in the fist"), StickLook.bSledge && StickLook.bInFist && StickLook.Hold == ECheeseStrikeHold::Knife);
	TestTrue(TEXT("in both hands: the left at its end, the right up the handle from it"), StickLook.LeftHand.IsSet() && FVector::DotProduct(*StickLook.LeftHand, StickLook.Along) < -5.0f && StickLook.FistAlong > 10.0f);
	const FWeaponLook& KnifeLook = CheeseStrikeWeaponLooks::For(Knife().Id);
	TestTrue(TEXT("the knife in one fist, the other free"), !KnifeLook.LeftHand.IsSet() && !KnifeLook.bSledge && KnifeLook.bInFist);
	using namespace CheeseStrikeArms;
	const FSledgeAt Rest = SledgeRest();
	TestTrue(FString::Printf(TEXT("at rest its head up over the right shoulder (%.2f, %.2f, %.2f)"), Rest.Along.X, Rest.Along.Y, Rest.Along.Z), Rest.Along.Z > 0.5f && Rest.Along.Y > 0.5f && Rest.Along.X < 0.0f);
	TestTrue(TEXT("the swing starts and ends there"), SledgeSwing(0.0f).Along.Equals(Rest.Along, 0.01f) && SledgeSwing(1.0f).Along.Equals(Rest.Along, 0.01f) && SledgeSwing(1.0f).Hand.IsNearlyZero(0.01f));
	TestTrue(TEXT("wound back over the shoulder"), SledgeSwing(0.2f).Along.X < Rest.Along.X - 0.2f && SledgeSwing(0.2f).Along.Z > 0.3f);
	const FSledgeAt Contact = SledgeSwing(SwipeContactPhase);
	TestTrue(FString::Printf(TEXT("its head straight ahead as the blow lands (%.2f, %.2f, %.2f)"), Contact.Along.X, Contact.Along.Y, Contact.Along.Z), Contact.Along.X > 0.99f);
	TestTrue(TEXT("coming down onto it from over the right"), SledgeSwing(SwipeContactPhase - 0.06f).Along.Z > 0.1f && SledgeSwing(SwipeContactPhase - 0.06f).Along.Y > 0.0f);
	TestTrue(TEXT("and going on round to the lower left"), SledgeSwing(0.5f).Along.Y < -0.3f && SledgeSwing(0.58f).Along.Z < -0.3f);
	// A knife on guard stands up out of the fist.
	const FKnifeAt Guard = KnifeGuard();
	TestTrue(FString::Printf(TEXT("the knife on guard stands up out of the fist (%.0f degrees)"), FMath::RadiansToDegrees(FMath::Asin(Guard.Blade.Z))), Guard.Blade.Z > FMath::Sin(FMath::DegreesToRadians(40.0f)) && Guard.Blade.X > 0.3f);
	TestTrue(TEXT("and the swipe starts and ends on that guard"), KnifeSwipe(0.0f).Blade.Equals(Guard.Blade, 0.01f) && KnifeSwipe(1.0f).Blade.Equals(Guard.Blade, 0.01f));
	const FVector Aim = FVector::ForwardVector;
	TestTrue(TEXT("someone ahead at 2.5 m"), InArc(Stick, Aim, FVector(250.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("someone 55 degrees to the side"), InArc(Stick, Aim, FVector(250.0f, 0.0f, 0.0f).RotateAngleAxis(55.0f, FVector::UpVector)));
	TestTrue(TEXT("on the other side too"), InArc(Stick, Aim, FVector(250.0f, 0.0f, 0.0f).RotateAngleAxis(-55.0f, FVector::UpVector)));
	TestFalse(TEXT("not 65 degrees off"), InArc(Stick, Aim, FVector(250.0f, 0.0f, 0.0f).RotateAngleAxis(65.0f, FVector::UpVector)));
	TestFalse(TEXT("nor behind"), InArc(Stick, Aim, FVector(-150.0f, 0.0f, 0.0f)));
	TestFalse(TEXT("nor past 3 m"), InArc(Stick, Aim, FVector(320.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("someone lying below the aim, whatever the pitch"), InArc(Stick, FVector(1.0f, 0.0f, -0.5f).GetSafeNormal(), FVector(150.0f, 20.0f, -120.0f)));
	TestTrue(TEXT("a knife strikes one along its sweep, the stick everyone in its arc"), Knife().ArcDegrees == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMunsterTwinsFireInTurn, "CheeseStrike.Weapons.TheMunsterTwinsFireInTurn", WeaponTests::Flags)

bool FTheMunsterTwinsFireInTurn::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Twins = Munster();
	TestEqual(TEXT("the Munster Twin SMGs"), Twins.DisplayName.ToString(), FString(TEXT("Munster Twin SMGs")));
	TestTrue(TEXT("two guns, automatic"), Twins.bDual && Twins.bAutomatic);
	TestEqual(TEXT("32 rounds each (wave 17, from a belt of 60)"), Twins.MagazineSize, 64);
	TestTrue(TEXT("firing in turn"), DualHand(Twins, 0) == 0 && DualHand(Twins, 1) == 1 && DualHand(Twins, 2) == 0);
	TestEqual(TEXT("not heavy any more"), Twins.CarrySpeed, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheGatlingsBarrelsSpinUpAndWindDown, "CheeseStrike.Weapons.TheGatlingsBarrelsSpinUpAndWindDown", WeaponTests::Flags)

bool FTheGatlingsBarrelsSpinUpAndWindDown::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Gatling = CheeseStrikeWeapons::Gatling();
	// Six barrels, a shot from each in turn: at twenty a second, a third of a turn a second... a sixth of a turn a shot.
	TestEqual(TEXT("six barrels"), Gatling.Barrels, 6);
	const float Full = FullBarrelSpeed(Gatling);
	TestTrue(FString::Printf(TEXT("at full speed a sixth of a turn a shot: %.0f degrees a second"), Full), FMath::IsNearlyEqual(Full, 360.0f / 6.0f / Gatling.FireInterval, 0.01f) && Full > 1000.0f);
	TestEqual(TEXT("a gun without barrels turns nothing"), FullBarrelSpeed(Blaster()), 0.0f);
	// Held: up to full speed in the spin-up's 0.4 s, steadily; let go: down to nothing over the wind-down, steadily.
	float Speed = 0.0f;
	float Time = 0.0f;
	int32 Frames = 0;
	while (Speed < Full && Frames < 1000)
	{
		Speed = BarrelSpeed(Speed, Full, 1.0f / 120.0f, Gatling.SpinUp, Full);
		Time += 1.0f / 120.0f;
		++Frames;
	}
	TestTrue(FString::Printf(TEXT("full speed as the spin-up ends (%.2f s)"), Time), FMath::IsNearlyEqual(Time, Gatling.SpinUp, 0.02f));
	TestTrue(TEXT("half way at half the spin-up"), FMath::IsNearlyEqual(BarrelSpeed(0.0f, Full, Gatling.SpinUp * 0.5f, Gatling.SpinUp, Full), Full * 0.5f, 1.0f));
	TestEqual(TEXT("and no faster, held on"), BarrelSpeed(Full, Full, 0.1f, Gatling.SpinUp, Full), Full);
	Time = 0.0f;
	Frames = 0;
	while (Speed > 0.0f && Frames < 1000)
	{
		Speed = BarrelSpeed(Speed, 0.0f, 1.0f / 60.0f, Gatling.SpinUp, Full);
		Time += 1.0f / 60.0f;
		++Frames;
	}
	TestTrue(FString::Printf(TEXT("wound down to nothing in %.1f s"), Time), FMath::IsNearlyEqual(Time, BarrelSpinDown, 0.03f) && BarrelSpinDown > Gatling.SpinUp);
	TestTrue(TEXT("let go half way up, it winds down from there at the same rate"), FMath::IsNearlyEqual(BarrelSpeed(Full * 0.5f, 0.0f, 0.1f, Gatling.SpinUp, Full), Full * 0.5f - Full / BarrelSpinDown * 0.1f, 0.5f));
	TestEqual(TEXT("and never below nothing"), BarrelSpeed(1.0f, 0.0f, 1.0f, Gatling.SpinUp, Full), 0.0f);
	// Its barrels are a part of their own, turning about the axle.
	const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(Gatling.Id);
	TestNotNull(TEXT("the Gatling's barrels are a part of their own"), Look.SpinningPart);
	TestTrue(TEXT("turning about the barrels' axis, along the gun"), Look.SpinAxis.GetSafeNormal().Equals(FVector(0.0f, 1.0f, 0.0f)) && FMath::IsNearlyEqual(Look.SpinCentre.Z, Look.Muzzle.Z));
	TestNull(TEXT("another gun has none"), CheeseStrikeWeaponLooks::For(Blaster().Id).SpinningPart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheReserveShowsWithoutEnd, "CheeseStrike.Weapons.TheReserveShowsWithoutEnd", WeaponTests::Flags)

bool FTheReserveShowsWithoutEnd::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a reserve shows its rounds"), CheeseStrikeWeapons::ReserveLabel(72), FString(TEXT("/ 72")));
	TestEqual(TEXT("an empty one shows none"), CheeseStrikeWeapons::ReserveLabel(0), FString(TEXT("/ 0")));
	TestEqual(TEXT("one without end shows it"), CheeseStrikeWeapons::ReserveLabel(-1), FString(TEXT("/ ∞")));
	return true;
}

#endif
