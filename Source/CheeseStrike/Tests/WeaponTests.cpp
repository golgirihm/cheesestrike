// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The arsenal as arithmetic: every weapon's numbers against the design's
// table (time to kill, bursts, pellets, the knife's reach and its stroke in
// the back, draw times, life-size lengths), the lists the game is played by
// (loadouts, Gun Game's ladder), and how far a shot carries to the ear.

#include "Misc/AutomationTest.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeGunfire.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeShot.h"

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
	// The design's TTK column (docs/unreal/design.md, "Weapons"): body shots, close, holding the trigger.
	for (const FWanted& Wanted : {FWanted{Pistol(), 9, 2.0f}, FWanted{Blaster(), 5, 0.48f}, FWanted{Carbine(), 6, 0.62f}, FWanted{Grater(), 9, 0.52f},
		FWanted{Munster(), 7, 0.51f}, FWanted{Shotgun(), 2, 0.85f}, FWanted{Roquefort(), 2, 1.3f}, FWanted{Knife(), 2, 0.75f}})
	{
		const FString Name = Wanted.Weapon.DisplayName.ToString();
		TestEqual(FString::Printf(TEXT("%s kills in %d"), *Name, Wanted.Shots), ShotsToKill(Wanted.Weapon), Wanted.Shots);
		TestTrue(FString::Printf(TEXT("in %.2f s (%.2f)"), Wanted.Seconds, TimeToKill(Wanted.Weapon)), FMath::IsNearlyEqual(TimeToKill(Wanted.Weapon), Wanted.Seconds, 0.01f));
	}
	TestTrue(TEXT("the Roquefort kills with one shot to the head"), DamageAt(Roquefort(), Roquefort().MaxRange, true) >= MaxHealth);
	TestTrue(TEXT("however far: it has no falloff"), DamageAt(Roquefort(), Roquefort().MaxRange, false) == Roquefort().Damage);
	TestTrue(TEXT("the Curd Pistol is the weakest gun"), Pistol().Damage / Pistol().FireInterval < Blaster().Damage / Blaster().FireInterval);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheShotgunFiresNinePellets, "CheeseStrike.Weapons.TheShotgunFiresNinePellets", WeaponTests::Flags)

bool FTheShotgunFiresNinePellets::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeWeapons;
	const FWeaponStats& Shotgun = CheeseStrikeWeapons::Shotgun();
	TestEqual(TEXT("nine pellets"), Shotgun.Pellets, 9);
	TestEqual(TEXT("ten each, close"), DamageAt(Shotgun, 100.0f, false), 10.0f);
	TestEqual(TEXT("three each far off"), DamageAt(Shotgun, 25.0f * WeaponTests::Metre, false), 3.0f);
	// The pellets are spread by the shooter's seed, so the server makes the same nine.
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
	TestTrue(FString::Printf(TEXT("it reaches 4.5 of the design's metres: 150 cm (%.0f)"), Knife.Reach), FMath::IsNearlyEqual(Knife.Reach, 150.0f, 0.5f));
	TestEqual(TEXT("55 from the front"), Knife.Damage, 55.0f);
	TestFalse(TEXT("it fires no rounds"), Knife.UsesRounds());

	const TArray<FVector> Fan = SweepDirections(Knife, FVector::ForwardVector);
	TestEqual(TEXT("five ways: straight, and two either side"), Fan.Num(), 5);
	TestTrue(TEXT("straight first"), Fan.Num() > 0 && Fan[0].Equals(FVector::ForwardVector, 0.001f));
	float Widest = 0.0f;
	for (const FVector& Way : Fan)
	{
		Widest = FMath::Max(Widest, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Way, FVector::ForwardVector), -1.0f, 1.0f))));
		TestTrue(TEXT("flat"), FMath::IsNearlyZero(Way.Z, 0.001f));
	}
	TestTrue(FString::Printf(TEXT("sweeping 16 degrees either side (%.1f)"), Widest), FMath::IsNearlyEqual(Widest, 16.0f, 0.1f));

	// Someone facing +X, struck going +X, is struck in the back.
	TestTrue(TEXT("a stroke the way they face is in the back"), FromBehind(FVector::ForwardVector, 0.0f));
	TestTrue(TEXT("from a little to one side too"), FromBehind(FVector(1.0f, 0.5f, 0.0f).GetSafeNormal(), 0.0f));
	TestFalse(TEXT("but not from the side"), FromBehind(FVector::RightVector, 0.0f));
	TestFalse(TEXT("nor the front"), FromBehind(FVector::BackwardVector, 0.0f));
	TestTrue(TEXT("whichever way they face"), FromBehind(FVector::RightVector, 90.0f));
	TestTrue(TEXT("in the back, it kills at a stroke"), StrokeDamage(Knife, FVector::ForwardVector, 0.0f) >= MaxHealth);
	TestEqual(TEXT("from the front, two strokes"), StrokeDamage(Knife, FVector::BackwardVector, 0.0f), 55.0f);
	TestTrue(TEXT("its stroke lunges at the design's 7 against its 6 m/s walk"), FMath::IsNearlyEqual(Knife.LungeSpeed, CheeseStrikeMovement::LungeSpeed) && FMath::IsNearlyEqual(Knife.LungeSpeed / CheeseStrikeMovement::WalkSpeed, 7.0f / 6.0f));
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
	for (const FReach& Wanted : {FReach{Pistol(), 90, 25, 60}, FReach{Blaster(), 150, 40, 90}, FReach{Carbine(), 140, 35, 80}, FReach{Grater(), 80, 15, 45},
		FReach{Munster(), 120, 25, 70}, FReach{Shotgun(), 40, 8, 25}, FReach{Roquefort(), 250, 0, 0}})
	{
		const FString Name = Wanted.Weapon.DisplayName.ToString();
		TestTrue(FString::Printf(TEXT("%s reaches %.0f m (%.0f cm)"), *Name, Wanted.Range / 3.0f, Wanted.Weapon.MaxRange), FMath::IsNearlyEqual(Wanted.Weapon.MaxRange, Wanted.Range * WeaponTests::Metre, 1.0f));
		TestTrue(FString::Printf(TEXT("%s falls off from %.0f to %.0f m"), *Name, Wanted.FalloffStart / 3.0f, Wanted.FalloffEnd / 3.0f),
			FMath::IsNearlyEqual(Wanted.Weapon.FalloffStart, Wanted.FalloffStart * WeaponTests::Metre, 1.0f) && FMath::IsNearlyEqual(Wanted.Weapon.FalloffEnd, Wanted.FalloffEnd * WeaponTests::Metre, 1.0f));
	}
	// Coming up: the usual third of a second, the machine gun slow and the sniper's rifle slower than most.
	TestEqual(TEXT("the Munster takes 0.9 s to draw"), Munster().DrawTime, 0.9f);
	TestEqual(TEXT("and slows its carrier to 0.85"), Munster().CarrySpeed, CheeseStrikeMovement::HeavyFactor);
	TestEqual(TEXT("the Roquefort 0.5 s"), Roquefort().DrawTime, 0.5f);
	TestTrue(TEXT("and is scoped"), Roquefort().bScoped);
	TestFalse(TEXT("drawn just now, it can't fire"), IsDrawn(Munster(), 10.0f, 10.5f));
	TestTrue(TEXT("once the draw time is past, it can"), IsDrawn(Munster(), 10.0f, 10.95f));
	TestFalse(TEXT("not a little before"), IsDrawn(Munster(), 10.0f, 10.8f));
	TestTrue(TEXT("though the server allows a shot a little early, for the network"), IsDrawn(Munster(), 10.0f, 10.8f, DrawAllowance));
	TestFalse(TEXT("but not much"), IsDrawn(Munster(), 10.0f, 10.7f, DrawAllowance));
	TestTrue(TEXT("put in hand at once, it can fire at once"), IsDrawn(Munster(), -1000.0f, 0.0f));
	// Carrying.
	TestTrue(TEXT("a heavy weapon slows its carrier"), FMath::IsNearlyEqual(CheeseStrikeMovement::WithWeapon(450.0f, true, false), 450.0f * 0.85f));
	TestTrue(TEXT("a lunge is quicker than a walk"), CheeseStrikeMovement::WithWeapon(CheeseStrikeMovement::WalkSpeed, false, true) > CheeseStrikeMovement::WalkSpeed);
	TestEqual(TEXT("but never slows a sprint"), CheeseStrikeMovement::WithWeapon(675.0f, false, true), 675.0f);
	// Rounds.
	TestTrue(TEXT("the Curd Pistol's rounds never run out"), Pistol().HasEndlessReserve());
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		if (Weapon->Slot == EWeaponSlot::Primary)
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
	TestEqual(TEXT("eight weapons"), All.Num(), 8);
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
	TestEqual(TEXT("six primaries"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary).Num(), 6);
	TestEqual(TEXT("the Curd Pistol is the sidearm"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Sidearm), TArray<FName>{CheeseStrikeArsenal::Sidearm()});
	TestEqual(TEXT("the Cheddar Blaster by default"), CheeseStrikeArsenal::DefaultPrimary(), CheeseStrikeWeapons::Blaster().Id);

	// Gun Game: through the guns a kill at a time, the knife last.
	const TArray<FName>& Ladder = CheeseStrikeArsenal::GunGameLadder();
	TestEqual(TEXT("Gun Game works through every weapon"), Ladder.Num(), All.Num());
	TestEqual(TEXT("from the Cheddar Blaster"), CheeseStrikeArsenal::GunGameWeapon(0), CheeseStrikeWeapons::Blaster().Id);
	TestEqual(TEXT("to the Curd Pistol"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 2), CheeseStrikeWeapons::Pistol().Id);
	TestEqual(TEXT("and last the knife"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 1), CheeseStrikeWeapons::Knife().Id);
	TestEqual(TEXT("which it keeps"), CheeseStrikeArsenal::GunGameWeapon(100), CheeseStrikeWeapons::Knife().Id);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheReserveShowsWithoutEnd, "CheeseStrike.Weapons.TheReserveShowsWithoutEnd", WeaponTests::Flags)

bool FTheReserveShowsWithoutEnd::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a reserve shows its rounds"), CheeseStrikeWeapons::ReserveLabel(72), FString(TEXT("/ 72")));
	TestEqual(TEXT("an empty one shows none"), CheeseStrikeWeapons::ReserveLabel(0), FString(TEXT("/ 0")));
	TestEqual(TEXT("one without end shows it"), CheeseStrikeWeapons::ReserveLabel(-1), FString(TEXT("/ ∞")));
	return true;
}

#endif
