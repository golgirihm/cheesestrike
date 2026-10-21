// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The cheese and what the maps build of it (tools/art/map_kit.py), played in
// the editor (see PlayTesting.h) on The Creamery: a column of cheese eaten at
// its foot comes down a block, a house eaten from the ground comes down with
// nothing left standing on nothing, a door opens for whoever walks up to it;
// and every map, once the mice have eaten all they may, still has its starts
// on firm ground and a way from each to every other.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeDoor.h"
#include "CheeseStrikeCharacter.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeRules.h"

namespace CheeseTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* const Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");
	/** A house of cheese in it, and a door. */
	const FName House(TEXT("CuringHouseGouda"));
	const TCHAR* const DoorLabel = TEXT("Hall_Door_South_West");
	/** Where it is: the middle of its doorway's bottom; it faces north, into the hall. */
	const FVector DoorAt(-1400.0f, -1800.0f, 0.0f);

	TArray<ACheeseStrikeCheese*> AllCheese()
	{
		TArray<ACheeseStrikeCheese*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<ACheeseStrikeCheese> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Found.Add(*It);
				}
			}
		}
		return Found;
	}

	/** The block standing right on `Below`, its middle over Below's, or null. */
	ACheeseStrikeCheese* OnTopOf(const ACheeseStrikeCheese* Below)
	{
		const FBox Box = Below->GetComponentsBoundingBox();
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			const FBox Other = Each->GetComponentsBoundingBox();
			if (Each != Below && FMath::Abs(Other.Min.Z - Box.Max.Z) < 5.0f && FVector::Dist2D(Other.GetCenter(), Box.GetCenter()) < 5.0f && FMath::IsNearlyEqual(Other.GetSize().X, Box.GetSize().X, 2.0))
			{
				return Each;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAWallComesDownFromItsFoot, "CheeseStrike.Play.AWallComesDownFromItsFoot", CheeseTests::Flags)

bool FAWallComesDownFromItsFoot::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace CheeseTests;
	static TArray<TWeakObjectPtr<ACheeseStrikeCheese>> Column;
	static TArray<float> WasAt;
	static float Drop = 0.0f;
	Column.Reset();
	WasAt.Reset();
	FSession Session;
	Session.Level = Level;
	StartPlaying(Session);
	Then([this]()
	{
		// A column of a wall: a block on the ground with at least three more stacked on it.
		for (ACheeseStrikeCheese* Foot : AllCheese())
		{
			if (Foot->GetComponentsBoundingBox().Min.Z > 5.0f)
			{
				continue;
			}
			TArray<ACheeseStrikeCheese*> Up = {Foot};
			while (ACheeseStrikeCheese* Next = OnTopOf(Up.Last()))
			{
				Up.Add(Next);
			}
			if (Up.Num() >= 4)
			{
				for (ACheeseStrikeCheese* Each : Up)
				{
					Column.Add(Each);
					WasAt.Add(Each->GetComponentsBoundingBox().Min.Z);
				}
				break;
			}
		}
		if (!TestTrue(TEXT("a column of cheese four blocks high"), Column.Num() >= 4))
		{
			return;
		}
		AddInfo(FString::Printf(TEXT("%s, %d blocks high"), *Column[0]->GetName(), Column.Num()));
		Drop = Column[0]->GetComponentsBoundingBox().GetSize().Z;
		Column[0]->Bite(CheeseStrikeRules::CheeseBlockHealth);
	});
	WaitUntil(TEXT("the column coming down"), 5.0f, []()
	{
		for (int32 Index = 1; Index < Column.Num(); ++Index)
		{
			if (!Column[Index].IsValid() || Column[Index]->IsFalling() || Column[Index]->GetComponentsBoundingBox().Min.Z > WasAt[Index] - Drop + 5.0f)
			{
				return false;
			}
		}
		return true;
	});
	Then([this]()
	{
		for (int32 Index = 1; Index < Column.Num(); ++Index)
		{
			const float Now = Column[Index].IsValid() ? Column[Index]->GetComponentsBoundingBox().Min.Z : -1.0f;
			TestTrue(FString::Printf(TEXT("block %d came down by the eaten one's height (%.0f to %.0f)"), Index, WasAt[Index], Now), FMath::IsNearlyEqual(Now, WasAt[Index] - Drop, 5.0f));
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHouseComesDownFromItsFoot, "CheeseStrike.Play.AHouseComesDownFromItsFoot", CheeseTests::Flags)

bool FAHouseComesDownFromItsFoot::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace CheeseTests;
	FSession Session;
	Session.Level = Level;
	StartPlaying(Session);
	// Eaten course by course from the ground, as the mice eat it: each time, everything comes down onto what holds it.
	for (int32 Round = 0; Round < 8; ++Round)
	{
		Then([this, Round]()
		{
			int32 Eaten = 0;
			for (ACheeseStrikeCheese* Each : AllCheese())
			{
				if (Each->GetStructure() == House && Each->GetComponentsBoundingBox().Min.Z < 5.0f)
				{
					Each->Bite(CheeseStrikeRules::CheeseBlockHealth);
					++Eaten;
				}
			}
			AddInfo(FString::Printf(TEXT("round %d: %d eaten at the foot"), Round, Eaten));
		});
		WaitUntil(TEXT("the house settling"), 8.0f, []()
		{
			for (ACheeseStrikeCheese* Each : AllCheese())
			{
				if (Each->IsFalling())
				{
					return false;
				}
			}
			return true;
		});
		Then([this, Round]()
		{
			for (ACheeseStrikeCheese* Each : ACheeseStrikeCheese::Unheld(World(0)))
			{
				FString Why;
				Each->IsHeld(&Why);
				if (Each->GetStructure() == House)
				{
					AddError(FString::Printf(TEXT("round %d: %s stands on nothing at %s:%s"), Round, *Each->GetActorLabel(), *Each->GetComponentsBoundingBox().ToString(), *Why));
				}
			}
		});
	}
	// What is left of it stands on its frame's upper floor: the blocks put there, and its roof's planks.
	Then([this]()
	{
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (Each->GetStructure() == House)
			{
				FString Why;
				Each->IsHeld(&Why);
				const FBox Box = Each->GetComponentsBoundingBox();
				AddInfo(FString::Printf(TEXT("left: %s at %s:%s"), *Each->GetActorLabel(), *Box.ToString(), *Why));
				TestTrue(FString::Printf(TEXT("%s is down on the upper floor or lower (%.0f cm up)"), *Each->GetActorLabel(), Box.Min.Z), Box.Min.Z < 400.0f);
			}
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FAfterTheMice, "CheeseStrike.Play.AfterTheMice", CheeseTests::Flags)

void FAfterTheMice::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Every map a host can pick.
	for (int32 Index = 0; Index < CheeseStrikeRules::NumMaps(); ++Index)
	{
		OutBeautifiedNames.Add(FString(CheeseStrikeRules::Maps[Index].Name).Replace(TEXT(" "), TEXT("")));
		OutTestCommands.Add(CheeseStrikeRules::Maps[Index].Level);
	}
}

bool FAfterTheMice::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace CheeseTests;
	static TArray<TPair<TWeakObjectPtr<ACheeseStrikeCheese>, FBox>> Kept;
	static int32 Edible = 0;
	Kept.Reset();
	FSession Session;
	Session.Level = Parameters;
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(World(0));
	});
	// The mice eat all they may, at once.
	Then([this]()
	{
		Edible = 0;
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (!Each->IsEdible())
			{
				Kept.Add({Each, Each->GetComponentsBoundingBox()});
			}
		}
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (Each->IsEdible())
			{
				Each->Bite(CheeseStrikeRules::CheeseBlockHealth);
				++Edible;
			}
		}
		AddInfo(FString::Printf(TEXT("%d blocks eaten, %d kept"), Edible, Kept.Num()));
	});
	WaitUntil(TEXT("what was on the cheese settling"), 10.0f, []()
	{
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (Each->IsFalling())
			{
				return false;
			}
		}
		return true;
	});
	Wait(1.0f);
	WaitUntil(TEXT("the navigation mesh rebuilt"), 60.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		return Navigation != nullptr && !Navigation->IsNavigationBeingBuilt(World(0));
	});
	Then([this]()
	{
		UWorld* Here = World(0);
		// What the map keeps stands where it stood.
		for (const TPair<TWeakObjectPtr<ACheeseStrikeCheese>, FBox>& Each : Kept)
		{
			TestTrue(FString::Printf(TEXT("%s, kept, still stands where it did"), Each.Key.IsValid() ? *Each.Key->GetActorLabel() : TEXT("a kept block")),
				Each.Key.IsValid() && Each.Key->GetComponentsBoundingBox().Min.Equals(Each.Value.Min, 2.0));
		}
		for (ACheeseStrikeCheese* Each : ACheeseStrikeCheese::Unheld(Here))
		{
			AddError(FString::Printf(TEXT("%s stands on nothing at %s"), *Each->GetActorLabel(), *Each->GetComponentsBoundingBox().ToString()));
		}
		// Every start on firm ground, and a way from the first to every other.
		TArray<APlayerStart*> Starts;
		for (TActorIterator<APlayerStart> It(Here); It; ++It)
		{
			Starts.Add(*It);
		}
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Here);
		if (Edible == 0 || Starts.Num() == 0 || !TestNotNull(TEXT("a navigation system"), Navigation))
		{
			AddInfo(TEXT("no cheese to eat: nothing changes"));
			return;
		}
		TArray<FVector> Feet;
		for (APlayerStart* Start : Starts)
		{
			FHitResult Hit;
			const FVector At = Start->GetActorLocation();
			const bool bGround = Here->LineTraceSingleByChannel(Hit, At, At - FVector(0.0f, 0.0f, 400.0f), ECC_Pawn, FCollisionQueryParams(FName(TEXT("AfterTheMice")), false, Start));
			TestTrue(FString::Printf(TEXT("%s stands on the ground (%.0f cm down)"), *Start->GetActorLabel(), bGround ? Hit.Distance : -1.0f), bGround && Hit.Distance < 200.0f);
			FNavLocation OnMesh;
			if (TestTrue(FString::Printf(TEXT("%s is on the navigation mesh"), *Start->GetActorLabel()), Navigation->ProjectPointToNavigation(At, OnMesh, FVector(100.0f, 100.0f, 250.0f))))
			{
				Feet.Add(OnMesh.Location);
			}
		}
		for (int32 Index = 1; Index < Feet.Num(); ++Index)
		{
			UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(Here, Feet[0], Feet[Index]);
			TestTrue(FString::Printf(TEXT("a way from %s to %s (%s)"), *Starts[0]->GetActorLabel(), *Starts[Index]->GetActorLabel(), Path == nullptr || !Path->IsValid() ? TEXT("none") : Path->IsPartial() ? TEXT("partly") : TEXT("whole")),
				Path != nullptr && Path->IsValid() && !Path->IsPartial());
		}
	});
	StopPlaying();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADoorOpensForWhoeverComes, "CheeseStrike.Play.ADoorOpensForWhoeverComes", CheeseTests::Flags)

bool FADoorOpensForWhoeverComes::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace CheeseTests;
	static TWeakObjectPtr<ACheeseStrikeDoor> Door;
	FSession Session;
	Session.Level = Level;
	StartPlaying(Session);
	Then([this]()
	{
		for (TActorIterator<ACheeseStrikeDoor> It(World(0)); It; ++It)
		{
			if (It->GetActorLabel() == DoorLabel)
			{
				Door = *It;
			}
		}
		if (!TestTrue(TEXT("a door"), Door.IsValid()))
		{
			return;
		}
		TestTrue(TEXT("where the test has it"), Door->GetActorLocation().Equals(DoorAt, 1.0) && Door->GetActorForwardVector().Equals(FVector(0.0f, 1.0f, 0.0f), 0.01));
		TestFalse(TEXT("shut with nobody near"), Door->IsOpen() || Door->GetOpenness() > 0.0f);
	});
	// Four metres in front of it (in the Rind Tunnel), facing it, and walking on through into the hall.
	PutAt(0, DoorAt - FVector(0.0f, 400.0f, -100.0f), 90.0f);
	Wait(0.5f);
	Then([this]()
	{
		TestFalse(TEXT("still shut with someone four metres off"), Door.IsValid() && Door->IsOpen());
	});
	// Walking up to it: it opens before they reach it.
	Play(0, TEXT("hold Move 0 1\nwait 0.8"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		if (Door.IsValid() && Me != nullptr)
		{
			TestTrue(FString::Printf(TEXT("open as they come up to it (%.0f cm off, %.0f%% open)"), FVector::Dist2D(Me->GetActorLocation(), DoorAt), Door->GetOpenness() * 100.0f), Door->IsOpen() && Door->GetOpenness() > 0.5f);
		}
	});
	Play(0, TEXT("wait 1.2\nrelease Move"));
	Then([this]()
	{
		const ACheeseStrikeCharacter* Me = Character(0);
		if (Door.IsValid() && Me != nullptr)
		{
			const float Through = FVector::DotProduct(Me->GetActorLocation() - DoorAt, Door->GetActorForwardVector());
			TestTrue(FString::Printf(TEXT("and they walked through it (%.0f cm past)"), Through), Through > 100.0f);
		}
	});
	// Away from it again, into the hall: it shuts.
	PutAt(0, DoorAt + FVector(0.0f, 1000.0f, 100.0f), 90.0f);
	WaitUntil(TEXT("the door shutting behind them"), 3.0f, []() { return Door.IsValid() && !Door->IsOpen() && Door->GetOpenness() == 0.0f; });
	StopPlaying();
	return true;
}

#endif
