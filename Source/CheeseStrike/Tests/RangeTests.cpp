// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The Practice Range (Rules/CheeseStrikeRange.h): its rules without a world,
// and a play of it as the front end's PRACTICE plays it: its stands never
// empty, its dummies standing still where the map puts them and coming back
// there, and its cheese targets breaking and growing back.

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Rules/CheeseStrikeBots.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikePickups.h"
#include "Rules/CheeseStrikeRange.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStart.h"
#include "Rules/CheeseStrikeMice.h"
#include "Weapons/CheeseStrikeArsenal.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RangeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePracticeRange, "CheeseStrike.Rules.ThePracticeRange", RangeTests::Flags)

bool FThePracticeRange::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRange;
	// PRACTICE: the range alone, at once, a free-for-all with its dummies, no mice, no count, and no end of its own.
	const FString Options = PracticeOptions();
	TestTrue(TEXT("the URL is the range's level"), PracticeUrl().StartsWith(Level));
	TestTrue(TEXT("a free-for-all"), CheeseStrikeModes::ModeFrom(Options) == CheeseStrikeModes::EMode::FreeForAll);
	TestTrue(TEXT("started at once, no lobby"), CheeseStrikeRules::StartsAtOnce(Options));
	TestEqual(TEXT("with its four dummies"), CheeseStrikeRules::BotsFrom(Options), Dummies);
	TestTrue(TEXT("dummies"), CheeseStrikeBots::DummiesFrom(Options));
	TestFalse(TEXT("no mice"), CheeseStrikeMice::HasMice(Options));
	TestEqual(TEXT("no count before it"), CheeseStrikeStart::CountdownFrom(Options), 0.0f);
	const int32 Limit = CheeseStrikeModes::KillLimitFor(CheeseStrikeModes::EMode::FreeForAll, Options);
	TestTrue(FString::Printf(TEXT("a kill limit nobody reaches (%d)"), Limit), Limit >= 1000);
	TestTrue(TEXT("the range is a map a host can pick"), CheeseStrikeRules::MapIndexFor(Level) != INDEX_NONE);

	// The dummies take the spots in turn, each keeping its own.
	TestEqual(TEXT("the first dummy, the first spot"), DummySpotFor({}, 4), 0);
	TestEqual(TEXT("the next, the first nobody holds"), DummySpotFor({0, 2}, 4), 1);
	TestEqual(TEXT("and after it"), DummySpotFor({0, 1, 2}, 4), 3);
	TestEqual(TEXT("more dummies than spots share them"), DummySpotFor({0, 1, 2, 3}, 4), 0);
	TestEqual(TEXT("a map with none has none"), DummySpotFor({}, 0), static_cast<int32>(INDEX_NONE));

	// A stand that is instant is back as soon as it is taken; any other after its seconds.
	TestEqual(TEXT("an instant stand is never empty"), CheeseStrikePickups::BackAt(100.0f, 60.0f, true), 0.0f);
	TestEqual(TEXT("and shows no seconds"), CheeseStrikePickups::SecondsLeft(100.0f, CheeseStrikePickups::BackAt(100.0f, 60.0f, true)), 0.0f);
	TestEqual(TEXT("another is back after its seconds"), CheeseStrikePickups::BackAt(100.0f, 60.0f, false), 160.0f);
	// Every weapon that lies on a stand has one on the range: all of them but the Colby Carbine everyone comes in with.
	int32 OnStands = 0;
	for (const FName& Weapon : CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary))
	{
		OnStands += CheeseStrikePickups::CanLieOnAStand(Weapon) ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("the weapons that lie on stands fit a map's stands (%d)"), OnStands), OnStands >= CheeseStrikePickups::FewestSpots && OnStands <= CheeseStrikePickups::MostSpots);

	// The targets: their holes grow as they are hit, and they grow back from a seed.
	TestEqual(TEXT("a target whole"), TargetStage(TargetHealth), 0);
	TestEqual(TEXT("hit once"), TargetStage(TargetHealth * 0.6f), 1);
	TestEqual(TEXT("nearly broken"), TargetStage(TargetHealth * 0.2f), 2);
	TestTrue(TEXT("growing back, it starts small"), GrownShare(0.0f) > 0.0f && GrownShare(0.0f) < 0.2f);
	TestTrue(TEXT("and grows"), GrownShare(GrowSeconds * 0.5f) > GrownShare(0.0f));
	TestEqual(TEXT("to whole"), GrownShare(GrowSeconds), 1.0f);
	TestTrue(TEXT("a few seconds after it broke"), RegrowSeconds >= 2.0f && RegrowSeconds <= 5.0f);
	return true;
}

#if WITH_EDITOR

#include "EngineUtils.h"
#include "Engine/World.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Map/CheeseStrikeTarget.h"
#include "PlayTesting.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikePickup.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace RangeTests
{
	TArray<ACheeseStrikeBotController*> Dummies()
	{
		TArray<ACheeseStrikeBotController*> Found;
		for (TActorIterator<ACheeseStrikeBotController> It(PlayTesting::World(0)); It; ++It)
		{
			Found.Add(*It);
		}
		return Found;
	}

	ACheeseStrikePickup* StandOf(FName Weapon)
	{
		for (TActorIterator<ACheeseStrikePickup> It(PlayTesting::World(0)); It; ++It)
		{
			if (!It->IsDropped() && It->GetWeapon() == Weapon)
			{
				return *It;
			}
		}
		return nullptr;
	}

	FName PrimaryOf(const ACheeseStrikeCharacter* Character)
	{
		const ACheeseStrikeWeapon* Primary = Character != nullptr && Character->GetInventory() != nullptr ? Character->GetInventory()->GetInSlot(EWeaponSlot::Primary) : nullptr;
		return Primary != nullptr ? Primary->GetWeaponId() : NAME_None;
	}

	/** Where each dummy came in, by its spot. */
	TMap<int32, FVector> CameIn;
	TWeakObjectPtr<ACheeseStrikeBotController> Killed;
	TWeakObjectPtr<APawn> KilledBody;
	TWeakObjectPtr<ACheeseStrikeTarget> Target;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePracticeRangeNeverEmpties, "CheeseStrike.Play.ThePracticeRangeNeverEmpties", RangeTests::Flags)

bool FThePracticeRangeNeverEmpties::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace RangeTests;
	CameIn.Reset();
	FSession Session;
	Session.Level = CheeseStrikeRange::Level;
	Session.Options = CheeseStrikeRange::PracticeOptions();
	// The game's own spawn primary, the Colby Carbine, which no stand holds.
	Session.SpawnPrimary = FString();
	StartPlaying(Session);
	WaitUntil(TEXT("the four dummies in"), 15.0f, []()
	{
		int32 In = 0;
		for (const ACheeseStrikeBotController* Bot : Dummies())
		{
			In += Bot->GetPawn() != nullptr ? 1 : 0;
		}
		return In == CheeseStrikeRange::Dummies;
	});
	Then([this]()
	{
		// Each a dummy, at a spot of its own.
		TArray<AActor*> Spots;
		for (TActorIterator<AActor> It(World(0)); It; ++It)
		{
			if (It->ActorHasTag(FName(CheeseStrikeRange::DummySpotTag)))
			{
				Spots.Add(*It);
			}
		}
		TestEqual(TEXT("the range has a spot for each dummy"), Spots.Num(), CheeseStrikeRange::Dummies);
		for (const ACheeseStrikeBotController* Bot : Dummies())
		{
			const FVector At = Bot->GetPawn()->GetActorLocation();
			TestTrue(TEXT("a dummy"), Bot->IsDummy());
			TestFalse(FString::Printf(TEXT("its spot its own (%d)"), Bot->GetDummySpot()), CameIn.Contains(Bot->GetDummySpot()));
			CameIn.Add(Bot->GetDummySpot(), At);
			const bool bAtASpot = Spots.ContainsByPredicate([&At](const AActor* Spot) { return FVector::Dist2D(Spot->GetActorLocation(), At) < 60.0f; });
			TestTrue(FString::Printf(TEXT("standing at a dummy spot (%s)"), *At.ToCompactString()), bAtASpot);
		}
		// A stand for every weapon that lies on one, each instant, each there.
		TArray<FName> Wanted;
		for (const FName& Weapon : CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary))
		{
			if (CheeseStrikePickups::CanLieOnAStand(Weapon))
			{
				Wanted.Add(Weapon);
			}
		}
		for (const FName& Weapon : Wanted)
		{
			const ACheeseStrikePickup* Stand = StandOf(Weapon);
			if (TestNotNull(FString::Printf(TEXT("a stand of the %s"), *Weapon.ToString()), Stand))
			{
				TestTrue(FString::Printf(TEXT("the %s's stand is instant"), *Weapon.ToString()), Stand->IsInstant());
				TestTrue(FString::Printf(TEXT("the %s's stand holds it"), *Weapon.ToString()), Stand->IsThere());
			}
		}
	});
	// The dummies don't move, turn or fire.
	Wait(2.0f);
	Then([this]()
	{
		for (const ACheeseStrikeBotController* Bot : Dummies())
		{
			const ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(Bot->GetPawn());
			const FVector* Was = CameIn.Find(Bot->GetDummySpot());
			TestTrue(FString::Printf(TEXT("dummy %d stays where it came in"), Bot->GetDummySpot()), Body != nullptr && Was != nullptr && FVector::Dist2D(Body->GetActorLocation(), *Was) < 20.0f);
			TestTrue(TEXT("its trigger never held"), Body != nullptr && (Body->GetWeapon() == nullptr || !Body->GetWeapon()->IsFiring()));
			TestTrue(TEXT("still"), Body != nullptr && Body->GetVelocity().Size2D() < 5.0f);
		}
	});

	// Every stand in turn: pressed for, its weapon in hand, and the stand never empty.
	for (const FName& Weapon : CheeseStrikeArsenal::InSlot(EWeaponSlot::Primary))
	{
		if (!CheeseStrikePickups::CanLieOnAStand(Weapon))
		{
			continue;
		}
		Then([Weapon]()
		{
			if (const ACheeseStrikePickup* Stand = StandOf(Weapon))
			{
				Character(0)->StartAt(Stand->FloorAt() + FVector(-30.0f, 0.0f, 100.0f), 0.0f);
				Controller(0)->SetControlRotation(FRotator::ZeroRotator);
			}
		});
		Wait(0.4f);
		Play(0, TEXT("tap Interact\nwait 0.4"));
		Then([this, Weapon]()
		{
			const ACheeseStrikePickup* Stand = StandOf(Weapon);
			TestEqual(FString::Printf(TEXT("pressed, the %s is taken"), *Weapon.ToString()), PrimaryOf(Character(0)), Weapon);
			TestTrue(FString::Printf(TEXT("and its stand still holds one (%.1f s left)"), Stand != nullptr ? Stand->SecondsLeft() : -1.0f), Stand != nullptr && Stand->IsThere() && Stand->SecondsLeft() == 0.0f);
		});
	}
	Then([this]()
	{
		for (TActorIterator<ACheeseStrikePickup> It(World(0)); It; ++It)
		{
			TestTrue(FString::Printf(TEXT("every stand still full: the %s's"), *It->GetWeapon().ToString()), It->IsDropped() || It->IsThere());
		}
	});

	// A dummy killed comes back where it stood.
	Then([this]()
	{
		ACheeseStrikeBotController* Bot = Dummies().Num() > 0 ? Dummies()[0] : nullptr;
		ACheeseStrikeCharacter* Body = Bot != nullptr ? Cast<ACheeseStrikeCharacter>(Bot->GetPawn()) : nullptr;
		if (TestNotNull(TEXT("a dummy to kill"), Body))
		{
			Killed = Bot;
			KilledBody = Body;
			Body->CheeseStrikeDie();
			TestFalse(TEXT("killed"), Body->IsAlive());
		}
	});
	WaitUntil(TEXT("the dummy coming back"), CheeseStrikeRange::RespawnDelay + 5.0f, []()
	{
		const ACheeseStrikeCharacter* Body = Killed.IsValid() ? Cast<ACheeseStrikeCharacter>(Killed->GetPawn()) : nullptr;
		return Body != nullptr && Body != KilledBody.Get() && Body->IsAlive();
	});
	Then([this]()
	{
		const APawn* Body = Killed->GetPawn();
		const FVector* Was = CameIn.Find(Killed->GetDummySpot());
		TestTrue(FString::Printf(TEXT("back where it stood (%s)"), *Body->GetActorLocation().ToCompactString()), Was != nullptr && FVector::Dist2D(Body->GetActorLocation(), *Was) < 60.0f);
		TestTrue(TEXT("still a dummy"), Killed->IsDummy());
	});

	// A cheese target: hit, it shows it; broken, it is gone and blocks nothing; it grows back.
	Then([this]()
	{
		for (TActorIterator<ACheeseStrikeTarget> It(World(0)); It; ++It)
		{
			Target = *It;
		}
		if (!TestTrue(TEXT("a cheese target"), Target.IsValid()))
		{
			return;
		}
		const FVector Way = (Target->GetActorLocation() - Character(0)->GetActorLocation()).GetSafeNormal();
		FHitResult Hit(Target.Get(), nullptr, Target->GetActorLocation(), -Way);
		UGameplayStatics::ApplyPointDamage(Target.Get(), CheeseStrikeRange::TargetHealth * 0.5f, Way, Hit, Controller(0), Character(0), nullptr);
		TestEqual(TEXT("hit, its holes grow"), Target->GetStage(), 1);
		TestFalse(TEXT("but it stands"), Target->IsBroken());
		UGameplayStatics::ApplyPointDamage(Target.Get(), CheeseStrikeRange::TargetHealth * 0.5f, Way, Hit, Controller(0), Character(0), nullptr);
		TestTrue(TEXT("hit again, it breaks"), Target->IsBroken());
		TestTrue(TEXT("gone"), Target->IsHidden() && !Target->GetActorEnableCollision());
	});
	WaitUntil(TEXT("the target growing back"), CheeseStrikeRange::RegrowSeconds + 3.0f, []()
	{
		return Target.IsValid() && !Target->IsBroken() && !Target->IsHidden();
	});
	Then([this]()
	{
		TestTrue(TEXT("whole again, and blocking shots"), Target->GetActorEnableCollision() && Target->GetHealth() == CheeseStrikeRange::TargetHealth && Target->GetStage() == 0);
		TestTrue(TEXT("the practice goes on"), GameState(0)->IsMatchInProgress());
	});
	StopPlaying();
	return true;
}

#endif
#endif
