// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The Fondue Floor (docs/unreal/maps/fondue.md) and falls that kill: the
// rules of a fall as plain functions (Rules/CheeseStrikeFalls.h), and the map
// played in the editor (see PlayTesting.h): its starts and the lines between
// them, a floor that nothing holds up but itself, a fall into the fondue that
// kills and says so, a mouse that falls in and comes back, and bots that keep
// out of the fondue on a floor full of holes.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeFalls.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FondueTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFallsKill, "CheeseStrike.Rules.FallsKill", FondueTests::Flags)

bool FFallsKill::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFalls;
	TestTrue(TEXT("a map that sets a kill height has one"), HasKillHeight(-160.0f));
	TestFalse(TEXT("the engine's own, far under everything, isn't one"), HasKillHeight(-1048575.0f));
	TestFalse(TEXT("nor is no kill height"), HasKillHeight(NoKillHeight));
	TestFalse(TEXT("without a world, nothing is under a kill height"), IsBelowKillHeight(nullptr, -1000000.0f));
	TestTrue(TEXT("a fall just after being hurt counts for whoever hurt them"), CreditsHurter(10.0f, 10.0f + CreditFor - 0.1f));
	TestFalse(TEXT("but not long after"), CreditsHurter(10.0f, 10.0f + CreditFor + 0.1f));
	TestFalse(TEXT("nor when never hurt"), CreditsHurter(-1000.0f, 3.0f));
	TestEqual(TEXT("into a named place"), Words(TEXT("The Fondue")), FString(TEXT("fell into the fondue")));
	TestEqual(TEXT("or nowhere named"), Words(TEXT("")), FString(TEXT("fell")));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "CheeseStrikeBotController.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeDoor.h"
#include "Map/CheeseStrikeMouse.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"

namespace FondueTests
{
	const TCHAR* const Level = TEXT("/Game/CheeseStrike/Maps/Lvl_FondueFloor");
	/** The map's heights (tools/art/build_fondue.py): its kill height, the fondue's surface, the gantry's deck. */
	constexpr float KillHeight = -160.0f;
	constexpr float Fondue = -250.0f;
	constexpr float Gantry = 320.0f;
	/** Tiles of edible floor in the open, west of the middle, with nothing on them. */
	const FVector OpenTile(-2100.0f, 500.0f, 0.0f);
	const FVector OtherOpenTile(-2100.0f, -500.0f, 0.0f);

	template <typename T>
	TArray<T*> All()
	{
		TArray<T*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<T> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Found.Add(*It);
				}
			}
		}
		return Found;
	}

	/** Whether `Cheese` is a tile of the floor: a pillar from the vat up to the floor. */
	bool IsTile(const ACheeseStrikeCheese* Cheese)
	{
		const FBox Box = Cheese->GetComponentsBoundingBox();
		return FMath::Abs(Box.Max.Z) < 5.0f && Box.Min.Z < KillHeight;
	}

	/** The tile under `At` (x and y), or null. */
	ACheeseStrikeCheese* TileUnder(const FVector& At)
	{
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			if (IsTile(Each) && Each->GetComponentsBoundingBox().IsInsideXY(At))
			{
				return Each;
			}
		}
		return nullptr;
	}

	/** The middle of the tile nearest `At` (x and y), at the floor. */
	FVector NearestTile(const FVector& At)
	{
		FVector Best = At;
		double BestDistance = TNumericLimits<double>::Max();
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			const FVector Middle = Each->GetComponentsBoundingBox().GetCenter();
			if (IsTile(Each) && FVector::Dist2D(Middle, At) < BestDistance)
			{
				BestDistance = FVector::Dist2D(Middle, At);
				Best = FVector(Middle.X, Middle.Y, 0.0f);
			}
		}
		return Best;
	}

	/** Eats the tile nearest `At` and the eight round it; or, not `bUnder`, only those round it, `Rings` deep. */
	void EatAround(const FVector& At, bool bUnder = true, int32 Rings = 1)
	{
		const FVector Centre = NearestTile(At);
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			const double Off = FVector::Dist2D(Each->GetComponentsBoundingBox().GetCenter(), Centre);
			if (IsTile(Each) && Each->IsEdible() && Off < 100.0 + 200.0 * Rings && (bUnder || Off > 50.0))
			{
				Each->Bite(CheeseStrikeRules::CheeseBlockHealth);
			}
		}
	}

	/** Every block of cheese and every door in the host's world, for traces that look past them: what stays once the mice have been, with the doors open. */
	FCollisionQueryParams PastTheCheese(const FName& Name)
	{
		FCollisionQueryParams Params(Name, false);
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			Params.AddIgnoredActor(Each);
		}
		for (ACheeseStrikeDoor* Each : All<ACheeseStrikeDoor>())
		{
			Params.AddIgnoredActor(Each);
		}
		return Params;
	}

	bool NavigationReady()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(PlayTesting::World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(PlayTesting::World(0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFondueFloorHoldsTogether, "CheeseStrike.Play.TheFondueFloorHoldsTogether", FondueTests::Flags)

bool FTheFondueFloorHoldsTogether::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FondueTests;
	FSession Session;
	Session.Level = Level;
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []() { return NavigationReady(); });
	Then([this]()
	{
		UWorld* Here = World(0);
		TestTrue(FString::Printf(TEXT("a fall into the fondue kills (kill height %.0f)"), CheeseStrikeFalls::KillHeight(Here)), FMath::IsNearlyEqual(CheeseStrikeFalls::KillHeight(Here), KillHeight, 1.0f));
		// Starts for sixteen, each side's tagged, on a deck at its end; free-for-all's on the paths and islands.
		TArray<APlayerStart*> Gouda, Mozza, Free;
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			(It->PlayerStartTag == FName(TEXT("Gouda")) ? Gouda : It->PlayerStartTag == FName(TEXT("Mozza")) ? Mozza : Free).Add(*It);
		}
		TestEqual(TEXT("eight Gouda starts"), Gouda.Num(), 8);
		TestEqual(TEXT("eight Mozza starts"), Mozza.Num(), 8);
		TestTrue(FString::Printf(TEXT("free-for-all adds starts through the map (%d)"), Free.Num()), Free.Num() >= 12);
		for (APlayerStart* Start : Gouda)
		{
			TestTrue(FString::Printf(TEXT("%s is at the west end"), *Start->GetActorLabel()), Start->GetActorLocation().X < -3000.0f);
		}
		for (APlayerStart* Start : Mozza)
		{
			TestTrue(FString::Printf(TEXT("%s is at the east end"), *Start->GetActorLabel()), Start->GetActorLocation().X > 3000.0f);
		}
		// Every start stands on what is never eaten.
		TArray<APlayerStart*> Every = Gouda;
		Every.Append(Mozza);
		Every.Append(Free);
		for (APlayerStart* Start : Every)
		{
			FHitResult Hit;
			const FVector At = Start->GetActorLocation();
			Here->LineTraceSingleByChannel(Hit, At, At - FVector(0.0f, 0.0f, 400.0f), ECC_Pawn, FCollisionQueryParams(FName(TEXT("FondueStart")), false, Start));
			const ACheeseStrikeCheese* Cheese = Cast<ACheeseStrikeCheese>(Hit.GetActor());
			TestTrue(FString::Printf(TEXT("%s stands on what the mice never eat (%s)"), *Start->GetActorLabel(), Hit.GetActor() != nullptr ? *Hit.GetActor()->GetActorLabel() : TEXT("nothing")),
				Hit.bBlockingHit && (Cheese == nullptr || !Cheese->IsEdible()));
		}
		// No line from one side's spawn to the other's, even once the mice have eaten everything and with the doors open: hard cover is in the way.
		const FCollisionQueryParams Past = PastTheCheese(FName(TEXT("FondueSpawnLine")));
		int32 Open = 0;
		for (APlayerStart* From : Gouda)
		{
			for (APlayerStart* To : Mozza)
			{
				for (const float Eye : {60.0f, 0.0f})
				{
					FHitResult Hit;
					if (!Here->LineTraceSingleByChannel(Hit, From->GetActorLocation() + FVector(0.0f, 0.0f, Eye), To->GetActorLocation() + FVector(0.0f, 0.0f, Eye), ECC_Visibility, Past))
					{
						++Open;
						AddError(FString::Printf(TEXT("a line from %s to %s at %.0f cm"), *From->GetActorLabel(), *To->GetActorLabel(), Eye));
					}
				}
			}
		}
		TestEqual(TEXT("no line from spawn to spawn"), Open, 0);
		// Nothing holds the floor up but itself: under every edible tile, past the cheese, is only the vat's floor, under the fondue.
		int32 Tiles = 0;
		int32 Held = 0;
		for (ACheeseStrikeCheese* Tile : All<ACheeseStrikeCheese>())
		{
			if (!IsTile(Tile) || !Tile->IsEdible())
			{
				continue;
			}
			++Tiles;
			const FBox Box = Tile->GetComponentsBoundingBox();
			for (const FVector2D& Off : {FVector2D(0.0, 0.0), FVector2D(70.0, 70.0), FVector2D(-70.0, 70.0), FVector2D(70.0, -70.0), FVector2D(-70.0, -70.0)})
			{
				const FVector Top(Box.GetCenter().X + Off.X, Box.GetCenter().Y + Off.Y, 50.0f);
				FHitResult Hit;
				if (Here->LineTraceSingleByChannel(Hit, Top, FVector(Top.X, Top.Y, -1000.0f), ECC_Pawn, Past) && Hit.ImpactPoint.Z > KillHeight)
				{
					++Held;
					AddError(FString::Printf(TEXT("%s has %s under it at %.0f cm"), *Tile->GetActorLabel(), *Hit.GetActor()->GetActorLabel(), Hit.ImpactPoint.Z));
					break;
				}
			}
		}
		TestTrue(FString::Printf(TEXT("a floor of tiles (%d edible)"), Tiles), Tiles > 400);
		TestEqual(TEXT("nothing under the floor to stand on once it is eaten"), Held, 0);
		// The fondue: there, and nothing to stand on.
		bool bFondue = false;
		for (AStaticMeshActor* Each : All<AStaticMeshActor>())
		{
			if (Each->GetActorLabel() == TEXT("Fondue"))
			{
				bFondue = true;
				TestFalse(TEXT("the fondue blocks nothing"), Each->GetStaticMeshComponent()->IsCollisionEnabled());
				TestTrue(TEXT("the fondue is under the kill height"), Each->GetComponentsBoundingBox(true).Max.Z < KillHeight);
			}
		}
		TestTrue(TEXT("a vat of fondue"), bFondue);
		TestEqual(TEXT("a door at the top of each of the decks' ramps"), All<ACheeseStrikeDoor>().Num(), 6);
	});
	// The mice eat everything they may: the waxed paths still join the decks to the islands, the Stirrer and the gantry.
	Then([this]()
	{
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			if (Each->IsEdible())
			{
				Each->Bite(CheeseStrikeRules::CheeseBlockHealth);
			}
		}
	});
	Wait(2.0f);
	WaitUntil(TEXT("the navigation mesh rebuilt"), 60.0f, []() { return NavigationReady(); });
	Then([this]()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		const FVector Deck(-3350.0f, 0.0f, 220.0f);
		struct FPlace { const TCHAR* Name; FVector At; };
		for (const FPlace& Place : {FPlace{TEXT("the other deck"), FVector(3350.0f, 0.0f, 220.0f)}, FPlace{TEXT("the Gouda pump"), FVector(-1500.0f, 900.0f, 100.0f)},
			FPlace{TEXT("the Mozza boiler"), FVector(1700.0f, -1200.0f, 100.0f)}, FPlace{TEXT("the Stirrer"), FVector(-300.0f, -300.0f, 100.0f)},
			FPlace{TEXT("the gantry"), FVector(-200.0f, 0.0f, Gantry + 100.0f)}, FPlace{TEXT("the Skim"), FVector(700.0f, 1700.0f, 100.0f)},
			FPlace{TEXT("the Drain"), FVector(-300.0f, -1700.0f, 100.0f)}, FPlace{TEXT("the Mozza Rind Run"), FVector(1100.0f, -1300.0f, 100.0f)}})
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(World(0), Deck, Place.At);
			TestTrue(FString::Printf(TEXT("%s can still be walked to from the Gouda deck (%s)"), Place.Name, Path == nullptr ? TEXT("no path") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
				Path != nullptr && Path->IsValid() && !Path->IsPartial());
		}
		int32 Left = 0;
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			Left += Each->IsEdible() ? 1 : 0;
		}
		TestEqual(TEXT("everything edible is gone, what stood on it sunk"), Left, 0);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFallIntoTheFondueKills, "CheeseStrike.Play.AFallIntoTheFondueKills", FondueTests::Flags)

bool FAFallIntoTheFondueKills::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FondueTests;
	static TWeakObjectPtr<ACheeseStrikeCharacter> Fallen;
	static FString Host;
	static FString Joiner;
	FSession Session;
	Session.Level = Level;
	Session.Players = 2;
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []() { return NavigationReady(); });
	// The joiner shoots the host, who then falls in: the kill is the joiner's.
	PutAt(0, OpenTile + FVector(0.0f, 0.0f, 100.0f), 0.0f);
	PutAt(1, FVector(-3350.0f, 0.0f, 220.0f), 0.0f);
	Then([this]()
	{
		ACheeseStrikeCharacter* Me = CharacterOf(0, 0);
		ACheeseStrikeCharacter* Them = CharacterOf(1, 0);
		Host = Me->DisplayName();
		Joiner = Them->DisplayName();
		Me->EndProtection();
		TestTrue(TEXT("the joiner hurts the host"), Me->TakeShot(10.0f, Them, false));
		Fallen = Me;
		EatAround(Me->GetActorLocation());
	});
	WaitUntil(TEXT("the host falling into the fondue and dying"), 6.0f, []() { return Fallen.IsValid() && !Fallen->IsAlive(); });
	Then([this]()
	{
		TestTrue(FString::Printf(TEXT("killed in the fondue (at %.0f cm)"), Fallen->GetActorLocation().Z), Fallen->GetActorLocation().Z < KillHeight + 10.0f);
		TestEqual(TEXT("by whoever hurt them just before"), Fallen->GetKilledBy(), Joiner);
		const TArray<FKillFeedEntry>& Feed = GameState(0)->GetKillFeed();
		if (TestTrue(TEXT("in the kill feed"), Feed.Num() > 0))
		{
			TestEqual(TEXT("the feed has the joiner's kill"), Feed.Last().Killer, Joiner);
			TestEqual(TEXT("of the host"), Feed.Last().Victim, Host);
		}
		const ACheeseStrikePlayerState* Theirs = CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestEqual(TEXT("the joiner scores it"), Theirs != nullptr ? Theirs->GetKills() : -1, 1);
	});
	// The body stays to be seen going in, under the fondue; the host comes back.
	Wait(1.5f);
	Then([this]()
	{
		if (TestTrue(TEXT("the body is kept, not destroyed"), Fallen.IsValid()))
		{
			TestTrue(FString::Printf(TEXT("and has gone under the fondue (%.0f cm)"), Fallen->GetMesh()->GetComponentLocation().Z), Fallen->GetMesh()->GetBoneLocation(TEXT("pelvis")).Z < Fondue);
		}
	});
	WaitUntil(TEXT("the host back"), 10.0f, []() { return CharacterOf(0, 0) != nullptr && CharacterOf(0, 0) != Fallen.Get() && CharacterOf(0, 0)->IsAlive(); });
	// Then falls in with nobody to blame: the fondue has it, and the feed and the death screen say so.
	PutAt(0, OtherOpenTile + FVector(0.0f, 0.0f, 100.0f), 0.0f);
	Then([this]()
	{
		Fallen = CharacterOf(0, 0);
		EatAround(Fallen->GetActorLocation());
	});
	WaitUntil(TEXT("the host falling in again"), 6.0f, []() { return Fallen.IsValid() && !Fallen->IsAlive(); });
	Wait(0.5f);
	Then([this]()
	{
		TestEqual(TEXT("nobody killed them"), Fallen->GetKilledBy(), FString());
		TestEqual(TEXT("they fell into the fondue"), Fallen->GetDiedHow(), FString(TEXT("fell into the fondue")));
		const TArray<FKillFeedEntry>& Feed = GameState(0)->GetKillFeed();
		if (TestTrue(TEXT("in the kill feed"), Feed.Num() > 0))
		{
			TestEqual(TEXT("with no killer"), Feed.Last().Killer, FString());
			TestEqual(TEXT("the host"), Feed.Last().Victim, Host);
			TestEqual(TEXT("fell into the fondue"), Feed.Last().How, FString(TEXT("fell into the fondue")));
		}
		// The joiner's machine has the same words.
		const ACheeseStrikeCharacter* Seen = CharacterOf(0, 1);
		TestTrue(TEXT("the joiner sees the host fell in"), Seen != nullptr && Seen->GetDiedHow() == TEXT("fell into the fondue"));
		const ACheeseStrikePlayerState* Mine = CharacterOf(0, 0)->GetPlayerState<ACheeseStrikePlayerState>();
		TestEqual(TEXT("two deaths for the host"), Mine != nullptr ? Mine->GetDeaths() : -1, 2);
		TestEqual(TEXT("and the joiner has no more kills"), CharacterOf(1, 0)->GetPlayerState<ACheeseStrikePlayerState>()->GetKills(), 1);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceOnAFloorTheyEat, "CheeseStrike.Play.MiceOnAFloorTheyEat", FondueTests::Flags)

bool FMiceOnAFloorTheyEat::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FondueTests;
	static TWeakObjectPtr<ACheeseStrikeMouse> Mouse;
	FSession Session;
	Session.Level = Level;
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []() { return NavigationReady(); });
	// A mouse comes in over the north wall onto the open floor.
	Then([this]()
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Top(-2900.0f, 2050.0f, 650.0f);
		Mouse = World(0)->SpawnActor<ACheeseStrikeMouse>(ACheeseStrikeMouse::StaticClass(), FTransform(Top), Spawn);
		if (TestTrue(TEXT("a mouse"), Mouse.IsValid()))
		{
			Mouse->Enter(Top, FVector(-2900.0f, 1700.0f, 0.0f));
		}
	});
	WaitUntil(TEXT("the mouse in"), 5.0f, []() { return Mouse.IsValid() && Mouse->GetDoing() != EMouseDoing::Entering; });
	// The floor round it goes, leaving it on an island: it leaps off it, to floor that leads out.
	Then([this]()
	{
		// Two rings of tiles round it: nothing left within its reach.
		const FVector Island = NearestTile(Mouse->GetActorLocation());
		Mouse->SetActorLocation(FVector(Island.X, Island.Y, Mouse->GetActorLocation().Z));
		EatAround(Island, false, 2);
	});
	WaitUntil(TEXT("the mouse leaping off its island"), 15.0f, []() { return Mouse.IsValid() && Mouse->GetLeaps() > 0; });
	WaitUntil(TEXT("the mouse across"), 5.0f, []() { return Mouse.IsValid() && Mouse->GetDoing() != EMouseDoing::Entering; });
	Wait(1.0f);
	Then([this]()
	{
		TestEqual(TEXT("it didn't fall in"), Mouse->GetFallenIn(), 0);
		TestTrue(FString::Printf(TEXT("it is on the floor (%.0f cm)"), Mouse->GetActorLocation().Z), Mouse->GetActorLocation().Z > -50.0f);
	});
	// The floor under it goes.
	Then([this]() { EatAround(Mouse->GetActorLocation()); });
	WaitUntil(TEXT("the mouse falling in"), 4.0f, []() { return Mouse.IsValid() && Mouse->GetDoing() == EMouseDoing::Sinking; });
	Then([this]() { TestEqual(TEXT("it has fallen in once"), Mouse->GetFallenIn(), 1); });
	WaitUntil(TEXT("the mouse gone under the fondue"), 4.0f, []() { return Mouse.IsValid() && Mouse->IsHidden(); });
	Then([this]() { TestTrue(FString::Printf(TEXT("under the fondue (%.0f cm)"), Mouse->GetActorLocation().Z), Mouse->GetActorLocation().Z < Fondue); });
	// A while later it comes back over the edge, onto the floor, and goes on eating.
	WaitUntil(TEXT("the mouse coming back"), CheeseStrikeFalls::MouseGoneFor + 4.0f, []() { return Mouse.IsValid() && Mouse->GetDoing() != EMouseDoing::Sinking; });
	Then([this]() { TestFalse(TEXT("seen again"), Mouse->IsHidden()); });
	WaitUntil(TEXT("the mouse back on the floor"), 5.0f, []() { return Mouse.IsValid() && Mouse->GetDoing() != EMouseDoing::Entering; });
	Wait(3.0f);
	Then([this]()
	{
		TestTrue(FString::Printf(TEXT("on the floor (%.0f cm), not in the fondue"), Mouse->GetActorLocation().Z), Mouse->GetActorLocation().Z > -50.0f);
		TestEqual(TEXT("and it hasn't fallen in again"), Mouse->GetFallenIn(), 1);
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsKeepOutOfTheFondue, "CheeseStrike.Play.BotsKeepOutOfTheFondue", FondueTests::Flags)

bool FBotsKeepOutOfTheFondue::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace FondueTests;
	static float StartedAt = 0.0f;
	static TMap<FString, FVector> Spawned;
	static TMap<FString, float> Furthest;
	/** Every death the feed has shown, as it shows them (it keeps only the last few): the falls, and the bots blown up by their own grenades (no kill, but no fall either). */
	static TSet<FString> Falls;
	static TSet<FString> OwnBlasts;
	Spawned.Reset();
	Furthest.Reset();
	Falls.Reset();
	OwnBlasts.Reset();
	FSession Session;
	Session.Level = Level;
	Session.Options = TEXT("?Bots=9?KillLimit=100");
	StartPlaying(Session);
	WaitUntil(TEXT("nine bots and the navigation mesh"), 30.0f, []() { return All<ACheeseStrikeBotController>().Num() == 9 && NavigationReady(); });
	// Half the floor gone (none from under anyone): holes everywhere.
	Then([this]()
	{
		TArray<ACheeseStrikeCheese*> Tiles;
		for (ACheeseStrikeCheese* Each : All<ACheeseStrikeCheese>())
		{
			bool bUnderSomeone = false;
			for (ACheeseStrikeCharacter* Someone : All<ACheeseStrikeCharacter>())
			{
				bUnderSomeone |= Each->GetComponentsBoundingBox().ExpandBy(FVector(80.0f, 80.0f, 0.0f)).IsInsideXY(Someone->GetActorLocation());
			}
			if (IsTile(Each) && Each->IsEdible() && !bUnderSomeone)
			{
				Tiles.Add(Each);
			}
		}
		for (int32 Index = 0; Index < Tiles.Num(); Index += 2)
		{
			Tiles[Index]->Bite(CheeseStrikeRules::CheeseBlockHealth);
		}
		StartedAt = Now(0);
	});
	// Forty seconds of bots roaming and fighting over the holes.
	WaitUntil(TEXT("forty seconds of the bots playing"), 60.0f, []()
	{
		for (ACheeseStrikeBotController* Bot : All<ACheeseStrikeBotController>())
		{
			const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>();
			const ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(Bot->GetPawn());
			if (State != nullptr && Body != nullptr && Body->IsAlive())
			{
				const FString Name = State->GetPlayerName();
				if (!Spawned.Contains(Name))
				{
					Spawned.Add(Name, Body->GetActorLocation());
				}
				Furthest.FindOrAdd(Name) = FMath::Max(Furthest.FindRef(Name), FVector::Dist2D(Spawned[Name], Body->GetActorLocation()));
			}
		}
		for (const FKillFeedEntry& Kill : GameState(0)->GetKillFeed())
		{
			const FString Key = FString::Printf(TEXT("%s@%.2f"), *Kill.Victim, Kill.At);
			if (Kill.How.StartsWith(TEXT("fell")))
			{
				Falls.Add(Key);
			}
			else if (Kill.Killer.IsEmpty() && Kill.How == CheeseStrikeGrenades::OwnWords)
			{
				OwnBlasts.Add(Key);
			}
		}
		return Now(0) - StartedAt >= 40.0f;
	});
	Then([this]()
	{
		int32 Kills = 0;
		for (ACheeseStrikeBotController* Bot : All<ACheeseStrikeBotController>())
		{
			if (const ACheeseStrikePlayerState* State = Bot->GetPlayerState<ACheeseStrikePlayerState>())
			{
				Kills += State->GetKills();
				AddInfo(FString::Printf(TEXT("%s: %d kills, %d deaths, went %.0f m"), *State->GetPlayerName(), State->GetKills(), State->GetDeaths(), Furthest.FindRef(State->GetPlayerName()) / 100.0f));
			}
		}
		int32 Moved = 0;
		for (const TPair<FString, float>& Each : Furthest)
		{
			Moved += Each.Value > 1000.0f ? 1 : 0;
		}
		// The feed keeps the last few; every bot's deaths are counted too, against the kills.
		int32 Deaths = 0;
		for (ACheeseStrikeBotController* Bot : All<ACheeseStrikeBotController>())
		{
			Deaths += Bot->GetPlayerState<ACheeseStrikePlayerState>() != nullptr ? Bot->GetPlayerState<ACheeseStrikePlayerState>()->GetDeaths() : 0;
		}
		const ACheeseStrikePlayerState* Mine = Character(0) != nullptr ? Character(0)->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
		const int32 AllKills = Kills + (Mine != nullptr ? Mine->GetKills() : 0);
		const int32 AllDeaths = Deaths + (Mine != nullptr ? Mine->GetDeaths() : 0);
		AddInfo(FString::Printf(TEXT("%d kills, %d deaths, %d by their own grenade"), AllKills, AllDeaths, OwnBlasts.Num()));
		TestEqual(TEXT("no fall in the kill feed"), Falls.Num(), 0);
		TestTrue(FString::Printf(TEXT("every death a kill or one's own grenade: nobody walked into the fondue (%d deaths, %d kills, %d own grenades)"), AllDeaths, AllKills, OwnBlasts.Num()),
			AllDeaths <= AllKills + OwnBlasts.Num());
		TestTrue(FString::Printf(TEXT("the bots got about over the holes (%d went 10 m or more)"), Moved), Moved >= 6);
		TestTrue(FString::Printf(TEXT("and fought (%d kills)"), AllKills), AllKills >= 3);
	});
	StopPlaying();
	return true;
}

#endif
