// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Tests that play the weapons' stands in the editor (see PlayTesting.h), on
// the shooter level's long clear run with stands spawned on it: nothing is
// taken by walking over it, even with the spawn primary; Interact pressed
// takes it, on the server and the player's machine, the weapon carried
// dropping to the floor with its rounds, where another player takes it
// again with the same press; the stand is seen empty by a joiner and
// comes back; one's own weapon tops up its rounds; Gun Game has none; and
// a bot goes for a better weapon near it, pressing for it.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "Map/CheeseStrikeHill.h"
#include "Rules/CheeseStrikeHills.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "PickupChecks.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikePickups.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikePickup.h"
#include "Weapons/CheeseStrikeWeapon.h"

namespace PickupPlayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/** The shooter level's long clear run (as the bot tests'): a point `Along` cm down it. */
	FVector OnTheRun(float Along)
	{
		return FVector(1150.0f, -1450.0f, 100.0f) + FRotator(0.0f, 117.0f, 0.0f).Vector() * Along;
	}
	constexpr float RunYaw = 117.0f;
	/** Seconds a stand stays empty in these tests, rather than half a minute. */
	constexpr float TestSeconds = 4.0f;

	void SetPickupSeconds(float Seconds)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("CheeseStrike.PickupSeconds")))
		{
			Variable->Set(Seconds);
		}
	}

	/** The floor under `At` in the host's world. */
	FVector FloorUnder(const FVector& At)
	{
		UWorld* World = PlayTesting::World(0);
		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("PickupFloor")), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		const FVector From(At.X, At.Y, At.Z + 200.0f);
		return World != nullptr && World->LineTraceSingleByChannel(Hit, From, From - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, Params) ? Hit.ImpactPoint : FVector(At.X, At.Y, 0.0f);
	}

	/** Server: a stand holding `Weapon` on the floor under `At`. */
	ACheeseStrikePickup* SpawnStand(FName Weapon, const FVector& At)
	{
		UWorld* World = PlayTesting::World(0);
		const FTransform Where(FloorUnder(At));
		ACheeseStrikePickup* Stand = World != nullptr ? World->SpawnActorDeferred<ACheeseStrikePickup>(ACheeseStrikePickup::StaticClass(), Where) : nullptr;
		if (Stand != nullptr)
		{
			Stand->SetWeapon(Weapon);
			UGameplayStatics::FinishSpawningActor(Stand, Where);
		}
		return Stand;
	}

	/** `Player`'s world's copy of the stand holding `Weapon`, or null. */
	ACheeseStrikePickup* StandIn(int32 Player, FName Weapon)
	{
		UWorld* World = PlayTesting::World(Player);
		for (TActorIterator<ACheeseStrikePickup> It(World); World != nullptr && It; ++It)
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

	FName HeldId(const ACheeseStrikeCharacter* Character)
	{
		return Character != nullptr && Character->GetWeapon() != nullptr ? Character->GetWeapon()->GetWeaponId() : NAME_None;
	}

	/** `Player`'s world's copy of a weapon `Weapon` on the floor, or null. */
	ACheeseStrikePickup* DroppedIn(int32 Player, FName Weapon)
	{
		UWorld* World = PlayTesting::World(Player);
		for (TActorIterator<ACheeseStrikePickup> It(World); World != nullptr && It; ++It)
		{
			if (It->IsDropped() && !It->IsActorBeingDestroyed() && It->GetWeapon() == Weapon)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Remembered between steps. */
	TWeakObjectPtr<ACheeseStrikeWeapon> Swapped;
	int32 ReserveBefore = 0;
	int32 MagazineBefore = 0;
	FVector DroppedAt = FVector::ZeroVector;
}

void PickupChecks::CheckStands(FAutomationTestBase& Test, UWorld* World, bool bAfterTheMice)
{
	using namespace CheeseStrikePickups;
	if (World == nullptr)
	{
		return;
	}
	TArray<ACheeseStrikePickup*> Stands;
	for (TActorIterator<ACheeseStrikePickup> It(World); It; ++It)
	{
		if (!It->IsDropped())
		{
			Stands.Add(*It);
		}
	}
	FCollisionQueryParams Params(FName(TEXT("PickupChecks")), false);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	for (const ACheeseStrikePickup* Stand : Stands)
	{
		const FVector Floor = Stand->FloorAt();
		FHitResult Hit;
		const bool bFloor = World->LineTraceSingleByChannel(Hit, Floor + FVector(0.0f, 0.0f, 50.0f), Floor - FVector(0.0f, 0.0f, 50.0f), ECC_Pawn, Params);
		Test.TestTrue(FString::Printf(TEXT("%s stands on its floor%s (%.0f cm under it)"), *Stand->GetActorLabel(), bAfterTheMice ? TEXT(" after the mice") : TEXT(""), bFloor ? Hit.Distance - 50.0f : -1.0f),
			bFloor && FMath::Abs(Hit.Distance - 50.0f) < 15.0f);
	}
	if (bAfterTheMice)
	{
		return;
	}
	// The Proving Ground, the template's shooter level, has none: the play tests walk all over it.
	if (World->GetMapName().Contains(TEXT("Lvl_Shooter")))
	{
		Test.TestEqual(TEXT("no weapons' stands on the Proving Ground"), Stands.Num(), 0);
		return;
	}
	Test.TestTrue(FString::Printf(TEXT("%d to %d weapons' stands (%d)"), FewestSpots, MostSpots, Stands.Num()), Stands.Num() >= FewestSpots && Stands.Num() <= MostSpots);
	TArray<FSpot> Spots;
	TArray<FVector> Gouda, Mozza;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->PlayerStartTag == FName(TEXT("Gouda")))
		{
			Gouda.Add(It->GetActorLocation());
		}
		else if (It->PlayerStartTag == FName(TEXT("Mozza")))
		{
			Mozza.Add(It->GetActorLocation());
		}
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation From;
	const bool bFrom = Navigation != nullptr && Gouda.Num() > 0 && Navigation->ProjectPointToNavigation(Gouda[0], From, FVector(100.0f, 100.0f, 250.0f));
	for (const ACheeseStrikePickup* Stand : Stands)
	{
		const FName Label(*Stand->GetActorLabel());
		Spots.Add({Stand->FloorAt(), Stand->GetWeapon()});
		Test.TestTrue(FString::Printf(TEXT("%s holds a weapon for a stand (%s)"), *Label.ToString(), *Stand->GetWeapon().ToString()), CanLieOnAStand(Stand->GetWeapon()));
		// Room for a player standing over it: nothing that blocks them there.
		const FVector Standing = Stand->FloorAt() + FVector(0.0f, 0.0f, 95.0f);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, Standing, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f), Params);
		FString Blocking;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.bBlockingHit && Overlap.GetActor() != nullptr)
			{
				Blocking += Overlap.GetActor()->GetActorLabel() + TEXT(" ");
			}
		}
		Test.TestTrue(FString::Printf(TEXT("room for a player over %s (%s)"), *Label.ToString(), Blocking.IsEmpty() ? TEXT("clear") : *Blocking), Blocking.IsEmpty());
		// Clear of every hill's spot (King of the Hill): its plinth outside the hill's round and height.
		for (TActorIterator<ACheeseStrikeHillSpot> Hill(World); Hill; ++Hill)
		{
			const FVector Spot = Hill->GetActorLocation();
			const float Rise = Stand->FloorAt().Z - Spot.Z;
			const bool bInside = FVector::Dist2D(Spot, Stand->FloorAt()) < Hill->GetRadius() + 55.0f && Rise > -CheeseStrikeHills::Below && Rise < CheeseStrikeHills::Above;
			Test.TestFalse(FString::Printf(TEXT("%s is clear of the hill %s"), *Label.ToString(), *Hill->GetActorLabel()), bInside);
		}
		// A way to it from a side's start.
		FNavLocation To;
		const bool bOnMesh = Navigation != nullptr && Navigation->ProjectPointToNavigation(Stand->FloorAt(), To, FVector(60.0f, 60.0f, 150.0f));
		UNavigationPath* Path = bFrom && bOnMesh ? Navigation->FindPathToLocationSynchronously(World, From.Location, To.Location) : nullptr;
		Test.TestTrue(FString::Printf(TEXT("a way to %s (%s)"), *Label.ToString(), !bOnMesh ? TEXT("off the navigation mesh") : Path == nullptr || !Path->IsValid() ? TEXT("none") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
			Path != nullptr && Path->IsValid() && !Path->IsPartial());
	}
	FString Why;
	const bool bFair = AreFair(Spots, Gouda, Mozza, Why);
	Test.TestTrue(FString::Printf(TEXT("the stands are fair to both sides%s%s"), Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why), bFair);
	Why.Reset();
	const bool bRiskier = BetterLieRiskier(Spots, Gouda, Mozza, Why);
	Test.TestTrue(FString::Printf(TEXT("the better weapons lie at the riskier spots%s%s"), Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why), bRiskier);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAWeaponIsTakenFromItsStandAndComesBack, "CheeseStrike.Play.AWeaponIsTakenFromItsStandAndComesBack", PickupPlayTests::Flags)

bool FAWeaponIsTakenFromItsStandAndComesBack::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace PickupPlayTests;
	const FName Roquefort = CheeseStrikeWeapons::Roquefort().Id;
	const FName Blaster = CheeseStrikeWeapons::Blaster().Id;
	const FName Carbine = CheeseStrikeWeapons::Carbine().Id;
	FSession Session;
	Session.Players = 2;
	Session.Options = TEXT("?RespawnDelay=1");
	// The game's own spawn primary, the Colby Carbine.
	Session.SpawnPrimary = FString();
	SetPickupSeconds(TestSeconds);
	StartPlaying(Session);
	// The joiner well down the run, out of the way; the host 3 m short of the Roquefort's stand.
	PutAt(1, OnTheRun(1100.0f), RunYaw + 180.0f);
	PutAt(0, OnTheRun(0.0f), RunYaw);
	Then([this, Roquefort, Blaster, Carbine]()
	{
		TestEqual(TEXT("the host comes in with the Colby Carbine"), PrimaryOf(CharacterOf(0, 0)), Carbine);
		TestEqual(TEXT("so does the joiner"), PrimaryOf(CharacterOf(1, 0)), Carbine);
		TestNotNull(TEXT("a Roquefort's stand"), SpawnStand(Roquefort, OnTheRun(300.0f)));
		TestNotNull(TEXT("and a Blaster's"), SpawnStand(Blaster, OnTheRun(600.0f)));
	});
	WaitUntil(TEXT("the joiner seeing both stands, their weapons on them"), 5.0f, [Roquefort, Blaster]()
	{
		const ACheeseStrikePickup* A = StandIn(1, Roquefort);
		const ACheeseStrikePickup* B = StandIn(1, Blaster);
		return A != nullptr && B != nullptr && A->IsThere() && B->IsThere() && !A->IsHidden();
	});
	Then([this, Roquefort]()
	{
		const ACheeseStrikePickup* Stand = StandIn(0, Roquefort);
		TestTrue(TEXT("3 m off, nothing is taken"), PrimaryOf(CharacterOf(0, 0)) == CheeseStrikeWeapons::Carbine().Id && Stand->IsThere());
		TestTrue(TEXT("with only the spawn primary, it is swapped for"), Stand->HowFor(CharacterOf(0, 0)) == CheeseStrikePickups::ETake::Swap);
		TestNull(TEXT("and out of reach no prompt asks to press for it"), ACheeseStrikePickup::OfferedTo(Character(0)));
	});

	// Walked over: never taken; Interact pressed, it is taken, the carbine dropped.
	PutAt(0, OnTheRun(300.0f), RunYaw);
	Wait(0.8f);
	Then([this, Roquefort, Carbine]()
	{
		TestEqual(TEXT("walking over the stand takes nothing, even with the spawn primary"), PrimaryOf(CharacterOf(0, 0)), Carbine);
		TestTrue(TEXT("the Roquefort stays on its stand"), StandIn(0, Roquefort)->IsThere());
		TestTrue(TEXT("the host's machine offers it, to press for"), ACheeseStrikePickup::OfferedTo(Character(0)) == StandIn(0, Roquefort));
	});
	Play(0, TEXT("tap Interact\nwait 0.4"));
	Then([this, Roquefort, Carbine]()
	{
		TestEqual(TEXT("pressed, Interact takes the Roquefort"), PrimaryOf(CharacterOf(0, 0)), Roquefort);
		TestEqual(TEXT("in hand"), HeldId(CharacterOf(0, 0)), Roquefort);
		TestFalse(TEXT("the stand is empty"), StandIn(0, Roquefort)->IsThere());
		TestTrue(FString::Printf(TEXT("for its seconds (%.1f)"), StandIn(0, Roquefort)->SecondsLeft()), StandIn(0, Roquefort)->SecondsLeft() > TestSeconds - 1.5f);
		const ACheeseStrikePickup* Dropped = DroppedIn(0, Carbine);
		TestTrue(TEXT("the Colby Carbine lies on the floor"), Dropped != nullptr && Dropped->IsThere());
		TestTrue(TEXT("near the stand"), Dropped != nullptr && FVector::Dist2D(Dropped->FloorAt(), OnTheRun(300.0f)) < 150.0f);
	});
	WaitUntil(TEXT("the host's own machine holding the Roquefort"), 3.0f, [Roquefort]() { return HeldId(Character(0)) == Roquefort; });
	WaitUntil(TEXT("the joiner seeing the stand empty, the host holding it and the carbine on the floor"), 3.0f, [Roquefort, Carbine]()
	{
		const ACheeseStrikePickup* Stand = StandIn(1, Roquefort);
		return Stand != nullptr && !Stand->IsThere() && Stand->SecondsLeft() > 0.0f && HeldId(CharacterOf(0, 1)) == Roquefort && DroppedIn(1, Carbine) != nullptr;
	});

	// Over the Blaster's stand with the Roquefort: nothing by walking; pressed, a swap, the Roquefort dropped with its rounds.
	PutAt(0, OnTheRun(600.0f), RunYaw);
	Wait(0.6f);
	Then([this, Roquefort, Blaster]()
	{
		TestEqual(TEXT("walking over a weapon with a better one swaps nothing"), PrimaryOf(CharacterOf(0, 0)), Roquefort);
		TestTrue(TEXT("the Blaster stays on its stand"), StandIn(0, Blaster)->IsThere());
		TestTrue(TEXT("the host's machine offers it, to press for"), ACheeseStrikePickup::OfferedTo(Character(0)) == StandIn(0, Blaster));
		Swapped = CharacterOf(0, 0)->GetInventory()->GetInSlot(EWeaponSlot::Primary);
		MagazineBefore = Swapped.IsValid() ? Swapped->GetMagazine() - 1 : 0;
		ReserveBefore = Swapped.IsValid() ? Swapped->GetReserve() : 0;
	});
	// One shot gone, so that its rounds are its own.
	Play(0, TEXT("look 0 -30\ntap Fire\nwait 1.5\nlook 0 30\ntap Interact\nwait 0.4"));
	Then([this, Roquefort, Blaster]()
	{
		TestEqual(TEXT("pressed, Interact swaps the Roquefort for the Blaster"), PrimaryOf(CharacterOf(0, 0)), Blaster);
		TestEqual(TEXT("in hand"), HeldId(CharacterOf(0, 0)), Blaster);
		TestFalse(TEXT("the Roquefort carried is gone"), Swapped.IsValid() && !Swapped->IsActorBeingDestroyed());
		TestFalse(TEXT("and the Blaster's stand is empty"), StandIn(0, Blaster)->IsThere());
		TestTrue(TEXT("the pistol and the knife are still carried"), CharacterOf(0, 0)->GetInventory()->CanDraw(EWeaponSlot::Sidearm) && CharacterOf(0, 0)->GetInventory()->CanDraw(EWeaponSlot::Knife));
		const ACheeseStrikePickup* Dropped = DroppedIn(0, Roquefort);
		if (TestNotNull(TEXT("the Roquefort lies on the floor"), Dropped))
		{
			TestEqual(TEXT("with the rounds in its magazine"), Dropped->GetDroppedMagazine(), MagazineBefore);
			TestEqual(TEXT("and in reserve"), Dropped->GetDroppedReserve(), ReserveBefore);
		}
	});
	WaitUntil(TEXT("the host's own machine holding the Blaster"), 3.0f, [Blaster]() { return HeldId(Character(0)) == Blaster; });

	// The dropped Roquefort taken again, by the joiner, with the same press: its rounds as they were.
	PutAt(0, OnTheRun(0.0f), RunYaw);
	Then([this, Roquefort]()
	{
		const ACheeseStrikePickup* Dropped = DroppedIn(0, Roquefort);
		DroppedAt = Dropped != nullptr ? Dropped->FloorAt() : OnTheRun(650.0f);
	});
	WaitUntil(TEXT("the joiner seeing the Roquefort on the floor"), 3.0f, [Roquefort]() { return DroppedIn(1, Roquefort) != nullptr; });
	Then([]()
	{
		// Where it lies, found only now: moved as PutAt moves a player.
		CharacterOf(1, 0)->StartAt(DroppedAt + FRotator(0.0f, RunYaw, 0.0f).Vector() * 30.0f + FVector(0.0f, 0.0f, 100.0f), RunYaw + 180.0f);
		Controller(1)->SetControlRotation(FRotator(0.0f, RunYaw + 180.0f, 0.0f));
	});
	Wait(0.5f);
	Play(1, TEXT("tap Interact\nwait 0.4"));
	Then([this, Roquefort]()
	{
		const ACheeseStrikeWeapon* Primary = CharacterOf(1, 0)->GetInventory()->GetInSlot(EWeaponSlot::Primary);
		TestEqual(TEXT("the joiner takes the dropped Roquefort with a press"), PrimaryOf(CharacterOf(1, 0)), Roquefort);
		TestTrue(FString::Printf(TEXT("its magazine as it was dropped (%d)"), Primary != nullptr ? Primary->GetMagazine() : -1), Primary != nullptr && Primary->GetMagazine() == MagazineBefore);
		TestTrue(TEXT("and its reserve"), Primary != nullptr && Primary->GetReserve() == ReserveBefore);
		TestNull(TEXT("it is gone from the floor"), DroppedIn(0, Roquefort));
		TestNotNull(TEXT("and the joiner's carbine lies there in its place"), DroppedIn(0, CheeseStrikeWeapons::Carbine().Id));
	});
	WaitUntil(TEXT("the joiner's machine seeing it gone from the floor, and the Roquefort in hand"), 3.0f, [Roquefort]()
	{
		return DroppedIn(1, Roquefort) == nullptr && HeldId(Character(1)) == Roquefort;
	});
	PutAt(1, OnTheRun(1100.0f), RunYaw + 180.0f);

	// Back after its seconds, on both machines.
	WaitUntil(TEXT("the Roquefort back on its stand"), TestSeconds + 3.0f, [Roquefort]() { return StandIn(0, Roquefort)->IsThere(); });
	WaitUntil(TEXT("the joiner seeing it back"), 3.0f, [Roquefort]() { return StandIn(1, Roquefort) != nullptr && StandIn(1, Roquefort)->IsThere(); });

	// One's own weapon: pressed for, it tops up its rounds, the weapon kept.
	WaitUntil(TEXT("the Blaster back on its stand"), TestSeconds + 3.0f, [Blaster]() { return StandIn(0, Blaster)->IsThere(); });
	Then([this]()
	{
		Swapped = CharacterOf(0, 0)->GetInventory()->GetInSlot(EWeaponSlot::Primary);
		ReserveBefore = Swapped.IsValid() ? Swapped->GetReserve() : 0;
		TestTrue(TEXT("the host's rounds aren't full"), Swapped.IsValid() && ReserveBefore < Swapped->GetStats().ReserveMax);
	});
	PutAt(0, OnTheRun(600.0f), RunYaw);
	Wait(0.6f);
	Then([this, Blaster]()
	{
		TestTrue(TEXT("walking over a Blaster with a Blaster takes nothing"), StandIn(0, Blaster)->IsThere());
	});
	Play(0, TEXT("tap Interact\nwait 0.4"));
	Then([this, Blaster]()
	{
		const ACheeseStrikeWeapon* Primary = CharacterOf(0, 0)->GetInventory()->GetInSlot(EWeaponSlot::Primary);
		TestTrue(TEXT("pressing for a Blaster with a Blaster keeps the one carried"), Primary != nullptr && Primary == Swapped.Get());
		TestTrue(FString::Printf(TEXT("and tops up its rounds (%d to %d)"), ReserveBefore, Primary != nullptr ? Primary->GetReserve() : -1), Primary != nullptr && Primary->GetReserve() > ReserveBefore);
		TestFalse(TEXT("taking the stand's"), StandIn(0, Blaster)->IsThere());
		TestNull(TEXT("and dropping nothing"), DroppedIn(0, Blaster));
		SetPickupSeconds(0.0f);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGunGameHasNoPickups, "CheeseStrike.Play.GunGameHasNoPickups", PickupPlayTests::Flags)

bool FGunGameHasNoPickups::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace PickupPlayTests;
	FSession Session;
	Session.Options = TEXT("?Mode=GunGame");
	Session.SpawnPrimary = FString();
	StartPlaying(Session);
	PutAt(0, OnTheRun(0.0f), RunYaw);
	Then([this]() { TestNotNull(TEXT("a Roquefort's stand"), SpawnStand(CheeseStrikeWeapons::Roquefort().Id, OnTheRun(300.0f))); });
	Wait(0.3f);
	Then([this]()
	{
		const ACheeseStrikePickup* Stand = StandIn(0, CheeseStrikeWeapons::Roquefort().Id);
		TestFalse(TEXT("in Gun Game a stand is out of play"), Stand->IsInPlay() || Stand->IsThere());
		TestTrue(TEXT("and hidden"), Stand->IsHidden());
	});
	PutAt(0, OnTheRun(300.0f), RunYaw);
	Wait(0.6f);
	Play(0, TEXT("tap Interact\nwait 0.4"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Host = CharacterOf(0, 0);
		TestEqual(TEXT("walking over it or pressing Interact gives nothing: the ladder's first weapon in hand"), HeldId(Host), CheeseStrikeArsenal::GunGameWeapon(0));
		TestFalse(TEXT("and nothing else carried"), Host->GetInventory()->CanDraw(EWeaponSlot::Sidearm) || Host->GetInventory()->CanDraw(EWeaponSlot::Knife));
		TestNull(TEXT("no prompt"), ACheeseStrikePickup::OfferedTo(Character(0)));
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FABotGoesForABetterWeapon, "CheeseStrike.Play.ABotGoesForABetterWeapon", PickupPlayTests::Flags)

bool FABotGoesForABetterWeapon::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace PickupPlayTests;
	static TWeakObjectPtr<ACheeseStrikeBotController> Bot;
	Bot = nullptr;
	FSession Session;
	Session.Options = TEXT("?Bots=1");
	Session.SpawnPrimary = FString();
	StartPlaying(Session);
	WaitUntil(TEXT("a bot in the match"), 10.0f, []()
	{
		for (TActorIterator<ACheeseStrikeBotController> It(World(0)); It; ++It)
		{
			Bot = *It;
		}
		return Bot.IsValid() && Bot->GetPawn() != nullptr;
	});
	WaitUntil(TEXT("the navigation mesh"), 15.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		FNavLocation Spot;
		return Navigation != nullptr && Navigation->ProjectPointToNavigation(OnTheRun(0.0f), Spot, FVector(100, 100, 300));
	});
	// The player is kept out of the bot's fight; the bot on the run, a Munster 7 m down it.
	Then([this]()
	{
		CharacterOf(0, 0)->CheeseStrikeProtect(300.0f);
		ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(Bot->GetPawn());
		TestEqual(TEXT("the bot comes in with the Colby Carbine"), PrimaryOf(Body), CheeseStrikeWeapons::Carbine().Id);
		Bot->StopMovement();
		Body->SetActorLocation(OnTheRun(0.0f));
		Bot->FaceTo(FRotator(0.0f, RunYaw + 90.0f, 0.0f));
		TestNotNull(TEXT("a Munster's stand"), SpawnStand(CheeseStrikeWeapons::Munster().Id, OnTheRun(700.0f)));
	});
	WaitUntil(TEXT("the bot taking the Munster"), 20.0f, []() { return PrimaryOf(Cast<ACheeseStrikeCharacter>(Bot->GetPawn())) == CheeseStrikeWeapons::Munster().Id; });
	Then([this]()
	{
		TestFalse(TEXT("its stand is empty"), StandIn(0, CheeseStrikeWeapons::Munster().Id)->IsThere());
		TestNotNull(TEXT("and its Colby Carbine lies on the floor"), DroppedIn(0, CheeseStrikeWeapons::Carbine().Id));
	});
	StopPlaying();
	return true;
}

#endif
