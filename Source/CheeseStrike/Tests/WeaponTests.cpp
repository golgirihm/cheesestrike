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
	// Carbine, the spawn primary, is softened from it: three bursts to a kill. Wave 14: the Cheese Grater's shards (before
	// the supercombine), the Munster Twin SMGs, the Swiss Shotgun's whole blast close, the BabyBomb's wheel and the Cheese
	// Stick's swing.
	for (const FWanted& Wanted : {FWanted{Pistol(), 9, 2.0f}, FWanted{Blaster(), 5, 0.48f}, FWanted{Carbine(), 7, 0.96f}, FWanted{Grater(), 13, 1.2f},
		FWanted{Munster(), 8, 0.525f}, FWanted{Shotgun(), 1, 0.0f}, FWanted{Roquefort(), 2, 1.3f}, FWanted{Knife(), 2, 0.75f}, FWanted{Burratas(), 7, 0.6f},
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
	// At 4 m its cone is no wider than a body, aimed or from the hip, so they all land: a kill a shot.
	const float Across4 = FMath::Tan(FMath::DegreesToRadians(Shotgun.HipSpreadDegrees * 0.5f)) * 400.0f;
	TestTrue(FString::Printf(TEXT("from the hip its pellets are within %.0f cm of the aim at 4 m: a body's width"), Across4), Across4 <= CheeseStrikeMovement::CapsuleRadius * 1.1f);
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
	// Rounds.
	TestTrue(TEXT("the Curd Pistol's rounds never run out"), Pistol().HasEndlessReserve());
	for (const FWeaponStats* Weapon : CheeseStrikeArsenal::All())
	{
		if (Weapon->Slot == EWeaponSlot::Primary && !Weapon->bSpecial)
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
	TestEqual(TEXT("at 25 m/s"), Bomb.ProjectileSpeed, 2500.0f);
	TestTrue(TEXT("with a little drop"), Bomb.GravityShare > 0.0f && Bomb.GravityShare < 0.5f);
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
	TestTrue(TEXT("8 a shard, at 40 m/s"), Grater.Damage == 8.0f && Grater.ProjectileSpeed == 4000.0f && Grater.GravityShare == 0.0f);
	TestTrue(TEXT("homing at up to 25 degrees a second, on someone within 10 degrees of the aim"), Grater.HomingTurn == 25.0f && Grater.HomingCone == 10.0f);
	// Whom it homes on: the nearest within the cone.
	const FVector Eye = FVector::ZeroVector;
	const TArray<FVector> Targets = {FVector(2000.0f, 100.0f, 0.0f), FVector(1000.0f, 120.0f, 0.0f), FVector(500.0f, 400.0f, 0.0f), FVector(-800.0f, 0.0f, 0.0f)};
	TestEqual(TEXT("the nearest within 10 degrees (not the nearer one 39 degrees off, nor the one behind)"), HomingPick(Eye, FVector::ForwardVector, Targets, Grater.HomingCone), 1);
	TestEqual(TEXT("nobody in the cone, nobody"), HomingPick(Eye, FVector::UpVector, Targets, Grater.HomingCone), static_cast<int32>(INDEX_NONE));
	// How it turns: by at most so much a frame, its speed kept, all the way once near enough.
	const FVector Turned = TurnToward(FVector(4000.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), 25.0f * 0.1f);
	const float Degrees = FMath::RadiansToDegrees(FMath::Acos(Turned.GetSafeNormal().X));
	TestTrue(FString::Printf(TEXT("a tenth of a second turns it 2.5 degrees (%.2f)"), Degrees), FMath::IsNearlyEqual(Degrees, 2.5f, 0.01f) && Turned.Y > 0.0f);
	TestTrue(TEXT("at the same speed"), FMath::IsNearlyEqual(Turned.Size(), 4000.0f, 0.1f));
	TestTrue(TEXT("straight at it once within the turn"), TurnToward(FVector(4000.0f, 0.0f, 0.0f), FVector(1.0f, 0.01f, 0.0f), 2.5f).GetSafeNormal().Equals(FVector(1.0f, 0.01f, 0.0f).GetSafeNormal(), 0.0001f));
	// The supercombine: 16 in one player within 2 s burst; slower, they don't.
	TestTrue(TEXT("16 shards (80% of a magazine) in two seconds"), Grater.SuperShards == 16 && Grater.SuperWindow == 2.0f && Grater.SuperShards * 5 == Grater.MagazineSize * 4);
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
	TestTrue(TEXT("a melee weapon in the primary's slot"), Stick.IsMelee() && Stick.Slot == EWeaponSlot::Primary && !Stick.UsesRounds());
	TestTrue(TEXT("a swing every 1.1 s, 3 m, 120 degrees"), Stick.FireInterval == 1.1f && Stick.Reach == 300.0f && Stick.ArcDegrees == 120.0f);
	TestTrue(TEXT("a blow that kills"), Stick.Damage >= MaxHealth);
	TestTrue(TEXT("a slower, wider swing than the knife's"), SwipeSecondsOf(Stick) > SwipeSecondsOf(Knife()) && Stick.SwipeSize > 1.0f && SwipeSecondsOf(Stick) < Stick.FireInterval);
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
	TestEqual(TEXT("a belt of 60 each"), Twins.MagazineSize, 120);
	TestTrue(TEXT("firing in turn"), DualHand(Twins, 0) == 0 && DualHand(Twins, 1) == 1 && DualHand(Twins, 2) == 0);
	TestEqual(TEXT("not heavy any more"), Twins.CarrySpeed, 1.0f);
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
