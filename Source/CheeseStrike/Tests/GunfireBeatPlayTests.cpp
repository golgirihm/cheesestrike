// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The beat of automatic fire, measured where it is put on: the Munster
// Machine Gun and the Cheddar Blaster held down for a while on the listen
// host and on a joining client, and the gaps between the shots fired, the
// shots shown (animation and flash together), the shot sounds started and
// the muzzle flashes, each as the machine that shows it has them. Waves 8 and
// 9 judged the beat by ear and by reasoning; this is the measure (the
// intervals are in the test's log).

#include "Containers/Ticker.h"
#include "Misc/AutomationTest.h"
#include "CheeseStrikeCharacter.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeBeat.h"
#include "Weapons/CheeseStrikeWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace GunfireBeatPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** Two spots on the shooter level, as WeaponPlayTests', each facing away from the other so that nothing is hit. */
	const FVector HostSpot(-231.0f, -300.0f, 100.0f);
	const FVector JoinerSpot(-231.0f, -700.0f, 100.0f);
	constexpr float HostYaw = 90.0f;
	constexpr float JoinerYaw = -90.0f;
	/** Seconds the trigger is held. */
	constexpr float Held = 2.5f;

	/** The length of the frames while the trigger is held (seconds): a shot is shown in a frame, so its gap is a whole number of them. */
	TArray<double> Frames;
	FTSTicker::FDelegateHandle FrameWatch;

	/** The frame length most frames are no longer than (the 95th percentile), so that a hitch or two doesn't widen what is allowed. */
	double TypicalFrame()
	{
		TArray<double> Sorted = Frames;
		Sorted.Sort();
		return Sorted.Num() > 0 ? Sorted[FMath::Min(Sorted.Num() - 1, static_cast<int32>(Sorted.Num() * 0.95))] : 0.0;
	}

	/** One machine's view of a weapon's beat: what is said of it, and what must hold. */
	void Check(FAutomationTestBase& Test, const TCHAR* Who, const TCHAR* What, const ACheeseStrikeWeapon* Weapon, CheeseStrikeBeat::EEvent Event, double Interval, bool bEvenToo)
	{
		const TArray<double> Times = CheeseStrikeBeat::Times(Weapon, Event);
		const CheeseStrikeBeat::FEvenness Even = CheeseStrikeBeat::Evenness(Times);
		const int32 Expected = FMath::FloorToInt(Held / Interval);
		Test.AddInfo(FString::Printf(TEXT("%s, %s: %d shots, gaps mean %.1f ms, shortest %.1f, longest %.1f, deviation %.1f (the beat is %.1f)"), Who, What, Times.Num(), Even.Mean * 1000.0, Even.Shortest * 1000.0, Even.Longest * 1000.0, Even.Deviation * 1000.0, Interval * 1000.0));
		Test.TestTrue(FString::Printf(TEXT("%s, %s: about %d shots came (%d)"), Who, What, Expected, Times.Num()), FMath::Abs(Times.Num() - Expected) <= 3);
		if (bEvenToo && Even.Intervals > 3)
		{
			// A shot is shown in a frame, so no gap can be nearer the beat than a frame; but none may be further off than that,
			// as network clumps, a late shot or a sound cut short would make it.
			const double Frame = TypicalFrame();
			const double Slack = Frame * 1.25 + 0.004;
			Test.TestTrue(FString::Printf(TEXT("%s, %s: no gap more than a frame off the beat (%.1f to %.1f ms of %.1f, frames %.1f ms)"), Who, What, Even.Shortest * 1000.0, Even.Longest * 1000.0, Interval * 1000.0, Frame * 1000.0),
				Even.Shortest > Interval - Slack && Even.Longest < Interval + Slack);
			Test.TestTrue(FString::Printf(TEXT("%s, %s: and the gaps average the beat (%.1f of %.1f ms)"), Who, What, Even.Mean * 1000.0, Interval * 1000.0), FMath::Abs(Even.Mean - Interval) < Interval * 0.03);
		}
	}

	/** Holds fire on `Shooter` and, when the shots are done, checks every machine's beat of them. */
	void HoldAndMeasure(FAutomationTestBase& Test, int32 Shooter, const FWeaponStats& Weapon)
	{
		using namespace PlayTesting;
		Then([Shooter]()
		{
			CheeseStrikeBeat::bRecording = true;
			CheeseStrikeBeat::Reset();
			Frames.Reset();
			FTSTicker::GetCoreTicker().RemoveTicker(FrameWatch);
			FrameWatch = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Delta) { Frames.Add(Delta); return true; }), 0.0f);
			// Rounds enough for the whole hold, on the server's copy.
			if (ACheeseStrikeCharacter* OnServer = CharacterOf(Shooter, 0))
			{
				OnServer->EndProtection();
				if (OnServer->GetWeapon() != nullptr)
				{
					OnServer->GetWeapon()->SetRounds(1000, 1000);
				}
			}
		});
		Wait(0.3f);
		Play(Shooter, FString::Printf(TEXT("hold Fire\nwait %.1f\nrelease Fire\nwait 0.6"), Held));
		Then([&Test, Shooter, &Weapon]()
		{
			using namespace CheeseStrikeBeat;
			const double Interval = Weapon.FireInterval;
			const int32 Other = 1 - Shooter;
			const ACheeseStrikeCharacter* Own = Character(Shooter);
			const ACheeseStrikeCharacter* AsSeen = CharacterOf(Shooter, Other);
			const FString Name = Weapon.DisplayName.ToString();
			if (Own == nullptr || Own->GetWeapon() == nullptr || AsSeen == nullptr || AsSeen->GetWeapon() == nullptr)
			{
				Test.AddError(TEXT("the shooter or their weapon is missing on a machine"));
				return;
			}
			const FString Mine = FString::Printf(TEXT("%s on %s"), *Name, Shooter == 0 ? TEXT("the host, its own shooter") : TEXT("the client, its own shooter"));
			const FString Theirs = FString::Printf(TEXT("%s on %s"), *Name, Shooter == 0 ? TEXT("the client, watching the host") : TEXT("the host, watching the client"));
			const ACheeseStrikeWeapon* Fired = Own->GetWeapon();
			const ACheeseStrikeWeapon* Watched = AsSeen->GetWeapon();
			Check(Test, *Mine, TEXT("fired"), Fired, EEvent::Fired, Interval, true);
			Check(Test, *Mine, TEXT("shown"), Fired, EEvent::Shown, Interval, true);
			Check(Test, *Mine, TEXT("sound started"), Fired, EEvent::Sounded, Interval, true);
			Check(Test, *Mine, TEXT("flashed"), Fired, EEvent::Flashed, Interval, true);
			Check(Test, *Theirs, TEXT("shown"), Watched, EEvent::Shown, Interval, true);
			Check(Test, *Theirs, TEXT("sound started"), Watched, EEvent::Sounded, Interval, true);
			Check(Test, *Theirs, TEXT("flashed"), Watched, EEvent::Flashed, Interval, true);
			// The server allowed every one the shooter fired.
			const ACheeseStrikeWeapon* OnServer = CharacterOf(Shooter, 0) != nullptr ? CharacterOf(Shooter, 0)->GetWeapon() : nullptr;
			Test.TestTrue(TEXT("the server has the shooter's weapon"), OnServer != nullptr);
			Test.TestEqual(TEXT("and every shot fired was noted"), Times(Fired, EEvent::Fired).Num(), static_cast<int32>(Fired->ShotsFired));
			CheeseStrikeBeat::bRecording = false;
			FTSTicker::GetCoreTicker().RemoveTicker(FrameWatch);
		});
	}

	void Measure(FAutomationTestBase& Test, const FWeaponStats& Weapon)
	{
		using namespace PlayTesting;
		FSession Session;
		Session.Players = 2;
		Session.SpawnPrimary = Weapon.Id.ToString();
		StartPlaying(Session);
		Then([&Test]()
		{
			// How long the shot sounds run: shots overlap as many times as this is long over the interval.
			for (int32 Variant = 1; Variant <= 3; ++Variant)
			{
				const FString Path = FString::Printf(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_0%d.Weapons_Rifle_Punch-Close_0%d"), Variant, Variant);
				if (const USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
				{
					Test.AddInfo(FString::Printf(TEXT("rifle shot %d runs %.3f s"), Variant, Sound->GetDuration()));
				}
			}
		});
		PutAt(0, HostSpot, HostYaw);
		PutAt(1, JoinerSpot, JoinerYaw);
		HoldAndMeasure(Test, 0, Weapon);
		HoldAndMeasure(Test, 1, Weapon);
		StopPlaying();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMunsterKeepsItsBeatOnEveryMachine, "CheeseStrike.Play.TheMunsterKeepsItsBeatOnEveryMachine", GunfireBeatPlayTests::Flags)

bool FTheMunsterKeepsItsBeatOnEveryMachine::RunTest(const FString& Parameters)
{
	GunfireBeatPlayTests::Measure(*this, CheeseStrikeWeapons::Munster());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheBlasterKeepsItsBeatOnEveryMachine, "CheeseStrike.Play.TheBlasterKeepsItsBeatOnEveryMachine", GunfireBeatPlayTests::Flags)

bool FTheBlasterKeepsItsBeatOnEveryMachine::RunTest(const FString& Parameters)
{
	GunfireBeatPlayTests::Measure(*this, CheeseStrikeWeapons::Blaster());
	return true;
}

#endif
