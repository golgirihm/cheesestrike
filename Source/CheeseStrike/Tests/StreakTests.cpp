// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The streaks' rules as plain functions (Rules/CheeseStrikeStreaks.h): a
// second Unmanned Aerial Cheese adding its time to the first's, the drone
// under a roof, and a strike's wheels coming through one.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "UI/CheeseStrikeHUDRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace StreakTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASecondAerialCheeseAddsItsTime, "CheeseStrike.Rules.ASecondAerialCheeseAddsItsTime", StreakTests::Flags)

bool FASecondAerialCheeseAddsItsTime::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStreaks;
	const FReveal First = AddReveal({}, 100.0f);
	TestEqual(TEXT("the first from now"), First.From, 100.0f);
	TestEqual(TEXT("for its time"), First.Until, 100.0f + RevealTime);
	const FReveal Second = AddReveal(First, 110.0f);
	TestEqual(TEXT("a second while it is up: its sweeps go on from the first"), Second.From, 100.0f);
	TestEqual(TEXT("and its whole time is added, none of it wasted"), Second.Until, 100.0f + 2.0f * RevealTime);
	TestEqual(TEXT("a third, more again"), AddReveal(Second, 140.0f).Until, 100.0f + 3.0f * RevealTime);
	const FReveal Later = AddReveal(First, 200.0f);
	TestTrue(TEXT("one after the first has run out starts afresh"), Later.From == 200.0f && Later.Until == 200.0f + RevealTime);
	// The sweeps go on through the added time, and stop at its end.
	const float Lasts = Second.Until - Second.From;
	TestTrue(TEXT("sweeping after the first's time would have run out"), CheeseStrikeHUD::SweepAt(130.0f, Second.Until, Lasts, SweepPeriod).Index != INDEX_NONE);
	TestEqual(TEXT("on the first's beat"), CheeseStrikeHUD::SweepAt(130.0f, Second.Until, Lasts, SweepPeriod).Index, FMath::FloorToInt(30.0f / SweepPeriod));
	TestEqual(TEXT("and not after it all"), CheeseStrikeHUD::SweepAt(Second.Until + 0.1f, Second.Until, Lasts, SweepPeriod).Index, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDroneFliesUnderARoof, "CheeseStrike.Rules.TheDroneFliesUnderARoof", StreakTests::Flags)

bool FTheDroneFliesUnderARoof::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStreaks;
	float Scale = 0.0f;
	TestEqual(TEXT("in the open, high over the starts"), DroneHeight(0.0f, 0.0f, 400.0f, Scale), 400.0f + DroneAltitude);
	TestEqual(TEXT("at its full size"), Scale, 1.0f);
	TestEqual(TEXT("under a high roof (the Grotto's 19 m), just under it"), DroneHeight(1900.0f, 0.0f, 400.0f, Scale), 1900.0f - DroneUnderRoof);
	TestEqual(TEXT("at its full size"), Scale, 1.0f);
	DroneHeight(1250.0f, 0.0f, 410.0f, Scale);
	TestEqual(TEXT("under the Kitchen's 12.5 m, though some start on the worktop, at its full size"), Scale, 1.0f);
	const float Low = DroneHeight(560.0f, 0.0f, 0.0f, Scale);
	TestEqual(TEXT("under a low one (the Labyrinth's 5.6 m), smaller"), Scale, DroneIndoorScale);
	TestTrue(FString::Printf(TEXT("and under it (%.0f)"), Low), Low < 560.0f && Low > 560.0f - DroneUnderRoof);
	TestEqual(TEXT("never down among the players"), DroneHeight(150.0f, 0.0f, 0.0f, Scale), DroneAboveStarts * DroneIndoorScale);
	const FDroneWay Ways[] = {{3000.0f, 560.0f, 9}, {2000.0f, 560.0f, 2}, {900.0f, 700.0f, 2}, {400.0f, 700.0f, 5}};
	TestEqual(TEXT("it takes the circle that runs into least, the first of those"), ChooseDroneWay(Ways), 1);
	TestEqual(TEXT("and none of nothing"), ChooseDroneWay({}), INDEX_NONE);
	TestEqual(TEXT("the roof over a circle: the middle of what is over its points, not a deck under part of it"), DroneRoof({1900.0f, 1000.0f, 1900.0f, 1900.0f, 1000.0f}), 1900.0f);
	TestEqual(TEXT("none over it, none"), DroneRoof({}), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAStrikeComesThroughTheRoof, "CheeseStrike.Rules.AStrikeComesThroughTheRoof", StreakTests::Flags)

bool FAStrikeComesThroughTheRoof::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStreaks;
	TestTrue(TEXT("the sky's fall is the usual"), FallTime() == FallTime(DropHeight, DropSpeed) && HeightBefore(0.3f) == HeightBefore(0.3f, DropHeight, DropSpeed));
	// Under the Labyrinth's ceiling, 5.6 m over the floor.
	const float Height = 560.0f - RoofGap;
	const float Fall = FallTime(Height, IndoorDropSpeed);
	TestTrue(FString::Printf(TEXT("under a low roof, a wheel is seen falling: not gone in a flash (%.2f s)"), Fall), Fall > 0.4f);
	TestTrue(TEXT("but still a strike: faster than the whistle"), Fall < StrikeWarning);
	TestTrue(TEXT("from under the roof"), FMath::IsNearlyEqual(HeightBefore(Fall, Height, IndoorDropSpeed), Height, 0.5f));
	TestTrue(TEXT("to the floor"), FMath::IsNearlyEqual(HeightBefore(0.0f, Height, IndoorDropSpeed), 0.0f, 0.01f));
	TestTrue(TEXT("slower at first than from the sky"), IndoorDropSpeed < DropSpeed);
	return true;
}

#endif
