// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The HUD's layout rules, without a world: the soft, smooth edges of its
// rounded shapes; the score summary's words never running into its scores;
// what a side's score on the scoreboard says it counts; the kill-streak
// panel's track, words and pop; and the strike map's turning and following
// its mark.

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "UI/CheeseStrikeHUDRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HUDLayoutTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	bool Near(const FVector2D& A, const FVector2D& B, float Within = 0.01f)
	{
		return FVector2D::Distance(A, B) <= Within;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundedShapesHaveSmoothEdges, "CheeseStrike.HUD.RoundedShapesHaveSmoothEdges", HUDLayoutTests::Flags)

bool FRoundedShapesHaveSmoothEdges::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// Corners as round as their size needs: the chords never stray from the arc by more than CornerError of a pixel.
	for (const float Radius : {2.0f, 6.0f, 12.0f, 16.0f, 24.0f, 32.0f, 60.0f})
	{
		const int32 Steps = CornerSteps(Radius);
		const float Stray = Radius * (1.0f - FMath::Cos(UE_HALF_PI / Steps / 2.0f));
		TestTrue(FString::Printf(TEXT("a corner %.0f px round strays %.3f px with %d steps"), Radius, Stray, Steps), Stray <= CornerError + 1e-4f);
	}
	TestTrue(TEXT("more steps for a bigger corner"), CornerSteps(32.0f) > CornerSteps(8.0f));
	TestEqual(TEXT("a corner of no size is one step"), CornerSteps(0.0f), 1);
	const FVector2D Min(0.0f, 0.0f);
	const FVector2D Max(200.0f, 50.0f);
	TestEqual(TEXT("a rectangle's arcs have their radius's steps"), RoundedRect(Min, Max, 16.0f).Num(), 4 * (CornerSteps(16.0f) + 1));

	// The soft edge: each corner of a square moves out along its diagonal, both its edges by one.
	const TArray<FVector2D> Square = {FVector2D(0.0f, 0.0f), FVector2D(10.0f, 0.0f), FVector2D(10.0f, 10.0f), FVector2D(0.0f, 10.0f)};
	const TArray<FVector2D> Out = OutwardMiters(Square);
	TestTrue(TEXT("the top left out up and left"), HUDLayoutTests::Near(Out[0], FVector2D(-1.0f, -1.0f)));
	TestTrue(TEXT("the bottom right out down and right"), HUDLayoutTests::Near(Out[2], FVector2D(1.0f, 1.0f)));
	TArray<FVector2D> Backwards = Square;
	Algo::Reverse(Backwards);
	TestTrue(TEXT("outward whichever way round it goes"), HUDLayoutTests::Near(OutwardMiters(Backwards)[3], FVector2D(-1.0f, -1.0f)));
	// Round a rounded rectangle: every way out is outside it, moving the edge by one, and points that coincide get one too.
	const TArray<FVector2D> Outline = RoundedRect(Min, Max, 16.0f);
	const TArray<FVector2D> Ways = OutwardMiters(Outline);
	bool bOut = true;
	for (int32 Index = 0; Index < Outline.Num(); ++Index)
	{
		bOut &= FMath::IsNearlyEqual(InsideRoundedRect(Outline[Index] + Ways[Index] * 0.5f, Min, Max, 16.0f), -0.5f, 0.02f);
	}
	TestTrue(TEXT("half a way out is half a pixel outside the edge, all round"), bOut);
	const TArray<FVector2D> Sharp = RoundedRect(Min, Max, 0.001f, 4);
	TestTrue(TEXT("an arc of no radius goes out by its corner's diagonal"), HUDLayoutTests::Near(OutwardMiters(Sharp)[2], FVector2D(-1.0f, -1.0f), 0.05f));
	const TArray<FVector2D> Spike = {FVector2D(0.0f, 0.0f), FVector2D(100.0f, 1.0f), FVector2D(0.0f, 2.0f)};
	TestTrue(TEXT("a sharp point's way out is held"), OutwardMiters(Spike)[1].Size() <= MostMiter + 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheScoreSummaryKeepsNamesOffTheScores, "CheeseStrike.HUD.TheScoreSummaryKeepsNamesOffTheScores", HUDLayoutTests::Flags)

bool FTheScoreSummaryKeepsNamesOffTheScores::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// Letters as wide as the score's are at its size: 8 px for the words, 20 for the score's digits.
	const auto Words = [](const FString& Text) { return 8.0f * Text.Len(); };
	const auto Digits = [](const FString& Text) { return 20.0f * Text.Len(); };
	const float Width = 200.0f;
	const float Pad = 12.0f;
	const float Gap = 8.0f;
	const auto Fits = [&](const FScoreLine& Line, const FScoreBlockText& Text)
	{
		const float Score = Digits(FString::FromInt(Line.Score)) * Text.ScoreScale;
		return Pad + FMath::Max(Words(Text.Caption), Words(Text.Name)) + Gap + Score <= Width - Pad + 0.01f;
	};
	// A side's name over a hill's three-figure points: the name is cut, never under the score.
	FScoreLine Side{TEXT("The Gouda Guys of the Western Cellar"), FString(), 150, 0, true};
	FScoreBlockText Text = FitScoreBlock(Side, Width, Pad, Gap, Words, Words, Digits);
	TestTrue(FString::Printf(TEXT("a long side's name is cut (%s)"), *Text.Caption), Text.Caption.EndsWith(TEXT("...")));
	TestTrue(TEXT("in capitals"), Text.Caption.StartsWith(TEXT("THE GOUDA")));
	TestTrue(TEXT("and clear of the score"), Fits(Side, Text));
	TestEqual(TEXT("which is made as small as it may be first"), Text.ScoreScale, ScoreLeastScale);
	// The game's own sides' names: whole with two figures, and whole over three with the score a little smaller.
	FScoreLine Mozza{TEXT("Mozza Radicals"), FString(), 42, 1, false};
	Text = FitScoreBlock(Mozza, Width, Pad, Gap, Words, Words, Digits);
	TestEqual(TEXT("a side's name that fits is whole"), Text.Caption, FString(TEXT("MOZZA RADICALS")));
	TestEqual(TEXT("beside a score of its size"), Text.ScoreScale, 1.0f);
	FScoreLine Gouda{TEXT("The Gouda Guys"), FString(), 150, 0, true};
	Text = FitScoreBlock(Gouda, Width, Pad, Gap, Words, Words, Digits);
	TestEqual(TEXT("a side's name over a hill's three-figure points is whole"), Text.Caption, FString(TEXT("THE GOUDA GUYS")));
	TestTrue(FString::Printf(TEXT("the score a little smaller for it (%.2f)"), Text.ScoreScale), Text.ScoreScale < 1.0f && Text.ScoreScale >= ScoreLeastScale && Fits(Gouda, Text));
	// A player's long name under the caption, in a free-for-all.
	FScoreLine Player{TEXT("You"), TEXT("ReginaldCheddaringtonTheThirdEsq"), 7, INDEX_NONE, true};
	Text = FitScoreBlock(Player, Width, Pad, Gap, Words, Words, Digits);
	TestTrue(TEXT("a long name is cut too"), Text.Name.EndsWith(TEXT("...")) && Fits(Player, Text));
	TestEqual(TEXT("the caption beside it whole"), Text.Caption, FString(TEXT("YOU")));
	// A score too wide for half the block is shrunk to it, and the words still fit beside it.
	FScoreLine Wide{TEXT("Leader"), TEXT("Brie"), 123456, INDEX_NONE, false};
	Text = FitScoreBlock(Wide, Width, Pad, Gap, Words, Words, Digits);
	TestTrue(TEXT("a very wide score shrinks"), Text.ScoreScale < 1.0f);
	TestTrue(TEXT("to its share of the block"), FMath::IsNearlyEqual(Digits(TEXT("123456")) * Text.ScoreScale, (Width - 2.0f * Pad) * ScoreMostShare, 0.01f));
	TestTrue(TEXT("with the words beside it"), Fits(Wide, Text) && Text.Name == TEXT("BRIE"));
	// A small window draws everything smaller alike: still clear.
	const auto SmallWords = [](const FString& Text) { return 5.4f * Text.Len(); };
	const auto SmallDigits = [](const FString& Text) { return 13.4f * Text.Len(); };
	Text = FitScoreBlock(Side, Width * 0.667f, Pad * 0.667f, Gap * 0.667f, SmallWords, SmallWords, SmallDigits);
	TestTrue(TEXT("at 1280 by 720 too"), 0.667f * Pad + SmallWords(Text.Caption) + 0.667f * Gap + SmallDigits(TEXT("150")) * Text.ScoreScale <= 0.667f * (Width - Pad) + 0.01f);

	// The scoreboard says what a side's score counts.
	TestEqual(TEXT("rounds won in a mode of rounds"), TeamScoreLabel(CheeseStrikeModes::EWinBy::Rounds), FString(TEXT("ROUNDS WON")));
	TestEqual(TEXT("points on the hill"), TeamScoreLabel(CheeseStrikeModes::EWinBy::Hill), FString(TEXT("POINTS")));
	TestEqual(TEXT("kills in Deathmatch"), TeamScoreLabel(CheeseStrikeModes::EWinBy::TeamKills), FString(TEXT("KILLS")));
	TestTrue(TEXT("Elimination and Execution are modes of rounds"), CheeseStrikeModes::Info(CheeseStrikeModes::EMode::Elimination).WinBy == CheeseStrikeModes::EWinBy::Rounds
		&& CheeseStrikeModes::Info(CheeseStrikeModes::EMode::Execution).WinBy == CheeseStrikeModes::EWinBy::Rounds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheStreakPanelCountsToEachStreak, "CheeseStrike.HUD.TheStreakPanelCountsToEachStreak", HUDLayoutTests::Flags)

bool FTheStreakPanelCountsToEachStreak::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	using CheeseStrikeStreaks::EStreak;
	// A pip a kill up to the Overload's ten, the streaks' kills marked.
	const TArray<FStreakPip> None = StreakPips(0);
	if (TestEqual(TEXT("ten pips"), None.Num(), 10))
	{
		TestTrue(TEXT("none made with no kills"), !None.ContainsByPredicate([](const FStreakPip& Pip) { return Pip.bMade; }));
		TestTrue(TEXT("the third earns the drone"), None[2].Earns == EStreak::AerialCheese);
		TestTrue(TEXT("the fifth the Cheese Strike"), None[4].Earns == EStreak::CheeseStrike);
		TestTrue(TEXT("the tenth the Overload"), None[9].Earns == EStreak::GoudaOverload);
		TestTrue(TEXT("the rest nothing"), None[0].Earns == EStreak::None && None[5].Earns == EStreak::None);
	}
	const TArray<FStreakPip> Four = StreakPips(4);
	TestTrue(TEXT("four kills make four pips"), Four[3].bMade && !Four[4].bMade);
	TestTrue(TEXT("a run past ten has them all"), !StreakPips(13).ContainsByPredicate([](const FStreakPip& Pip) { return !Pip.bMade; }) && StreakPips(13).Num() == 10);
	// What comes next.
	TestEqual(TEXT("from nothing, the drone in three"), StreakNext(0), FString(TEXT("UNMANNED AERIAL CHEESE IN 3")));
	TestEqual(TEXT("one off, the next kill"), StreakNext(4), FString(TEXT("CHEESE STRIKE NEXT KILL")));
	TestEqual(TEXT("after the strike, the Overload"), StreakNext(5), FString(TEXT("GOUDA OVERLOAD IN 5")));
	TestEqual(TEXT("after the last"), StreakNext(10), FString(TEXT("EVERY STREAK EARNED")));
	TestEqual(TEXT("or short"), StreakNext(1, true), FString(TEXT("DRONE IN 2")));
	TestEqual(TEXT("the pips' words"), StreakShortName(EStreak::CheeseStrike), FString(TEXT("STRIKE")));
	// The pop: big at once, back through its size, a little under, and settled.
	TestTrue(TEXT("at rest before a kill"), StreakPop(-1.0f) == 1.0f);
	TestTrue(TEXT("springing up at the kill"), StreakPop(0.0f) > 1.4f);
	TestTrue(TEXT("back down soon"), StreakPop(StreakPopTime / 3.0f) < 1.05f);
	TestTrue(TEXT("a little under"), StreakPop(StreakPopTime * 0.6f) < 1.0f);
	TestTrue(TEXT("and settled"), StreakPop(StreakPopTime) == 1.0f && FMath::IsNearlyEqual(StreakPop(StreakPopTime * 0.999f), 1.0f, 0.01f));
	// The track fills toward the run, and a death drains it.
	float Fill = 0.0f;
	Fill = StreakFillToward(Fill, 3, 0.1f);
	TestTrue(TEXT("filling, not all at once"), Fill > 0.0f && Fill < 3.0f);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Fill = StreakFillToward(Fill, 3, 1.0f / 60.0f);
	}
	TestEqual(TEXT("then to the run"), Fill, 3.0f);
	Fill = StreakFillToward(10.0f, 0, StreakDrainTime / 2.0f);
	TestTrue(TEXT("a whole track drains halfway in half the time"), FMath::IsNearlyEqual(Fill, 5.0f, 0.01f));
	TestEqual(TEXT("and empty in all of it"), StreakFillToward(Fill, 0, StreakDrainTime / 2.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheStrikeMapFollowsItsMark, "CheeseStrike.HUD.TheStrikeMapFollowsItsMark", HUDLayoutTests::Flags)

bool FTheStrikeMapFollowsItsMark::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// The way back from the map to the world: what MapPoint does, undone, at any turn.
	for (const float Yaw : {0.0f, 37.0f, 90.0f, -150.0f})
	{
		const FVector2D Offset(1234.0f, -567.0f);
		TestTrue(FString::Printf(TEXT("there and back at %.0f degrees"), Yaw), HUDLayoutTests::Near(MapOffset(MapPoint(Offset, Yaw, 0.08f), Yaw, 0.08f), Offset, 0.1f));
	}
	TestTrue(TEXT("up the map is the way it is turned"), HUDLayoutTests::Near(MapOffset(FVector2D(0.0f, -100.0f), 90.0f, 1.0f), FVector2D(0.0f, 100.0f), 0.01f));
	// The map's middle stays while the mark is near it, and follows it just enough when it strays.
	const FVector2D Middle(500.0f, 500.0f);
	TestTrue(TEXT("a mark near the middle leaves it"), HUDLayoutTests::Near(FollowMark(Middle, Middle + FVector2D(300.0f, -200.0f), 0.0f, 1000.0f), Middle));
	const FVector2D Followed = FollowMark(Middle, Middle + FVector2D(2500.0f, 0.0f), 0.0f, 1000.0f);
	TestTrue(TEXT("one ahead beyond the reach brings it along"), HUDLayoutTests::Near(Followed, Middle + FVector2D(1500.0f, 0.0f), 0.1f));
	const FVector2D Turned = FollowMark(Middle, Middle + FVector2D(0.0f, -2500.0f), 90.0f, 1000.0f);
	TestTrue(TEXT("on a turned map, too"), HUDLayoutTests::Near(Turned, Middle + FVector2D(0.0f, -1500.0f), 0.1f));
	// The strike's area fits the map it is chosen on, wherever the mark is let go.
	TestTrue(TEXT("the area fits inside the strike map at the furthest the mark goes from its middle"), StrikeMapRange * StrikeMapFollow + CheeseStrikeStreaks::StrikeRadius < StrikeMapRange);
	return true;
}

#endif
