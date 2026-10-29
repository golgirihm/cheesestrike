// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The mouse that now and then crosses the front end behind the figure: when it
// comes, the phases of a visit and their lengths, and its path, clear of the
// figure's pedestal.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeMenuMouseRules.h"
#include "UI/CheeseStrikeMenuFigure.h"
#include "UI/CheeseStrikeMenuMouse.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuMouseComesNowAndThen, "CheeseStrike.Rules.TheMenuMouseComesNowAndThen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTheMenuMouseComesNowAndThen::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMenuMouse;
	// When: never in the front end's first seconds, then every 20 to 40 s.
	for (const float Random : {0.0f, 0.3f, 0.7f, 1.0f, -1.0f, 2.0f})
	{
		const float First = NextVisitIn(true, Random);
		const float Later = NextVisitIn(false, Random);
		TestTrue(FString::Printf(TEXT("the first visit comes after the first %.0f s (%.1f)"), NotBefore, First), First >= NotBefore && First <= FirstTo);
		TestTrue(FString::Printf(TEXT("later ones every 20 to 40 s (%.1f)"), Later), Later >= 20.0f && Later <= 40.0f);
	}
	TestTrue(TEXT("the range is used"), NextVisitIn(false, 0.0f) < NextVisitIn(false, 1.0f));

	// A visit: walk in, sniff, turn the head, look, walk off, in that order and for their lengths.
	constexpr float WalkIn = 6.0f;
	constexpr float WalkOff = 3.6f;
	const float Length = VisitSeconds(WalkIn, WalkOff);
	TestEqual(TEXT("a visit is its phases end to end"), Length, WalkIn + SniffSeconds + TurnSeconds + LookSeconds + WalkOff);
	TestTrue(TEXT("before it starts, away"), At(-0.1f, WalkIn, WalkOff).Phase == EPhase::Away);
	TestTrue(TEXT("and after it ends"), At(Length + 0.01f, WalkIn, WalkOff).Phase == EPhase::Away);
	const FMoment Halfway = At(WalkIn / 2.0f, WalkIn, WalkOff);
	TestTrue(TEXT("first it walks in"), Halfway.Phase == EPhase::WalkIn && FMath::IsNearlyEqual(Halfway.Along, 0.5f, 0.001f) && Halfway.HeadShare == 0.0f);
	const FMoment Sniffing = At(WalkIn + 1.0f, WalkIn, WalkOff);
	TestTrue(TEXT("then stops and sniffs, its head ahead"), Sniffing.Phase == EPhase::Sniff && Sniffing.Along == 1.0f && Sniffing.HeadShare == 0.0f);
	const FMoment Turning = At(WalkIn + SniffSeconds + TurnSeconds / 2.0f, WalkIn, WalkOff);
	TestTrue(TEXT("then turns its head toward the cheese"), Turning.Phase == EPhase::Turn && Turning.HeadShare > 0.2f && Turning.HeadShare < 0.8f);
	const FMoment Looking = At(WalkIn + SniffSeconds + TurnSeconds + 0.5f, WalkIn, WalkOff);
	TestTrue(TEXT("looks at it a moment"), Looking.Phase == EPhase::Look && Looking.HeadShare == 1.0f);
	const FMoment Leaving = At(WalkIn + SniffSeconds + TurnSeconds + LookSeconds + WalkOff / 2.0f, WalkIn, WalkOff);
	TestTrue(TEXT("and walks off, its head back ahead"), Leaving.Phase == EPhase::WalkOff && FMath::IsNearlyEqual(Leaving.Along, 0.5f, 0.001f) && Leaving.HeadShare == 0.0f);
	const FMoment Setting = At(WalkIn + SniffSeconds + TurnSeconds + LookSeconds + 0.05f, WalkIn, WalkOff);
	TestTrue(TEXT("its head coming round as it sets off, not at once"), Setting.HeadShare > 0.5f && Setting.HeadShare < 1.0f);
	TestTrue(TEXT("every phase is a moment, not a flash"), SniffSeconds >= 1.5f && TurnSeconds >= 0.3f && LookSeconds >= 0.8f);
	TestTrue(TEXT("the head turns only so far"), HeadTurnToward(0.0f, 120.0f) == MostHeadTurn && HeadTurnToward(0.0f, -120.0f) == -MostHeadTurn && FMath::IsNearlyEqual(HeadTurnToward(170.0f, -170.0f), 20.0f, 0.001f));

	// Its path runs behind the figure's pedestal, across, and stops clear of the figure on the far side of it from where it
	// came in; its cheese is beyond the path, so it never walks over it.
	const float HalfWidth = 5.3f * Scale;
	const float PedestalRadius = ACheeseStrikeMenuFigure::PedestalWidth / 2.0f;
	TestTrue(TEXT("its path is behind the pedestal"), ACheeseStrikeMenuMouse::PathFrom.Y - HalfWidth > PedestalRadius && ACheeseStrikeMenuMouse::PathTo.Y - HalfWidth > PedestalRadius);
	TestTrue(TEXT("across the view, from one side to the other"), FMath::Abs(ACheeseStrikeMenuMouse::PathFrom.X) > 400.0f && FMath::Abs(ACheeseStrikeMenuMouse::PathTo.X) > 400.0f && ACheeseStrikeMenuMouse::PathFrom.X * ACheeseStrikeMenuMouse::PathTo.X < 0.0f);
	TestTrue(TEXT("it stops beside the figure, not behind it"), FMath::Abs(ACheeseStrikeMenuMouse::PathStop.X) > 100.0f);
	TestTrue(TEXT("its cheese is beyond its path"), ACheeseStrikeMenuMouse::CheeseAt.Y - ACheeseStrikeMenuMouse::CheeseWidth / 2.0f > ACheeseStrikeMenuMouse::PathStop.Y + HalfWidth);
	return true;
}

#endif
