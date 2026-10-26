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
#include "Map/CheeseStrikeMouse.h"
#include "Misc/App.h"
#include "CheeseStrikeCharacter.h"
#include "PickupChecks.h"
#include "PlayTesting.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"

namespace CheeseTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	/**
	 * The mice in AfterTheMice: when they come, by when they have eaten
	 * everything (twice as long as the minute and a half of 23 Oct 2026, which
	 * the user found twice too fast), when it is seen that they eat steadily, and the most and the least of the
	 * blocks left then.
	 */
	constexpr int32 MiceAt = 8;
	constexpr float EatenBy = 180.0f;
	constexpr float SteadyAt = 50.0f;
	constexpr float MostLeftThen = 0.95f;
	constexpr float LeastLeftThen = 0.4f;
	/**
	 * How spread out they eat: the mean distance from each mouse eating to
	 * the nearest other, as a share of what it would be were they scattered
	 * at random over the cheese (half the square root of its area from above
	 * over the mice), at least (0.8 till the mice ate at half the pace, 26 Oct 2026: eating twice as long, the few left near the end
	 * stand closer, and Crumb Kitchen reads 0.68).
	 */
	constexpr float SpreadAtLeast = 0.6f;
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
	// How the mice got on: when they came, the share of the blocks left at SteadyAt, when the last went (or -1), and how
	// far apart they ate (the sum of each eating mouse's distance to the nearest other, and how many were summed).
	static float CameAt = 0.0f;
	static float LeftThen = 1.0f;
	static float GoneAfter = -1.0f;
	static float LastSeen = 0.0f;
	static double ApartSum = 0.0;
	static int32 ApartCount = 0;
	static FBox2D Spread(ForceInit);
	// The most mice seen at once (they leave once the cheese is gone), and how many were left once they had had time to go.
	static int32 MostMice = 0;
	static int32 MiceLeft = -1;
	MostMice = 0;
	MiceLeft = -1;
	Kept.Reset();
	Spread = FBox2D(ForceInit);
	CameAt = 0.0f;
	LeftThen = 1.0f;
	GoneAfter = -1.0f;
	LastSeen = 0.0f;
	ApartSum = 0.0;
	ApartCount = 0;
	FSession Session;
	Session.Level = Parameters;
	Session.Options = FString::Printf(TEXT("?Mice=1?MiceAt=%d"), MiceAt);
	StartPlaying(Session);
	WaitUntil(TEXT("the navigation mesh"), 30.0f, []()
	{
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World(0));
		return Navigation != nullptr && Navigation->GetDefaultNavDataInstance() != nullptr && !Navigation->IsNavigationBeingBuilt(World(0));
	});
	Then([this]()
	{
		Edible = 0;
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (!Each->IsEdible())
			{
				Kept.Add({Each, Each->GetComponentsBoundingBox()});
			}
			else
			{
				++Edible;
				const FBox Box = Each->GetComponentsBoundingBox();
				Spread += FVector2D(Box.Min);
				Spread += FVector2D(Box.Max);
			}
		}
		AddInfo(FString::Printf(TEXT("%d blocks to eat, %d kept"), Edible, Kept.Num()));
		// Its weapons' stands, as the map has them.
		PickupChecks::CheckStands(*this, World(0), false);
	});
	// The mice come, and eat all they may in about a minute and a quarter, steadily and spread out: played at a fixed
	// step, as fast as the editor goes.
	WaitUntil(TEXT("the mice coming"), 60.0f, []()
	{
		if (Edible == 0)
		{
			return true;
		}
		for (TActorIterator<ACheeseStrikeMouse> It(World(0)); It; ++It)
		{
			CameAt = World(0)->GetTimeSeconds();
			FApp::SetUseFixedTimeStep(true);
			FApp::SetFixedDeltaTime(1.0 / 30.0);
			return true;
		}
		return false;
	});
	WaitUntil(TEXT("the mice eating everything"), 900.0f, []()
	{
		if (Edible == 0)
		{
			return true;
		}
		const float Since = World(0)->GetTimeSeconds() - CameAt;
		const int32 Left = ACheeseStrikeCheese::CountEdible(World(0));
		if (Since >= SteadyAt && LastSeen < SteadyAt)
		{
			LeftThen = static_cast<float>(Left) / Edible;
		}
		if (FMath::FloorToInt(Since / 10.0f) != FMath::FloorToInt(LastSeen / 10.0f))
		{
			Test()->AddInfo(FString::Printf(TEXT("%.0f s after the mice came: %d of %d blocks left"), Since, Left, Edible));
		}
		// Every two seconds, how far each mouse eating is from the nearest other.
		if (FMath::FloorToInt(Since / 2.0f) != FMath::FloorToInt(LastSeen / 2.0f))
		{
			TArray<const ACheeseStrikeMouse*> Mice;
			for (TActorIterator<ACheeseStrikeMouse> It(World(0)); It; ++It)
			{
				if (It->GetDoing() != EMouseDoing::Sinking && It->GetDoing() != EMouseDoing::Entering)
				{
					Mice.Add(*It);
				}
			}
			MostMice = FMath::Max(MostMice, Mice.Num());
			for (const ACheeseStrikeMouse* Mouse : Mice)
			{
				float Nearest = TNumericLimits<float>::Max();
				for (const ACheeseStrikeMouse* Other : Mice)
				{
					Nearest = Other == Mouse ? Nearest : FMath::Min(Nearest, static_cast<float>(FVector::Dist2D(Mouse->GetActorLocation(), Other->GetActorLocation())));
				}
				if (Mouse->GetDoing() == EMouseDoing::Eating && Nearest < TNumericLimits<float>::Max())
				{
					ApartSum += Nearest;
					++ApartCount;
				}
			}
		}
		LastSeen = Since;
		if (Left == 0)
		{
			GoneAfter = Since;
			return true;
		}
		return Since > EatenBy + 30.0f;
	});
	// The cheese all eaten, the mice go home over the edge, every one.
	WaitUntil(TEXT("the mice leaving"), 120.0f, []()
	{
		if (Edible == 0 || GoneAfter < 0.0f)
		{
			return true;
		}
		int32 Mice = 0;
		for (TActorIterator<ACheeseStrikeMouse> It(World(0)); It; ++It)
		{
			++Mice;
		}
		MiceLeft = Mice;
		return Mice == 0 || World(0)->GetTimeSeconds() - CameAt > GoneAfter + CheeseStrikeMice::LeaveWithin + 10.0f;
	});
	Then([this]()
	{
		FApp::SetUseFixedTimeStep(false);
		if (Edible == 0)
		{
			return;
		}
		const float Apart = ApartCount > 0 ? static_cast<float>(ApartSum / ApartCount) : 0.0f;
		const int32 Mice = MostMice;
		const float Scattered = 0.5f * FMath::Sqrt(static_cast<float>(Spread.GetArea()) / FMath::Max(1, Mice));
		const float SpreadShare = Scattered > 0.0f ? Apart / Scattered : 0.0f;
		AddInfo(FString::Printf(TEXT("all %d blocks eaten by %d mice %.0f s after they came; %.0f%% left after %.0f s; a mouse eating was %.0f cm from the nearest other, on average, %.2f of %.0f cm at random"), Edible, Mice, GoneAfter, 100.0f * LeftThen, SteadyAt, Apart, SpreadShare, Scattered));
		if (GoneAfter < 0.0f)
		{
			// What is left, and what the mice are doing, to see why.
			TMap<FName, int32> Left;
			for (ACheeseStrikeCheese* Each : AllCheese())
			{
				if (Each->IsEdible())
				{
					Left.FindOrAdd(Each->GetStructure())++;
					AddInfo(FString::Printf(TEXT("left: %s (%s) at %s, %.0f health"), *Each->GetActorLabel(), *Each->GetStructure().ToString(), *Each->GetComponentsBoundingBox().GetCenter().ToCompactString(), Each->GetHealth()));
				}
			}
			for (TActorIterator<ACheeseStrikeMouse> It(World(0)); It; ++It)
			{
				AddInfo(FString::Printf(TEXT("mouse: %s"), *It->Describe()));
			}
		}
		TestTrue(FString::Printf(TEXT("the mice eat everything within %.0f s of coming (%.0f s)"), EatenBy, GoneAfter), GoneAfter >= 0.0f && GoneAfter <= EatenBy);
		TestTrue(FString::Printf(TEXT("the mice eat steadily: more than %.0f%% of the blocks left %.0f s after they came (%.0f%%)"), 100.0f * LeastLeftThen, SteadyAt, 100.0f * LeftThen), LeftThen > LeastLeftThen);
		TestTrue(FString::Printf(TEXT("the mice eat steadily: no more than %.0f%% of the blocks left %.0f s after they came (%.0f%%)"), 100.0f * MostLeftThen, SteadyAt, 100.0f * LeftThen), LeftThen <= MostLeftThen);
		if (GoneAfter >= 0.0f)
		{
			for (TActorIterator<ACheeseStrikeMouse> It(World(0)); It; ++It)
			{
				AddInfo(FString::Printf(TEXT("mouse still here: %s"), *It->Describe()));
			}
			TestEqual(TEXT("the cheese gone, the mice all leave"), MiceLeft, 0);
		}
		TestTrue(FString::Printf(TEXT("the mice spread out: a mouse eating is %.2f as far from the nearest other as at random, at least %.2f"), SpreadShare, SpreadAtLeast), SpreadShare >= SpreadAtLeast);
		// Whatever is left, eaten at once, for the checks of what the map keeps.
		for (ACheeseStrikeCheese* Each : AllCheese())
		{
			if (Each->IsEdible())
			{
				Each->Bite(CheeseStrikeRules::CheeseBlockHealth);
			}
		}
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
		PickupChecks::CheckStands(*this, Here, true);
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
