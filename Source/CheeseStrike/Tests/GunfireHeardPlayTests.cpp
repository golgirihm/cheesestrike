// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Wave 16: gunfire and reloads went silent for good partway through a long
// match, while the hit's thud still played. This plays a long match on the
// shooter level against bots that fight back: weapon after weapon taken from
// stands (dual guns among them), automatic and single shots, reloads, the
// sidearm and the knife, hits landed (each a thud made in code, as many as a
// long match has), deaths and spawns; and asks the audio engine after every
// sound a weapon starts whether it got a voice and played (CheeseStrikeHeard).
// Every shot and every reload of the player's own must be heard, the last ones
// as well as the first, and once the shooting stops the voices must free up.
//
// It needs the audio engine: `python tools/run_tests.py --sound
// CheeseStrike.Play.GunfireIsHeard`. Run without (the suite runs -nosound),
// it plays the match and says it could not listen.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "EngineUtils.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeHeard.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikePickup.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace GunfireHeardPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** The shooter level's long clear run, as the bot tests have it. */
	const FVector Run(1150.0f, -1450.0f, 100.0f);
	constexpr float RunYaw = 117.0f;
	/** Weapon after weapon, the dual ones among them, each taken from a stand. */
	const TCHAR* const Weapons[] = {TEXT("munster"), TEXT("burratas"), TEXT("shotgun"), TEXT("carbine"), TEXT("grater"), TEXT("roquefort"), TEXT("munster"), TEXT("blaster"), TEXT("burratas"), TEXT("carbine")};
	/** Thuds of hits landed between two weapons: about a long match's worth over the whole (a hit thud each). */
	constexpr int32 HitsEachRound = 15;

	FTSTicker::FDelegateHandle Watch;
	bool bHasDevice = false;

	/** A stand holding `Weapon` on the floor at the player's feet, on the server. */
	void StandAtFeet(const TCHAR* Weapon)
	{
		UWorld* World = PlayTesting::World(0);
		const ACheeseStrikeCharacter* Player = PlayTesting::Character(0);
		if (World == nullptr || Player == nullptr)
		{
			return;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("GunfireHeardFloor")), false, Player);
		const FVector From = Player->GetActorLocation() + FRotator(0.0f, Player->GetControlRotation().Yaw, 0.0f).Vector() * 60.0f;
		const FTransform Where(World->LineTraceSingleByChannel(Hit, From, From - FVector(0.0f, 0.0f, 400.0f), ECC_Visibility, Params) ? Hit.ImpactPoint : From - FVector(0.0f, 0.0f, 90.0f));
		if (ACheeseStrikePickup* Stand = World->SpawnActorDeferred<ACheeseStrikePickup>(ACheeseStrikePickup::StaticClass(), Where))
		{
			Stand->SetWeapon(FName(Weapon));
			UGameplayStatics::FinishSpawningActor(Stand, Where);
		}
	}

	/** A round with one weapon: taken, fired in bursts and single shots, reloaded, the sidearm and the knife, back to it. */
	FString RoundScript()
	{
		FString Script = TEXT("tap Interact\nwait 0.7\nhold Fire\nwait 1.2\nrelease Fire\n");
		for (int32 Shot = 0; Shot < 6; ++Shot)
		{
			Script += TEXT("tap Fire\nwait 0.2\n");
		}
		Script += TEXT("tap Reload\nwait 3.2\nhold Fire\nwait 0.6\nrelease Fire\ntap Sidearm\nwait 0.7\n");
		for (int32 Shot = 0; Shot < 4; ++Shot)
		{
			Script += TEXT("tap Fire\nwait 0.2\n");
		}
		Script += TEXT("tap Reload\nwait 2.2\ntap Knife\nwait 0.6\ntap Fire\nwait 0.8\ntap Primary\nwait 0.8\nlook 25 0\n");
		return Script;
	}

	/** Thuds of hits landed, as the server confirms them to the player's own weapon. */
	void LandHits(int32 Count)
	{
		ACheeseStrikeCharacter* Player = PlayTesting::Character(0);
		ACheeseStrikeWeapon* Weapon = Player != nullptr ? Player->GetWeapon() : nullptr;
		for (int32 Hit = 0; Weapon != nullptr && Hit < Count; ++Hit)
		{
			Weapon->ConfirmHit(10.0f, false, Hit == Count - 1);
		}
	}

	const TCHAR* KindName(CheeseStrikeHeard::EKind Kind)
	{
		switch (Kind)
		{
		case CheeseStrikeHeard::EKind::OwnShot: return TEXT("own shot");
		case CheeseStrikeHeard::EKind::OwnHandling: return TEXT("own handling");
		case CheeseStrikeHeard::EKind::OthersShot: return TEXT("bot's shot");
		default: return TEXT("bot's handling");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGunfireIsHeardThroughALongMatch, "CheeseStrike.Play.GunfireIsHeardThroughALongMatch", GunfireHeardPlayTests::Flags)

bool FGunfireIsHeardThroughALongMatch::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace GunfireHeardPlayTests;
	FSession Session;
	Session.Options = TEXT("?Bots=3?BotSkill=1");
	StartPlaying(Session);
	PutAt(0, Run, RunYaw);
	Then([this]()
	{
		bHasDevice = World(0) != nullptr && World(0)->GetAudioDeviceRaw() != nullptr;
		if (!bHasDevice)
		{
			AddInfo(TEXT("no audio device (the tests run -nosound): the match is played but not listened to; run_tests.py --sound listens"));
		}
		CheeseStrikeHeard::Reset();
		CheeseStrikeHeard::bRecording = true;
		FTSTicker::GetCoreTicker().RemoveTicker(Watch);
		Watch = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			CheeseStrikeHeard::Poll(World(0));
			return true;
		}), 0.0f);
	});
	for (int32 Round = 0; Round < UE_ARRAY_COUNT(Weapons); ++Round)
	{
		WaitUntil(TEXT("the player alive"), 15.0f, []() { return Character(0) != nullptr && Character(0)->IsAlive(); });
		const TCHAR* Weapon = Weapons[Round];
		Then([Weapon]() { StandAtFeet(Weapon); });
		Play(0, RoundScript(), 40.0f);
		Then([]() { LandHits(HitsEachRound); });
		if (Round % 3 == 2)
		{
			// A death now and then: the weapons go with the body and come anew with the next character.
			Play(0, TEXT("cmd CheeseStrikeDie\nwait 0.5"));
		}
	}
	// Last, after all of that: a clean burst, single shots and a reload, each of which must be heard.
	WaitUntil(TEXT("the player alive at the end"), 15.0f, []() { return Character(0) != nullptr && Character(0)->IsAlive(); });
	Then([]() { StandAtFeet(TEXT("munster")); });
	Play(0, TEXT("tap Interact\nwait 0.8\nhold Fire\nwait 1.0\nrelease Fire\ntap Reload\nwait 3.0\ntap Sidearm\nwait 0.7\ntap Fire\nwait 0.3\ntap Fire\nwait 0.3\ntap Fire\nwait 1.0"));
	// Then nothing: whatever was started has ended, and the voices are free again.
	Then([]()
	{
		// The bots out of the match, so that nothing shoots on.
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			if (APawn* Body = It->GetPawn())
			{
				Body->Destroy();
			}
			It->Destroy();
		}
	});
	Wait(4.0f);
	Then([this]()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Watch);
		CheeseStrikeHeard::bRecording = false;
		const TPair<int32, int32> Voices = CheeseStrikeHeard::Voices(World(0));
		if (!bHasDevice)
		{
			AddInfo(FString::Printf(TEXT("%d sounds started, none listened to"), CheeseStrikeHeard::All().Num()));
			return;
		}
		const TArray<CheeseStrikeHeard::FHeard>& All = CheeseStrikeHeard::All();
		const double Now = FPlatformTime::Seconds();
		TMap<CheeseStrikeHeard::EKind, TPair<int32, int32>> Counts;
		TArray<const CheeseStrikeHeard::FHeard*> Own;
		TArray<FString> Unheard;
		for (const CheeseStrikeHeard::FHeard& Sound : All)
		{
			TPair<int32, int32>& Count = Counts.FindOrAdd(Sound.Kind);
			++Count.Key;
			Count.Value += Sound.Heard->load() ? 1 : 0;
			if (Sound.Kind == CheeseStrikeHeard::EKind::OwnShot || Sound.Kind == CheeseStrikeHeard::EKind::OwnHandling)
			{
				Own.Add(&Sound);
				if (!Sound.Heard->load())
				{
					Unheard.Add(FString::Printf(TEXT("%s %s %.1f s before the end"), KindName(Sound.Kind), *Sound.What, Now - Sound.At));
				}
			}
		}
		for (const TPair<CheeseStrikeHeard::EKind, TPair<int32, int32>>& Count : Counts)
		{
			AddInfo(FString::Printf(TEXT("%s: %d of %d heard"), KindName(Count.Key), Count.Value.Value, Count.Value.Key));
		}
		AddInfo(FString::Printf(TEXT("voices after the shooting: %d of %d"), Voices.Key, Voices.Value));
		if (Unheard.Num() > 0)
		{
			AddInfo(FString::Printf(TEXT("unheard: %s"), *FString::Join(Unheard, TEXT("; "))));
		}
		const TPair<int32, int32> Shots = Counts.FindRef(CheeseStrikeHeard::EKind::OwnShot);
		const TPair<int32, int32> Handling = Counts.FindRef(CheeseStrikeHeard::EKind::OwnHandling);
		TestTrue(FString::Printf(TEXT("a long match's shots were fired (%d)"), Shots.Key), Shots.Key >= 150);
		TestTrue(FString::Printf(TEXT("and reloads and draws handled (%d)"), Handling.Key), Handling.Key >= 40);
		// A shot queued on the beat whose gun is dropped or dies before its tick is rightly never heard; nothing else may be.
		TestTrue(FString::Printf(TEXT("every shot of the player's own heard but a gun gone before its beat (%d of %d)"), Shots.Value, Shots.Key), Shots.Key - Shots.Value <= FMath::Max(2, Shots.Key / 100));
		TestTrue(FString::Printf(TEXT("every reload, dry fire and draw heard but one cut short (%d of %d)"), Handling.Value, Handling.Key), Handling.Key - Handling.Value <= FMath::Max(2, Handling.Key / 50));
		// Never for good: the last of them, after the whole match, all heard.
		int32 LastUnheard = 0;
		for (int32 Index = FMath::Max(0, Own.Num() - 25); Index < Own.Num(); ++Index)
		{
			LastUnheard += Own[Index]->Heard->load() ? 0 : 1;
		}
		TestEqual(TEXT("the last 25 of the player's own weapon sounds all heard"), LastUnheard, 0);
		const TPair<int32, int32> Theirs = Counts.FindRef(CheeseStrikeHeard::EKind::OthersShot);
		TestTrue(FString::Printf(TEXT("the bots' shots heard too, but for the farthest when many play at once (%d of %d)"), Theirs.Value, Theirs.Key), Theirs.Key == 0 || Theirs.Value >= Theirs.Key * 0.8);
		// Nothing held: a few footsteps or a last echo at most, never the mixer's voices filled with sounds that never end.
		TestTrue(FString::Printf(TEXT("the voices free once the shooting stops (%d of %d)"), Voices.Key, Voices.Value), Voices.Key >= 0 && Voices.Key <= 8);
	});
	StopPlaying();
	return true;
}

#endif
