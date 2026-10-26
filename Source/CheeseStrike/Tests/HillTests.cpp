// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// King of the Hill, without a world: the modes, who holds the hill (alone,
// contested, in teams and in the solo mode), the points it earns, the hill
// moving round its spots, where players come back, what a bot does, and
// what the HUD says about it.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeHills.h"
#include "Rules/CheeseStrikeModes.h"
#include "Map/CheeseStrikeHill.h"
#include "UI/CheeseStrikeHillHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HillTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	CheeseStrikeHills::FHold Held(int32 Holder)
	{
		CheeseStrikeHills::FHold Hold;
		Hold.State = CheeseStrikeHills::EHold::Held;
		Hold.Holder = Holder;
		return Hold;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKingOfTheHillIsAMode, "CheeseStrike.Rules.KingOfTheHillIsAMode", HillTests::Flags)

bool FKingOfTheHillIsAMode::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	const FModeInfo& Teams = Info(EMode::KingOfTheHill);
	const FModeInfo& Solo = Info(EMode::SoloKingOfTheHill);
	TestEqual(TEXT("named"), FString(Teams.Name), FString(TEXT("King of the Hill")));
	TestTrue(TEXT("in two teams"), Teams.bTeams);
	TestFalse(TEXT("and the solo mode everyone for themself"), Solo.bTeams);
	TestTrue(TEXT("both won by the hill"), Teams.WinBy == EWinBy::Hill && Solo.WinBy == EWinBy::Hill);
	TestTrue(TEXT("the killed come back, as in Deathmatch"), Teams.bRespawns && Solo.bRespawns);
	TestTrue(TEXT("with streaks"), Teams.bStreaks && Solo.bStreaks);
	TestFalse(TEXT("and no last stand"), Teams.bLastStand || Solo.bLastStand);
	TestTrue(TEXT("by its key"), ModeFrom(TEXT("?Mode=KingOfTheHill")) == EMode::KingOfTheHill && ModeFrom(TEXT("?Mode=SoloKingOfTheHill")) == EMode::SoloKingOfTheHill);
	TestEqual(TEXT("a side plays to 150 points"), KillLimitFor(EMode::KingOfTheHill, TEXT("")), CheeseStrikeHills::TeamTarget);
	TestEqual(TEXT("a player to 100"), KillLimitFor(EMode::SoloKingOfTheHill, TEXT("")), CheeseStrikeHills::SoloTarget);
	TestEqual(TEXT("unless the host says"), KillLimitFor(EMode::KingOfTheHill, TEXT("?KillLimit=200")), 200);
	TestTrue(TEXT("the usual targets are among the host's choices"), MakeArrayView(CheeseStrikeHills::TargetChoices).Contains(CheeseStrikeHills::TeamTarget) && MakeArrayView(CheeseStrikeHills::TargetChoices).Contains(CheeseStrikeHills::SoloTarget));
	TestEqual(TEXT("what it plays to"), Goal(EMode::KingOfTheHill, 150, 4), FString(TEXT("first team to 150 points")));
	TestEqual(TEXT("and the solo mode"), Goal(EMode::SoloKingOfTheHill, 100, 4), FString(TEXT("first to 100 points")));
	TestEqual(TEXT("the scoreboard's title"), Title(EMode::SoloKingOfTheHill), FString(TEXT("SOLO KING OF THE HILL")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHillIsHeldAloneOrContested, "CheeseStrike.Rules.TheHillIsHeldAloneOrContested", HillTests::Flags)

bool FTheHillIsHeldAloneOrContested::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHills;
	TestTrue(TEXT("nobody in it: nobody holds it"), HoldOf(true, {}).State == EHold::Neutral && HoldOf(true, {}).Holder == INDEX_NONE);
	// Teams: a side alone holds it, however many of it stand there; both sides contest it.
	const FInHill GoudaA{0, 11};
	const FInHill GoudaB{0, 12};
	const FInHill Mozza{1, 21};
	const FInHill Alone[] = {GoudaA};
	const FInHill Together[] = {GoudaA, GoudaB};
	const FInHill Both[] = {GoudaA, Mozza, GoudaB};
	TestTrue(TEXT("one player holds it for their side"), HoldOf(true, Alone) == HillTests::Held(0));
	TestTrue(TEXT("so do two of a side"), HoldOf(true, Together) == HillTests::Held(0));
	TestTrue(TEXT("one of each side contests it"), HoldOf(true, Both).State == EHold::Contested && HoldOf(true, Both).Holder == INDEX_NONE);
	const FInHill OnlyMozza[] = {Mozza};
	TestTrue(TEXT("the other side alone holds it for theirs"), HoldOf(true, OnlyMozza) == HillTests::Held(1));
	// Solo: a player holds it alone; anyone else in it contests it, sides or none.
	TestTrue(TEXT("solo: one player alone holds it, by who they are"), HoldOf(false, Alone) == HillTests::Held(11));
	TestTrue(TEXT("solo: two players contest it, even of the same side"), HoldOf(false, Together).State == EHold::Contested);

	// Standing in it: within its radius, from a little below its floor to well above it.
	const FVector Spot(1000.0f, 0.0f, 400.0f);
	TestTrue(TEXT("in it, at its middle"), IsInside(Spot, Spot, 400.0f));
	TestTrue(TEXT("in it, near its edge"), IsInside(Spot + FVector(390.0f, 0.0f, 0.0f), Spot, 400.0f));
	TestFalse(TEXT("out of it, past its edge"), IsInside(Spot + FVector(290.0f, 290.0f, 0.0f), Spot, 400.0f));
	TestTrue(TEXT("in it, up a step or a crate"), IsInside(Spot + FVector(0.0f, 0.0f, 150.0f), Spot, 400.0f));
	TestFalse(TEXT("not from a floor above"), IsInside(Spot + FVector(0.0f, 0.0f, Above + 50.0f), Spot, 400.0f));
	TestFalse(TEXT("nor from a floor below"), IsInside(Spot - FVector(0.0f, 0.0f, 400.0f), Spot, 400.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldingTheHillScores, "CheeseStrike.Rules.HoldingTheHillScores", HillTests::Flags)

bool FHoldingTheHillScores::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHills;
	FClock Clock;
	const FHold Gouda = HillTests::Held(0);
	const FHold Mozza = HillTests::Held(1);
	FHold Contested;
	Contested.State = EHold::Contested;
	TestEqual(TEXT("half a second held earns nothing yet"), Clock.Advance(0.5f, Gouda), 0);
	TestEqual(TEXT("the second half earns a point"), Clock.Advance(0.5f, Gouda), PointsPerSecond);
	int32 Points = 0;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		Points += Clock.Advance(0.1f, Gouda);
	}
	TestEqual(TEXT("ten seconds held, a tenth at a time, earn ten points"), Points, 10 * PointsPerSecond);
	TestEqual(TEXT("contested, nobody scores"), Clock.Advance(3.0f, Contested), 0);
	TestEqual(TEXT("and the part of a second held before it is lost"), Clock.Advance(0.6f, Gouda), 0);
	TestEqual(TEXT("held again, the points start again from nothing"), Clock.Advance(0.4f, Gouda), PointsPerSecond);
	TestEqual(TEXT("a new holder starts from nothing too"), Clock.Advance(0.9f, Mozza), 0);
	TestEqual(TEXT("and scores its own second"), Clock.Advance(0.1f, Mozza), PointsPerSecond);
	TestEqual(TEXT("nobody in it, nobody scores"), Clock.Advance(5.0f, FHold()), 0);
	TestEqual(TEXT("a long frame held earns every whole second in it"), Clock.Advance(2.5f, Mozza), 2 * PointsPerSecond);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHillMovesRoundItsSpots, "CheeseStrike.Rules.TheHillMovesRoundItsSpots", HillTests::Flags)

bool FTheHillMovesRoundItsSpots::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHills;
	TestEqual(TEXT("from the first to the second"), NextSpot(0, 4), 1);
	TestEqual(TEXT("each in turn"), NextSpot(2, 4), 3);
	TestEqual(TEXT("from the last back to the first"), NextSpot(3, 4), 0);
	TestEqual(TEXT("one spot: it stays"), NextSpot(0, 1), 0);
	TestEqual(TEXT("no spots"), NextSpot(0, 0), 0);
	int32 Spot = 0;
	TArray<int32> Visited;
	for (int32 Move = 0; Move < 5; ++Move)
	{
		Visited.Add(Spot);
		Spot = NextSpot(Spot, 5);
	}
	TestEqual(TEXT("five moves visit five spots once each"), TSet<int32>(Visited).Num(), 5);
	TestEqual(TEXT("and come back to the first"), Spot, 0);
	TestTrue(TEXT("about a minute at each"), HillTime >= 45.0f && HillTime <= 75.0f);
	TestTrue(TEXT("warning a few seconds before it moves"), IsWarning(MoveWarning) && IsWarning(1.0f));
	TestFalse(TEXT("not before"), IsWarning(MoveWarning + 0.1f));
	TestFalse(TEXT("nor once it has moved"), IsWarning(0.0f));

	// Coming back: away from the hill, and from where it is going, while any start is.
	const FVector Starts[] = {FVector(0.0f, 0.0f, 0.0f), FVector(1000.0f, 0.0f, 0.0f), FVector(5000.0f, 0.0f, 0.0f), FVector(-5000.0f, 0.0f, 0.0f)};
	const TArray<int32> Away = AwayFromHill(Starts, FVector(500.0f, 0.0f, 0.0f), SpawnClear);
	TestTrue(TEXT("the starts near the hill are passed over"), !Away.Contains(0) && !Away.Contains(1));
	TestTrue(TEXT("the far ones kept"), Away.Contains(2) && Away.Contains(3));
	const TArray<int32> AwayFromBoth = AwayFromHill(Starts, FVector(500.0f, 0.0f, 0.0f), SpawnClear, true, FVector(4800.0f, 0.0f, 0.0f));
	TestTrue(TEXT("and while it warns, those by where it goes"), AwayFromBoth.Num() == 1 && AwayFromBoth[0] == 3);
	TestEqual(TEXT("with every start near it, any will do"), AwayFromHill(MakeArrayView(Starts, 2), FVector(500.0f, 0.0f, 0.0f), SpawnClear).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotsPlayTheHill, "CheeseStrike.Bots.PlayTheHill", HillTests::Flags)

bool FBotsPlayTheHill::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHills;
	TestTrue(TEXT("nobody about: to the hill"), BotPlan(false, false, 0.0f, false) == EBotPlan::GoToHill);
	TestTrue(TEXT("someone far off in sight: on to the hill, fighting on the way"), BotPlan(false, true, 3000.0f, false) == EBotPlan::GoToHill);
	TestTrue(TEXT("someone close: fight them"), BotPlan(false, true, BotFightRange - 100.0f, false) == EBotPlan::Fight);
	TestTrue(TEXT("in the hill: hold it"), BotPlan(true, false, 0.0f, false) == EBotPlan::HoldHill);
	TestTrue(TEXT("in the hill with someone close: hold it all the same, fighting from it"), BotPlan(true, true, 300.0f, false) == EBotPlan::HoldHill);
	TestTrue(TEXT("in the hill as it warns: hold it while it scores"), BotPlan(true, false, 0.0f, true) == EBotPlan::HoldHill);
	TestTrue(TEXT("out of it as it warns: on to where it goes"), BotPlan(false, false, 0.0f, true) == EBotPlan::GoToNext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHUDShowsTheHill, "CheeseStrike.HUD.ShowsTheHill", HillTests::Flags)

bool FTheHUDShowsTheHill::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHillHUD;
	using CheeseStrikeHills::EHold;
	TestEqual(TEXT("nobody's: capture it"), MarkerWord(EHold::Neutral, false), FString(TEXT("CAPTURE")));
	TestEqual(TEXT("the enemy's: capture it"), MarkerWord(EHold::Held, false), FString(TEXT("CAPTURE")));
	TestEqual(TEXT("ours: defend it"), MarkerWord(EHold::Held, true), FString(TEXT("DEFEND")));
	TestEqual(TEXT("contested"), MarkerWord(EHold::Contested, false), FString(TEXT("CONTESTED")));
	TestEqual(TEXT("the strip, for a side"), HoldWords(true, EHold::Held, false, TEXT("The Mozza Radicals")), FString(TEXT("THE MOZZA RADICALS HOLD THE HILL")));
	TestEqual(TEXT("and our side"), HoldWords(true, EHold::Held, true, TEXT("The Gouda Guys")), FString(TEXT("YOUR SIDE HOLDS THE HILL")));
	TestEqual(TEXT("for a player"), HoldWords(false, EHold::Held, false, TEXT("Brie Larson")), FString(TEXT("BRIE LARSON HOLDS THE HILL")));
	TestEqual(TEXT("and ourselves"), HoldWords(false, EHold::Held, true, TEXT("Me")), FString(TEXT("YOU HOLD THE HILL")));
	TestEqual(TEXT("nobody"), HoldWords(true, EHold::Neutral, false, FString()), FString(TEXT("NOBODY HOLDS THE HILL")));
	TestEqual(TEXT("contested"), HoldWords(false, EHold::Contested, false, FString()), FString(TEXT("CONTESTED")));
	TestEqual(TEXT("when it moves"), MoveWords(42.3f), FString(TEXT("MOVES IN 0:43")));
	TestEqual(TEXT("and a minute off"), MoveWords(60.0f), FString(TEXT("MOVES IN 1:00")));
	TestEqual(TEXT("as it warns"), MoveWords(4.2f), FString(TEXT("MOVING IN 5")));
	TestEqual(TEXT("how far"), DistanceWords(4249.0f), FString(TEXT("42 M")));
	// Nobody's hill is a grey, far from every colour of a hold: the sides', and the solo mode's own and other.
	const FLinearColor Nobody = ACheeseStrikeHill::Neutral;
	TestTrue(TEXT("nobody's is a grey"), Nobody.LinearRGBToHSV().G < 0.15f);
	for (const FLinearColor& Held : {CheeseStrikeModes::TeamColour(0), CheeseStrikeModes::TeamColour(1), ACheeseStrikeHill::Own, ACheeseStrikeHill::Other})
	{
		const float Apart = FMath::Sqrt(FMath::Square(Held.R - Nobody.R) + FMath::Square(Held.G - Nobody.G) + FMath::Square(Held.B - Nobody.B));
		// A colour (saturated), or far brighter.
		const bool bOther = Held.LinearRGBToHSV().G > 0.4f || Held.LinearRGBToHSV().B - Nobody.LinearRGBToHSV().B > 0.3f;
		TestTrue(FString::Printf(TEXT("and unlike a hold's colour (%.2f apart, and that one's saturation %.2f)"), Apart, Held.LinearRGBToHSV().G), Apart > 0.3f && bOther);
	}

	// The marker: where the hill is, on the screen; held to the edge of what the HUD leaves clear the way it lies, off it.
	const FBox2D Area(FVector2D(70.0f, 120.0f), FVector2D(1850.0f, 900.0f));
	bool bOnEdge = true;
	TestEqual(TEXT("on the screen, where it is"), MarkerAt(FVector2D(800.0f, 400.0f), Area, bOnEdge), FVector2D(800.0f, 400.0f));
	TestFalse(TEXT("not on the edge"), bOnEdge);
	const FVector2D Right = MarkerAt(FVector2D(5000.0f, 510.0f), Area, bOnEdge);
	TestTrue(TEXT("off to the right: on the right edge, level"), bOnEdge && FMath::IsNearlyEqual(Right.X, 1850.0f, 0.5f) && FMath::IsNearlyEqual(Right.Y, 510.0f, 0.5f));
	const FVector2D Below = MarkerAt(FVector2D(960.0f + 200.0f, 510.0f + 100000.0f), Area, bOnEdge);
	TestTrue(TEXT("behind, below the view: on the bottom edge, over the score"), bOnEdge && FMath::IsNearlyEqual(Below.Y, 900.0f, 0.5f) && Below.X > 960.0f && Below.X < 963.0f);
	const FVector2D Top = MarkerAt(FVector2D(960.0f, 60.0f), Area, bOnEdge);
	TestTrue(TEXT("up by the players along the top: held under them"), bOnEdge && FMath::IsNearlyEqual(Top.Y, 120.0f, 0.5f));
	const FVector2D Corner = MarkerAt(FVector2D(-4000.0f, -4000.0f), Area, bOnEdge);
	TestTrue(TEXT("off a corner: inside, on an edge"), bOnEdge && Corner.X >= 69.5f && Corner.Y >= 119.5f && (FMath::IsNearlyEqual(Corner.X, 70.0f, 0.5f) || FMath::IsNearlyEqual(Corner.Y, 120.0f, 0.5f)));
	return true;
}

#endif
