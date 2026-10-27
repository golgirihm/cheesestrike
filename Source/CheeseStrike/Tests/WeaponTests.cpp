// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The arsenal as arithmetic: every weapon's numbers against the design's
// table (time to kill, bursts, pellets, the knife's reach and its stroke in
// the back, draw times, life-size lengths), the lists the game is played by
// (loadouts, Gun Game's ladder), and how far a shot carries to the ear.

#include "Misc/AutomationTest.h"
#include "Weapons/CheeseStrikeBeat.h"
#include "Animation/CheeseStrikeDualHold.h"
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
	// The design's TTK column (docs/unreal/design.md, "Weapons"): body shots, close, holding the trigger. The Colby
	// Carbine, the spawn primary, is softened from it: three bursts to a kill.
	for (const FWanted& Wanted : {FWanted{Pistol(), 9, 2.0f}, FWanted{Blaster(), 5, 0.48f}, FWanted{Carbine(), 7, 0.96f}, FWanted{Grater(), 9, 0.52f},
		FWanted{Munster(), 7, 0.51f}, FWanted{Shotgun(), 2, 0.85f}, FWanted{Roquefort(), 2, 1.3f}, FWanted{Knife(), 2, 0.75f}, FWanted{Burratas(), 7, 0.6f}})
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
	// A Munster's shots shown on frames of 46 ms with a hitch now and then: the sounds still come exactly an interval apart.
	const double Interval = 0.085;
	FSoundBeat Beat;
	TArray<double> Heard;
	const FSoundBeat::FPlan First = Beat.Plan(10.0, Interval);
	TestTrue(TEXT("the first sound starts the run, held back a lead"), First.bStartsRun && FMath::IsNearlyEqual(First.CallAt, 10.0 + FSoundBeat::LeadShare * Interval));
	Beat.Started(First.CallAt);
	Heard.Add(First.CallAt);
	double Shown = 10.0;
	for (int32 Shot = 1; Shot < 40; ++Shot)
	{
		// The shot is shown on the first frame at or after its time; a long frame every seventh shot.
		const double Frame = Shot % 7 == 0 ? 0.075 : 0.046;
		Shown = 10.0 + FMath::CeilToDouble(Shot * Interval / Frame) * Frame;
		const FSoundBeat::FPlan Plan = Beat.Plan(Shown, Interval);
		TestFalse(TEXT("only the first starts the run"), Plan.bStartsRun);
		TestTrue(TEXT("and it is heard after it is asked for"), Plan.BeatTime >= Shown);
		Heard.Add(Plan.BeatTime);
	}
	bool bExact = true;
	for (int32 Index = 1; Index < Heard.Num(); ++Index)
	{
		const double Beats = (Heard[Index] - Heard[Index - 1]) / Interval;
		bExact = bExact && FMath::Abs(Beats - FMath::RoundToDouble(Beats)) * Interval < 0.001 && Beats > 0.5;
	}
	TestTrue(TEXT("every sound is on the beat, none on another's"), bExact);
	TestTrue(TEXT("a run that stops starts again"), Beat.Plan(Shown + 1.0, Interval).bStartsRun);
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
	for (const FReach& Wanted : {FReach{Pistol(), 90, 25, 60}, FReach{Blaster(), 150, 40, 90}, FReach{Carbine(), 140, 35, 80}, FReach{Grater(), 80, 15, 45},
		FReach{Munster(), 120, 25, 70}, FReach{Shotgun(), 40, 8, 25}, FReach{Roquefort(), 250, 0, 0}, FReach{Burratas(), 90, 20, 55}})
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
	TestTrue(TEXT("a heavy weapon slows its carrier"), FMath::IsNearlyEqual(CheeseStrikeMovement::WithWeapon(450.0f, true), 450.0f * 0.85f));
	TestEqual(TEXT("another doesn't"), CheeseStrikeMovement::WithWeapon(675.0f, false), 675.0f);
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
	TestEqual(TEXT("nine weapons"), All.Num(), 9);
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
	TestEqual(TEXT("seven primaries"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary).Num(), 7);
	TestEqual(TEXT("the Curd Pistol is the sidearm"), CheeseStrikeArsenal::InSlot(EWeaponSlot::Sidearm), TArray<FName>{CheeseStrikeArsenal::Sidearm()});
	TestEqual(TEXT("everyone comes in with the Colby Carbine"), CheeseStrikeArsenal::DefaultPrimary(), CheeseStrikeWeapons::Carbine().Id);

	// Gun Game: through the guns a kill at a time, the knife last.
	const TArray<FName>& Ladder = CheeseStrikeArsenal::GunGameLadder();
	TestEqual(TEXT("Gun Game works through every weapon"), Ladder.Num(), All.Num());
	TestEqual(TEXT("from the Cheddar Blaster"), CheeseStrikeArsenal::GunGameWeapon(0), CheeseStrikeWeapons::Blaster().Id);
	TestEqual(TEXT("to the Curd Pistol"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 2), CheeseStrikeWeapons::Pistol().Id);
	TestEqual(TEXT("and last the knife"), CheeseStrikeArsenal::GunGameWeapon(Ladder.Num() - 1), CheeseStrikeWeapons::Knife().Id);
	TestEqual(TEXT("which it keeps"), CheeseStrikeArsenal::GunGameWeapon(100), CheeseStrikeWeapons::Knife().Id);
	TestTrue(TEXT("the Dual Burratas are on it, after the Colby Carbine"), Ladder.IndexOfByKey(CheeseStrikeWeapons::Burratas().Id) == Ladder.IndexOfByKey(CheeseStrikeWeapons::Carbine().Id) + 1);
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
	// A shot a pull (as Counter-Strike's are), no more than ten a second however fast the pulls; a long magazine, a long
	// reload, middling damage.
	TestFalse(TEXT("not automatic: a shot a pull"), Dual.bAutomatic);
	TestEqual(TEXT("one shot to a pull, no burst"), Dual.Burst, 1);
	TestTrue(FString::Printf(TEXT("pulled as fast as can be, %.0f a second"), ShotsPerSecond(Dual)), ShotsPerSecond(Dual) <= 10.0f + KINDA_SMALL_NUMBER && ShotsPerSecond(Dual) >= 8.0f);
	TestTrue(TEXT("faster than the Cheddar Blaster"), ShotsPerSecond(Dual) > ShotsPerSecond(Blaster()));
	TestEqual(TEXT("thirty rounds"), Dual.MagazineSize, 30);
	TestTrue(TEXT("a longer magazine than the Cheddar Blaster's"), Dual.MagazineSize > Blaster().MagazineSize);
	TestTrue(TEXT("a reload longer than any but the Munster's"), Dual.ReloadTime > Roquefort().ReloadTime && Dual.ReloadTime < Munster().ReloadTime);
	TestTrue(TEXT("a hit between the pistol's and the Cheddar Blaster's"), Dual.Damage > Pistol().Damage && Dual.Damage < Blaster().Damage);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheReserveShowsWithoutEnd, "CheeseStrike.Weapons.TheReserveShowsWithoutEnd", WeaponTests::Flags)

bool FTheReserveShowsWithoutEnd::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("a reserve shows its rounds"), CheeseStrikeWeapons::ReserveLabel(72), FString(TEXT("/ 72")));
	TestEqual(TEXT("an empty one shows none"), CheeseStrikeWeapons::ReserveLabel(0), FString(TEXT("/ 0")));
	TestEqual(TEXT("one without end shows it"), CheeseStrikeWeapons::ReserveLabel(-1), FString(TEXT("/ ∞")));
	return true;
}

#endif
