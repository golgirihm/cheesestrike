// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The mice: their rules as plain functions (Rules/CheeseStrikeMice.h), and
// mice coming into The Creamery in the editor and eating it (see PlayTesting.h).

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MiceTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHowManyMiceComeAndWhen, "CheeseStrike.Mice.HowManyComeAndWhen", MiceTests::Flags)

bool FHowManyMiceComeAndWhen::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestEqual(TEXT("none for a map without cheese"), MiceFor(0), 0);
	TestEqual(TEXT("two at the least"), MiceFor(30), 2);
	TestEqual(TEXT("one for every sixty blocks"), MiceFor(240), 4);
	TestEqual(TEXT("eight at the most: The Creamery's thousand blocks"), MiceFor(1000), 8);
	TestTrue(TEXT("a match has mice"), HasMice(TEXT("?listen")));
	TestFalse(TEXT("unless told not to"), HasMice(TEXT("?Mice=0")));
	TestEqual(TEXT("a minute in"), ArrivalFrom(TEXT("?listen")), 60.0f);
	TestEqual(TEXT("or when the URL says"), ArrivalFrom(TEXT("?MiceAt=5")), 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceGoForCheeseLowDown, "CheeseStrike.Mice.GoForCheeseLowDown", MiceTests::Flags)

bool FMiceGoForCheeseLowDown::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestEqual(TEXT("cheese on the ground costs its distance"), CheeseCost(1000.0f, 0.0f, false), 1000.0f);
	TestEqual(TEXT("a metre up costs four"), CheeseCost(0.0f, 100.0f, false), 400.0f);
	TestTrue(TEXT("so ten metres off on the ground beats three metres off two metres up"), CheeseCost(1000.0f, 0.0f, false) < CheeseCost(300.0f, 200.0f, false));
	TestTrue(TEXT("and another mouse's cheese costs 13 m more"), CheeseCost(500.0f, 0.0f, true) > CheeseCost(1800.0f, 0.0f, false));
	TestEqual(TEXT("cheese below costs nothing for being below"), CheeseCost(500.0f, -300.0f, false), 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceEatBlocksInStages, "CheeseStrike.Mice.EatBlocksInStages", MiceTests::Flags)

bool FMiceEatBlocksInStages::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	const float Full = CheeseStrikeRules::CheeseBlockHealth;
	TestEqual(TEXT("a block takes ten bites"), FMath::CeilToInt(Full / Bite), 10);
	TestEqual(TEXT("five seconds of one mouse"), FMath::CeilToInt(Full / Bite) * BiteEvery, 5.0f);
	TestEqual(TEXT("whole"), Stage(Full, Full), 0);
	TestEqual(TEXT("bitten"), Stage(Full - Bite, Full), 1);
	TestEqual(TEXT("a third gone"), Stage(Full * 0.6f, Full), 2);
	TestEqual(TEXT("two thirds gone"), Stage(Full * 0.3f, Full), 3);
	TestEqual(TEXT("nearly gone"), Stage(1.0f, Full), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAScaredMouseRunsAndComesBack, "CheeseStrike.Mice.AScaredMouseRunsAndComesBack", MiceTests::Flags)

bool FAScaredMouseRunsAndComesBack::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestEqual(TEXT("it runs at 300 of hurt"), FearAt, 300.0f);
	TestEqual(TEXT("which wears off at 20 a second"), FearAfter(300.0f, 5.0f), 200.0f);
	TestEqual(TEXT("and no further than none"), FearAfter(30.0f, 5.0f), 0.0f);
	TestEqual(TEXT("for six seconds"), FleeFor, 6.0f);
	TestTrue(TEXT("faster than it walks"), FleeSpeed > WalkSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceLeaveTheWayThrough, "CheeseStrike.Mice.LeaveTheWayThrough", MiceTests::Flags)

bool FMiceLeaveTheWayThrough::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	const float Full = CheeseStrikeRules::CheeseBlockHealth;
	TestTrue(TEXT("a mouse goes for cheese"), MayEat(true, Full, false));
	TestTrue(TEXT("and for cheese another has bitten"), MayEat(true, 1.0f, false));
	TestFalse(TEXT("never for the cheese a map keeps: its way through, what its starts stand on"), MayEat(false, Full, false));
	TestFalse(TEXT("nor for cheese that is gone"), MayEat(true, 0.0f, false));
	TestFalse(TEXT("nor for cheese that is falling"), MayEat(true, Full, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWhatHoldsCheeseUp, "CheeseStrike.Mice.WhatHoldsCheeseUp", MiceTests::Flags)

bool FWhatHoldsCheeseUp::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	auto Box = [](double X0, double Y0, double X1, double Y1) { return FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); };
	// A block a metre square.
	const FBox2D Block = Box(0, 0, 100, 100);
	TestFalse(TEXT("on nothing it falls"), IsHeld(Block, {}));
	TestTrue(TEXT("on a block under it it stands"), IsHeld(Block, {Box(0, 0, 100, 100)}));
	TestTrue(TEXT("on a smaller one under its middle"), IsHeld(Block, {Box(30, 30, 70, 70)}));
	TestTrue(TEXT("on the ground, wider than it"), IsHeld(Block, {Box(-1000, -1000, 1000, 1000)}));
	TestFalse(TEXT("half on a block beside its middle it falls"), IsHeld(Block, {Box(0, 0, 50, 100)}));
	TestTrue(TEXT("mostly on one it stands"), IsHeld(Block, {Box(0, 0, 75, 100)}));
	TestFalse(TEXT("on one edge it falls"), IsHeld(Block, {Box(-50, 0, 10, 100)}));
	TestTrue(TEXT("on blocks under both ends it stands"), IsHeld(Block, {Box(0, 0, 20, 100), Box(80, 0, 100, 100)}));
	TestFalse(TEXT("on a corner it falls"), IsHeld(Block, {Box(0, 0, 20, 20)}));
	TestTrue(TEXT("on four corners it stands"), IsHeld(Block, {Box(0, 0, 20, 20), Box(80, 0, 100, 20), Box(0, 80, 20, 100), Box(80, 80, 100, 100)}));
	TestFalse(TEXT("on two corners of one side it falls"), IsHeld(Block, {Box(0, 0, 20, 20), Box(80, 0, 100, 20)}));
	TestFalse(TEXT("what is beside it, touching only its edge, doesn't hold it"), IsHeld(Block, {Box(100, 0, 200, 100), Box(-100, 0, 0, 100)}));
	TestFalse(TEXT("nor do thin posts under it, a door's jambs"), IsHeld(Block, {Box(10, 44, 22, 56), Box(78, 44, 90, 56)}));
	TestTrue(TEXT("but a frame's posts do"), IsHeld(Block, {Box(0, 0, 24, 24), Box(76, 0, 100, 24), Box(0, 76, 24, 100), Box(76, 76, 100, 100)}));
	// A lintel 4 m long over a 2 m doorway, a metre of wall under each end.
	const FBox2D Lintel = Box(0, 0, 400, 50);
	TestTrue(TEXT("a lintel stands on the wall either side of its doorway"), IsHeld(Lintel, {Box(-100, 0, 100, 50), Box(300, 0, 500, 50)}));
	TestFalse(TEXT("and falls once one side is eaten"), IsHeld(Lintel, {Box(-100, 0, 100, 50)}));
	// A plank 9 m long across an 8 m canal.
	const FBox2D Plank = Box(0, 0, 100, 900);
	TestTrue(TEXT("a plank across a canal stands on both banks"), IsHeld(Plank, {Box(-1000, -500, 1000, 50), Box(-1000, 850, 1000, 2000)}));
	TestFalse(TEXT("and falls with one bank under it"), IsHeld(Plank, {Box(-1000, -500, 1000, 50)}));
	// A roof 12 m across a house, on two of its walls.
	const FBox2D Roof = Box(0, 0, 1200, 200);
	TestTrue(TEXT("a roof stands on two opposite walls"), IsHeld(Roof, {Box(0, 0, 50, 200), Box(1150, 0, 1200, 200)}));
	TestFalse(TEXT("but not on one"), IsHeld(Roof, {Box(1150, 0, 1200, 200)}));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeMouse.h"
#include "PlayTesting.h"

namespace MiceTests
{
	template <typename T>
	TArray<T*> All()
	{
		TArray<T*> Found;
		if (UWorld* World = PlayTesting::World(0))
		{
			for (TActorIterator<T> It(World); It; ++It)
			{
				Found.Add(*It);
			}
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMiceEatTheCreamery, "CheeseStrike.Play.TheMiceEatTheCreamery", MiceTests::Flags)

bool FTheMiceEatTheCreamery::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MiceTests;
	static int32 BlocksAtFirst = 0;
	static TWeakObjectPtr<ACheeseStrikeCheese> Upper;
	static float UpperWasAt = 0.0f;
	static TWeakObjectPtr<ACheeseStrikeMouse> Scared;
	static FVector ScaredFrom = FVector::ZeroVector;
	FSession Session;
	Session.Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");
	Session.Options = TEXT("?Mice=1?MiceAt=3");
	StartPlaying(Session);
	Then([this]()
	{
		BlocksAtFirst = All<ACheeseStrikeCheese>().Num();
		TestTrue(TEXT("the countdown is set"), GameState(0)->GetMiceAt() > 0.0f);
	});
	WaitUntil(TEXT("eight mice over the edge"), 15.0f, []() { return All<ACheeseStrikeMouse>().Num() == 8; });
	WaitUntil(TEXT("the mice eating"), 45.0f, []() { return All<ACheeseStrikeCheese>().Num() <= BlocksAtFirst - 12; });
	Then([this]()
	{
		int32 Low = 0;
		int32 Going = 0;
		for (ACheeseStrikeMouse* Mouse : All<ACheeseStrikeMouse>())
		{
			TestTrue(FString::Printf(TEXT("%s has come in"), *Mouse->GetName()), Mouse->GetDoing() != EMouseDoing::Entering);
			TestTrue(FString::Printf(TEXT("%s squeaks, gnaws and crunches"), *Mouse->GetName()), Mouse->HasSounds());
			TestTrue(FString::Printf(TEXT("%s has its body, animations and materials"), *Mouse->GetName()), Mouse->HasBody());
			if (const ACheeseStrikeCheese* Cheese = Mouse->GetTarget())
			{
				++Going;
				Low += Cheese->GetComponentsBoundingBox().Min.Z < 150.0f ? 1 : 0;
			}
		}
		TestTrue(FString::Printf(TEXT("they go for cheese low down (%d of %d)"), Low, Going), Going > 0 && Low * 2 >= Going);
		// A wheel at the bottom of the Wheel Stack is eaten: the one on it falls to the ground.
		ACheeseStrikeCheese* Bottom = nullptr;
		for (ACheeseStrikeCheese* Cheese : All<ACheeseStrikeCheese>())
		{
			const FBox Box = Cheese->GetComponentsBoundingBox();
			if (Cheese->GetStructure() == FName(TEXT("WheelStack")) && Box.Min.Z < 5.0f && Box.GetCenter().Y > 3800.0f)
			{
				for (ACheeseStrikeCheese* Other : All<ACheeseStrikeCheese>())
				{
					const FBox Above = Other->GetComponentsBoundingBox();
					if (Other != Cheese && FMath::Abs(Above.Min.Z - Box.Max.Z) < 5.0f && FVector::Dist2D(Above.GetCenter(), Box.GetCenter()) < 10.0f)
					{
						Bottom = Cheese;
						Upper = Other;
						UpperWasAt = Above.Min.Z;
						break;
					}
				}
			}
			if (Bottom != nullptr)
			{
				break;
			}
		}
		if (TestNotNull(TEXT("a wheel with one on it"), Bottom))
		{
			Bottom->Bite(CheeseStrikeRules::CheeseBlockHealth);
		}
		// A mouse shot for 300 runs from the shooter.
		Scared = All<ACheeseStrikeMouse>()[0];
		ScaredFrom = Scared->GetActorLocation();
		ACheeseStrikeCharacter* Shooter = Character(0);
		FHitResult Hit;
		UGameplayStatics::ApplyPointDamage(Scared.Get(), CheeseStrikeMice::FearAt, (ScaredFrom - Shooter->GetActorLocation()).GetSafeNormal(), Hit, Controller(0), Shooter, nullptr);
		TestTrue(TEXT("the shot mouse runs"), Scared->GetDoing() == EMouseDoing::Fleeing);
	});
	WaitUntil(TEXT("the wheel on the eaten one coming down"), 5.0f, []() { return Upper.IsValid() && !Upper->IsFalling() && Upper->GetComponentsBoundingBox().Min.Z < UpperWasAt - 80.0f; });
	// It runs for FleeFor seconds; how far it has got a moment after depends on its path and, on a busy PC, the frames.
	WaitUntil(TEXT("the shot mouse running off"), CheeseStrikeMice::FleeFor, []() { return Scared.IsValid() && FVector::Dist2D(Scared->GetActorLocation(), ScaredFrom) > 500.0f; });
	Then([this]()
	{
		TestTrue(FString::Printf(TEXT("the shot mouse ran off (%.0f m)"), FVector::Dist2D(Scared->GetActorLocation(), ScaredFrom) / 100.0f), Scared.IsValid() && FVector::Dist2D(Scared->GetActorLocation(), ScaredFrom) > 500.0f);
		int32 Bitten = 0;
		for (const ACheeseStrikeCheese* Cheese : All<ACheeseStrikeCheese>())
		{
			Bitten += Cheese->GetStage() > 0 ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("blocks show being eaten (%d)"), Bitten), Bitten > 0);
		AddInfo(FString::Printf(TEXT("%d of %d blocks left"), All<ACheeseStrikeCheese>().Num(), BlocksAtFirst));
	});
	StopPlaying();
	return true;
}

#endif
