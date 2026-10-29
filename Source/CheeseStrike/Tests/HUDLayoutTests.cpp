// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The HUD's layout rules, without a world: the soft, smooth edges of its
// rounded shapes; the score summary's words never running into its scores;
// what a side's score on the scoreboard says it counts; the kill-streak
// panel's track, words and pop; and the strike map's turning and following
// its mark.

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Rules/CheeseStrikeModes.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "UI/CheeseStrikeHUDRules.h"
#include "UI/CheeseStrikeLoader.h"
#include "Engine/Texture2D.h"

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
	TestEqual(TEXT("rounds won in a mode of rounds"), TeamScoreLabel(CheeseStrikeModes::EWinBy::Rounds, 2), FString(TEXT("ROUNDS WON")));
	TestEqual(TEXT("round one in a mode of rounds"), TeamScoreLabel(CheeseStrikeModes::EWinBy::Rounds, 1), FString(TEXT("ROUND WON")));
	TestEqual(TEXT("points on the hill"), TeamScoreLabel(CheeseStrikeModes::EWinBy::Hill, 2), FString(TEXT("POINTS")));
	TestEqual(TEXT("kills in Deathmatch"), TeamScoreLabel(CheeseStrikeModes::EWinBy::TeamKills, 2), FString(TEXT("KILLS")));
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
	TestEqual(TEXT("from nothing, the UAC in three"), StreakNext(0), FString(TEXT("UAC in 3")));
	TestEqual(TEXT("one off, the next kill"), StreakNext(4), FString(TEXT("Cheese Strike next kill")));
	TestEqual(TEXT("after the strike, the Gouda Overload"), StreakNext(5), FString(TEXT("Gouda Overload in 5")));
	TestEqual(TEXT("after the last"), StreakNext(10), FString(TEXT("EVERY STREAK EARNED")));
	TestEqual(TEXT("the UAC one off"), StreakNext(1), FString(TEXT("UAC in 2")));
	TestEqual(TEXT("the names as the panel says them"), StreakShortName(EStreak::CheeseStrike), FString(TEXT("Cheese Strike")));
	// The pop: the count and its tile are about the minimap's size at once (no growing in), hold half a second, and shrink back over half a second.
	TestTrue(TEXT("at rest before a kill"), StreakPop(-1.0f) == 1.0f && StreakPop(-0.001f) == 1.0f);
	TestTrue(TEXT("about the minimap's size from the kill itself: its side over the tile's"), FMath::IsNearlyEqual(StreakPopPeak * 58.0f, 250.0f, 1.0f) && StreakPop(0.0f) == StreakPopPeak && StreakPop(0.02f) == StreakPopPeak);
	TestTrue(TEXT("held for half a second"), FMath::IsNearlyEqual(StreakPopHold, 0.5f) && StreakPop(StreakPopHold - 0.01f) == StreakPopPeak);
	TestTrue(TEXT("then shrinks over half a second"), FMath::IsNearlyEqual(StreakPopShrink, 0.5f) && FMath::IsNearlyEqual(StreakPopTime, 1.0f));
	const float ShrinkStart = StreakPopHold;
	TestTrue(TEXT("easing down, steady through the middle, gentle at both ends"), StreakPop(ShrinkStart + 0.05f) > StreakPopPeak - 0.1f * (StreakPopPeak - 1.0f) && StreakPop(ShrinkStart + 0.25f) < StreakPop(ShrinkStart + 0.1f) && StreakPop(ShrinkStart + 0.25f) > StreakPop(ShrinkStart + 0.4f) && StreakPop(ShrinkStart + 0.45f) < 1.0f + 0.1f * (StreakPopPeak - 1.0f));
	TestTrue(TEXT("halfway down at the middle"), FMath::IsNearlyEqual(StreakPop(ShrinkStart + 0.25f), (1.0f + StreakPopPeak) / 2.0f, 0.01f));
	TestTrue(TEXT("and settled"), StreakPop(StreakPopTime) == 1.0f && FMath::IsNearlyEqual(StreakPop(StreakPopTime * 0.999f), 1.0f, 0.02f));
	{
		// Grown, the tile stands over the panel (and the health under it), never on them; at rest it is in its place; between, it comes down.
		const float PanelTop = 900.0f;
		const float Tile = 58.0f;
		const FVector2D Peak = StreakPopCorner(28.0f, PanelTop, Tile, StreakPopPeak, StreakPopClear);
		TestTrue(TEXT("at its peak the tile's bottom is over the panel's top"), Peak.Y + Tile * StreakPopPeak <= PanelTop - StreakPopClear + 0.01f && Peak.X == 28.0f);
		TestTrue(TEXT("at rest it is in its place"), StreakPopCorner(28.0f, PanelTop, Tile, 1.0f, StreakPopClear).Equals(FVector2D(28.0f, PanelTop)));
		const float Half = (1.0f + StreakPopPeak) / 2.0f;
		const float HalfBottom = StreakPopCorner(28.0f, PanelTop, Tile, Half, StreakPopClear).Y + Tile * Half;
		TestTrue(TEXT("halfway, its bottom is between the two"), HalfBottom > PanelTop - StreakPopClear && HalfBottom < PanelTop + Tile);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryStreakHasAnIconInCheese, "CheeseStrike.HUD.EveryStreakHasAnIconInCheese", HUDLayoutTests::Flags)
bool FEveryStreakHasAnIconInCheese::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	using namespace CheeseStrikeStreaks;
	TestTrue(TEXT("no streak, no icon"), StreakIconPath(EStreak::None).IsEmpty());
	TSet<FString> Seen;
	for (int32 Index = 1; Index <= NumStreaks; ++Index)
	{
		const EStreak Kind = StreakAt(Index);
		const FString Path = StreakIconPath(Kind);
		TestTrue(FString::Printf(TEXT("%s's icon is in the streaks' folder"), Info(Kind).Name), Path.StartsWith(TEXT("/Game/CheeseStrike/UI/Streaks/T_Streak_")));
		TestFalse(FString::Printf(TEXT("%s's icon is its own"), Info(Kind).Name), Seen.Contains(Path));
		Seen.Add(Path);
		// Rendered by tools/art/streak_icons.py and imported by import_streak_icons.py: 128 pixels square, with a clear background.
		const UTexture2D* Picture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (TestNotNull(FString::Printf(TEXT("%s's icon is there"), Info(Kind).Name), Picture))
		{
#if WITH_EDITORONLY_DATA
			// The source picture's size (the headless editor may not have built the texture for the renderer).
			TestTrue(FString::Printf(TEXT("%s's icon is 128 pixels square"), Info(Kind).Name), Picture->Source.GetSizeX() == 128 && Picture->Source.GetSizeY() == 128);
#endif
			TestFalse(FString::Printf(TEXT("%s's icon keeps its alpha"), Info(Kind).Name), Picture->CompressionNoAlpha);
		}
	}
	// Lit once made, dim until then.
	TestEqual(TEXT("made, in its own colours"), StreakIconTint(true), FLinearColor::White);
	const FLinearColor Dim = StreakIconTint(false);
	TestTrue(TEXT("not yet, darkened and faded"), Dim.R < 0.6f && Dim.A < 0.7f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCoverFitsAtAnyResolution, "CheeseStrike.HUD.TheCoverFitsAtAnyResolution", HUDLayoutTests::Flags)

bool FTheCoverFitsAtAnyResolution::RunTest(const FString& Parameters)
{
	// The cover's content lays out in its own size, drawn at the target scale: the loading screen gives a scale of 1, the game's
	// viewport the target itself, and at 1280x720 up to 3840x2160 the targets run from 2/3 to 2 (the DPI curve).
	const FVector2D Content(480.0, 40.0);
	for (const float Target : {2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 2.0f})
	{
		for (const float Given : {1.0f, Target, 0.5f * Target})
		{
			const FVector2D Wants = SCheeseStrikeSteadyScale::DesiredFor(Content, Target, Given);
			const FVector2D Area = SCheeseStrikeSteadyScale::ContentArea(Wants, Target, Given);
			TestTrue(*FString::Printf(TEXT("the content has all of its own size at target %.2f given %.2f"), Target, Given), FMath::IsNearlyEqual(Area.X, Content.X, 0.01) && FMath::IsNearlyEqual(Area.Y, Content.Y, 0.01));
			// Drawn on screen: the box it asked for, in pixels, is as big as it is drawn.
			TestTrue(TEXT("and it is drawn within the box it was given"), FMath::IsNearlyEqual(Content.X * SCheeseStrikeSteadyScale::RelativeScale(Target, Given) * Given, Wants.X * Given, 0.01));
		}
	}
	TestTrue(TEXT("above 1920x1080 on the loading screen the box is larger than the content, not smaller"), SCheeseStrikeSteadyScale::DesiredFor(Content, 2.0f, 1.0f).X > Content.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheStrikeMapShowsTheWholeLevel, "CheeseStrike.HUD.TheStrikeMapShowsTheWholeLevel", HUDLayoutTests::Flags)

bool FTheStrikeMapShowsTheWholeLevel::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// The way back from the map to the world: what MapPoint does, undone, at any turn.
	for (const float Yaw : {0.0f, 37.0f, 90.0f, -150.0f})
	{
		const FVector2D Offset(1234.0f, -567.0f);
		TestTrue(FString::Printf(TEXT("there and back at %.0f degrees"), Yaw), HUDLayoutTests::Near(MapOffset(MapPoint(Offset, Yaw, 0.08f), Yaw, 0.08f), Offset, 0.1f));
	}
	TestTrue(TEXT("up the map is the way it is turned"), HUDLayoutTests::Near(MapOffset(FVector2D(0.0f, -100.0f), 90.0f, 1.0f), FVector2D(0.0f, 100.0f), 0.01f));
	// The map shows the whole level at once: every corner of it, turned as the map is, inside the square with its border.
	const FVector2D Min(-4000.0f, -1000.0f);
	const FVector2D Max(6000.0f, 3000.0f);
	for (const float Yaw : {0.0f, 37.0f, 90.0f, -150.0f})
	{
		const FMapFit Fit = FitMap(Min, Max, Yaw, 620.0f, 20.0f);
		TestTrue(TEXT("its middle is the level's"), HUDLayoutTests::Near(Fit.Middle, FVector2D(1000.0f, 1000.0f)));
		float Furthest = 0.0f;
		for (const FVector2D& Corner : {Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y)})
		{
			const FVector2D On = MapPoint(Corner - Fit.Middle, Yaw, Fit.Scale);
			Furthest = FMath::Max(Furthest, FMath::Max(FMath::Abs(On.X), FMath::Abs(On.Y)));
		}
		TestTrue(FString::Printf(TEXT("the level just fits the square at %.0f degrees (%.2f of 290 px)"), Yaw, Furthest), FMath::IsNearlyEqual(Furthest, 290.0f, 0.1f));
	}
	TestTrue(TEXT("a level with no size has no scale"), FitMap(FVector2D::ZeroVector, FVector2D::ZeroVector, 0.0f, 620.0f, 20.0f).Scale == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheVerminWordsComeInBigAndAHitThumpsSoftly, "CheeseStrike.HUD.TheVerminWordsComeInBigAndAHitThumpsSoftly", HUDLayoutTests::Flags)

bool FTheVerminWordsComeInBigAndAHitThumpsSoftly::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const FVerminLook First = VerminLook(0.0f, -1.0f);
	TestTrue(TEXT("they start 2.5 times as big and clear"), FMath::IsNearlyEqual(First.Scale, 2.5f, 0.01f) && First.Alpha < 0.01f);
	TestTrue(TEXT("shrink past 1 a little"), VerminLook(VerminEntrance * 0.7f, -1.0f).Scale < 1.0f);
	const FVerminLook Settled = VerminLook(VerminEntrance + 1.0f, -1.0f);
	TestTrue(TEXT("and settle solid at 1"), Settled.Scale == 1.0f && Settled.Alpha == 1.0f);
	TestTrue(TEXT("each counted second swells them"), VerminLook(VerminEntrance + 1.0f, 0.0f).Scale > 1.1f && VerminLook(VerminEntrance + 1.0f, VerminPulse).Scale == 1.0f);
	// A hit's thump is short, soft and not silent.
	const TArray<int16> Thump = CheeseStrikeSynth::Thump(1.0f);
	int32 Loudest = 0;
	for (const int16 Sample : Thump)
	{
		Loudest = FMath::Max(Loudest, FMath::Abs(static_cast<int32>(Sample)));
	}
	TestTrue(TEXT("about 80 ms"), FMath::Abs(Thump.Num() - CheeseStrikeSynth::SampleRate * 8 / 100) <= 2);
	TestTrue(TEXT("audible but not loud"), Loudest > 2000 && Loudest < 30000);
	return true;
}

#endif
