// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The HUD's rules, without a world: how the minimap turns and what of the
// level it shows, which way a damage arc points, how red the edges get, what
// the row of players and the score summary say, how name tags shrink, how
// the death screen's cheese runs and when it shows, how long names are cut,
// the rounded shapes, and when the crosshair says a shot would do nothing.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeHUDRules.h"
#include "Rules/CheeseStrikeModes.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace HUDTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	bool Near(const FVector2D& A, const FVector2D& B, float Within = 0.01f)
	{
		return FVector2D::Distance(A, B) <= Within;
	}

	CheeseStrikeRules::FStanding Line(const TCHAR* Name, int32 Kills, int32 Deaths, int32 Order)
	{
		CheeseStrikeRules::FStanding Standing;
		Standing.Name = Name;
		Standing.Kills = Kills;
		Standing.Deaths = Deaths;
		Standing.Order = Order;
		return Standing;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMinimapTurnsWithTheView, "CheeseStrike.HUD.TheMinimapTurnsWithTheView", HUDTests::Flags)

bool FTheMinimapTurnsWithTheView::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// Facing along +X: what is ahead is up, what is to the right (+Y) is to the right.
	TestTrue(TEXT("ahead is up"), HUDTests::Near(MapPoint(FVector2D(100, 0), 0.0f, 1.0f), FVector2D(0, -100)));
	TestTrue(TEXT("the right is to the right"), HUDTests::Near(MapPoint(FVector2D(0, 100), 0.0f, 1.0f), FVector2D(100, 0)));
	TestTrue(TEXT("behind is down"), HUDTests::Near(MapPoint(FVector2D(-100, 0), 0.0f, 1.0f), FVector2D(0, 100)));
	// Turned to face +Y: +Y is up now, and +X (to the left of +Y) is to the left.
	TestTrue(TEXT("turned a quarter right, what was to the right is ahead"), HUDTests::Near(MapPoint(FVector2D(0, 100), 90.0f, 1.0f), FVector2D(0, -100)));
	TestTrue(TEXT("and what was ahead is to the left"), HUDTests::Near(MapPoint(FVector2D(100, 0), 90.0f, 1.0f), FVector2D(-100, 0)));
	TestTrue(TEXT("at half a pixel a cm, half as far"), HUDTests::Near(MapPoint(FVector2D(100, 0), 0.0f, 0.5f), FVector2D(0, -50)));
	TestTrue(TEXT("turning keeps distances"), FMath::IsNearlyEqual(MapPoint(FVector2D(300, 400), 37.0f, 1.0f).Size(), 500.0f, 0.01f));

	// The N mark rides the square's edge where +Y is.
	TestTrue(TEXT("facing north, N is at the top"), HUDTests::Near(NorthMark(NorthYaw, 100.0f), FVector2D(0, -100)));
	TestTrue(TEXT("facing +X, north (+Y) is on the right, as it is in the view"), HUDTests::Near(NorthMark(0.0f, 100.0f), FVector2D(100, 0)));
	TestTrue(TEXT("facing south, N is at the bottom"), HUDTests::Near(NorthMark(NorthYaw + 180.0f, 100.0f), FVector2D(0, 100)));
	const FVector2D Corner = NorthMark(NorthYaw - 45.0f, 100.0f);
	TestTrue(FString::Printf(TEXT("half way round, in a corner (%.1f, %.1f)"), Corner.X, Corner.Y), HUDTests::Near(Corner, FVector2D(100, -100)));
	const FVector2D Side = NorthMark(NorthYaw - 20.0f, 100.0f);
	TestTrue(FString::Printf(TEXT("and always on the edge (%.1f, %.1f)"), Side.X, Side.Y), FMath::IsNearlyEqual(FMath::Max(FMath::Abs(Side.X), FMath::Abs(Side.Y)), 100.0f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMinimapCutsShapesToItsSquare, "CheeseStrike.HUD.TheMinimapCutsShapesToItsSquare", HUDTests::Flags)

bool FTheMinimapCutsShapesToItsSquare::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// A box's eight corners seen from above are a rectangle, the inner points left out.
	const TArray<FVector2D> Hull = ConvexHull({FVector2D(0, 0), FVector2D(10, 0), FVector2D(10, 5), FVector2D(0, 5), FVector2D(0, 0), FVector2D(10, 5), FVector2D(5, 2), FVector2D(3, 3)});
	TestEqual(TEXT("the hull of a box from above has four corners"), Hull.Num(), 4);
	TestEqual(TEXT("two points stay two"), ConvexHull({FVector2D(0, 0), FVector2D(1, 1)}).Num(), 2);
	TestTrue(TEXT("a wall 10 long and 5 thick is 5 wide"), FMath::IsNearlyEqual(NarrowestWidth(Hull), 5.0f, 0.01f));
	const TArray<FVector2D> Turned = {FVector2D(0, 0), FVector2D(300, 300), FVector2D(280, 320), FVector2D(-20, 20)};
	TestTrue(FString::Printf(TEXT("turned, still as thick (%.1f)"), NarrowestWidth(Turned)), FMath::IsNearlyEqual(NarrowestWidth(Turned), 20.0f * FMath::Sqrt(2.0f), 0.1f));

	const FVector2D Min(0, 0);
	const FVector2D Max(100, 100);
	const TArray<FVector2D> Inside = {FVector2D(10, 10), FVector2D(20, 10), FVector2D(20, 20)};
	TestEqual(TEXT("a shape inside is as it was"), ClipToRect(Inside, Min, Max), Inside);
	TestEqual(TEXT("a shape outside is gone"), ClipToRect({FVector2D(110, 10), FVector2D(120, 10), FVector2D(120, 20)}, Min, Max).Num(), 0);
	// A square across the corner: the part inside is the square from (50, 50) to (100, 100).
	const TArray<FVector2D> Cut = ClipToRect({FVector2D(50, 50), FVector2D(150, 50), FVector2D(150, 150), FVector2D(50, 150)}, Min, Max);
	TestEqual(TEXT("a square across the corner is cut to four corners"), Cut.Num(), 4);
	for (const FVector2D& Point : Cut)
	{
		TestTrue(FString::Printf(TEXT("each inside (%.0f, %.0f)"), Point.X, Point.Y), Point.X >= 50.0f && Point.X <= 100.0f && Point.Y >= 50.0f && Point.Y <= 100.0f);
	}
	// A big shape covering it all comes back as the square itself.
	const TArray<FVector2D> All = ClipToRect({FVector2D(-500, -500), FVector2D(500, -500), FVector2D(500, 500), FVector2D(-500, 500)}, Min, Max);
	float Area = 0.0f;
	for (int32 Index = 0; Index < All.Num(); ++Index)
	{
		Area += FVector2D::CrossProduct(All[Index], All[(Index + 1) % All.Num()]) / 2.0f;
	}
	TestTrue(FString::Printf(TEXT("a floor under it all fills the square (%.0f)"), FMath::Abs(Area)), FMath::IsNearlyEqual(FMath::Abs(Area), 10000.0f, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMinimapShowsWhatIsNearThePlayersHeight, "CheeseStrike.HUD.TheMinimapShowsWhatIsNearThePlayersHeight", HUDTests::Flags)

bool FTheMinimapShowsWhatIsNearThePlayersHeight::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// On the ground (feet at 0), in The Creamery's hall.
	TestEqual(TEXT("the floor stood on is floor"), LayerFor(-400.0f, 0.0f, 0.0f, false), ELayer::Floor);
	TestEqual(TEXT("a crate to crouch behind is low cover"), LayerFor(0.0f, 110.0f, 0.0f, false), ELayer::Low);
	TestEqual(TEXT("the hall's wall is a wall"), LayerFor(0.0f, 900.0f, 0.0f, false), ELayer::Wall);
	TestEqual(TEXT("the mezzanine overhead is left out"), LayerFor(370.0f, 400.0f, 0.0f, false), ELayer::Hidden);
	TestEqual(TEXT("the hall's roof is left out"), LayerFor(900.0f, 940.0f, 0.0f, false), ELayer::Hidden);
	TestEqual(TEXT("a lintel over a door is left out, so the door shows open"), LayerFor(250.0f, 300.0f, 0.0f, false), ELayer::Hidden);
	TestEqual(TEXT("the canal's floor is a level below"), LayerFor(-340.0f, -300.0f, 0.0f, false), ELayer::Below);
	TestEqual(TEXT("a ramp up to the mezzanine is floor"), LayerFor(0.0f, 430.0f, 0.0f, true), ELayer::Floor);
	// Up on the mezzanine (feet at 400).
	TestEqual(TEXT("from the mezzanine, its floor is floor"), LayerFor(370.0f, 400.0f, 400.0f, false), ELayer::Floor);
	TestEqual(TEXT("the hall's floor is below"), LayerFor(-400.0f, 0.0f, 400.0f, false), ELayer::Below);
	TestEqual(TEXT("its rail is low cover"), LayerFor(400.0f, 500.0f, 400.0f, false), ELayer::Low);
	TestEqual(TEXT("the racks' tops are below"), LayerFor(0.0f, 210.0f, 400.0f, false), ELayer::Below);
	// Down in the canal (feet at -300): its sides are walls.
	TestEqual(TEXT("from the canal, its wall is a wall"), LayerFor(-400.0f, 0.0f, -300.0f, false), ELayer::Wall);
	TestEqual(TEXT("and the wide ground beside it a level above"), LayerFor(-400.0f, 0.0f, -300.0f, false, true), ELayer::Raised);
	TestEqual(TEXT("a wide platform at waist height is still cover"), LayerFor(0.0f, 120.0f, 0.0f, false, true), ELayer::Low);
	TestEqual(TEXT("and a bridge over it is left out"), LayerFor(-40.0f, 0.0f, -300.0f, false), ELayer::Hidden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheAerialCheeseSweepsInPulses, "CheeseStrike.HUD.TheAerialCheeseSweepsInPulses", HUDTests::Flags)

bool FTheAerialCheeseSweepsInPulses::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// One called in at 100 for 25 s of sweeps every 2 s: it ends at 125.
	TestEqual(TEXT("none before it is called in"), SweepAt(99.0f, 125.0f, 25.0f, 2.0f).Index, INDEX_NONE);
	TestEqual(TEXT("none once it is over"), SweepAt(125.0f, 125.0f, 25.0f, 2.0f).Index, INDEX_NONE);
	TestEqual(TEXT("none without one"), SweepAt(50.0f, 0.0f, 25.0f, 2.0f).Index, INDEX_NONE);
	const FSweep First = SweepAt(100.0f, 125.0f, 25.0f, 2.0f);
	TestTrue(TEXT("the first sweep as it comes, at full strength"), First.Index == 0 && FMath::IsNearlyEqual(First.Fade, 1.0f));
	const FSweep Later = SweepAt(101.5f, 125.0f, 25.0f, 2.0f);
	TestTrue(FString::Printf(TEXT("fading towards the next (%.2f)"), Later.Fade), Later.Index == 0 && Later.Fade < First.Fade && Later.Fade > 0.3f);
	const FSweep Next = SweepAt(102.1f, 125.0f, 25.0f, 2.0f);
	TestTrue(TEXT("then the next sweep, full again"), Next.Index == 1 && Next.Fade > 0.95f && FMath::IsNearlyEqual(Next.Since, 0.1f, 0.001f));
	TestEqual(TEXT("the last sweep"), SweepAt(124.9f, 125.0f, 25.0f, 2.0f).Index, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDamageArcPointsAtTheShooter, "CheeseStrike.HUD.TheDamageArcPointsAtTheShooter", HUDTests::Flags)

bool FTheDamageArcPointsAtTheShooter::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const FVector2D At(1000, 1000);
	TestTrue(TEXT("a shooter ahead: the top"), FMath::IsNearlyEqual(DamageArcAngle(0.0f, At, At + FVector2D(500, 0)), 0.0f, 0.01f));
	TestTrue(TEXT("on the right (+Y facing +X): the right"), FMath::IsNearlyEqual(DamageArcAngle(0.0f, At, At + FVector2D(0, 500)), 90.0f, 0.01f));
	TestTrue(TEXT("on the left: the left"), FMath::IsNearlyEqual(DamageArcAngle(0.0f, At, At + FVector2D(0, -500)), -90.0f, 0.01f));
	TestTrue(TEXT("behind: the bottom"), FMath::IsNearlyEqual(FMath::Abs(DamageArcAngle(0.0f, At, At + FVector2D(-500, 0))), 180.0f, 0.01f));
	TestTrue(TEXT("it is the view that counts: turned to face the shooter, the top"), FMath::IsNearlyEqual(DamageArcAngle(90.0f, At, At + FVector2D(0, 500)), 0.0f, 0.01f));
	TestTrue(TEXT("turned away from them, behind"), FMath::IsNearlyEqual(FMath::Abs(DamageArcAngle(-90.0f, At, At + FVector2D(0, 500))), 180.0f, 0.01f));
	TestTrue(TEXT("half right, half way round"), FMath::IsNearlyEqual(DamageArcAngle(0.0f, At, At + FVector2D(300, 300)), 45.0f, 0.01f));
	TestTrue(TEXT("from straight above or below, the top"), FMath::IsNearlyEqual(DamageArcAngle(30.0f, At, At), 0.0f, 0.01f));
	TestTrue(TEXT("however far round the view has turned"), FMath::IsNearlyEqual(DamageArcAngle(720.0f + 90.0f, At, At + FVector2D(0, 500)), 0.0f, 0.01f));

	TestEqual(TEXT("an arc is full when hit"), DamageArcFade(0.0f), 1.0f);
	TestEqual(TEXT("and for a moment after"), DamageArcFade(0.3f), 1.0f);
	TestTrue(TEXT("then fades"), DamageArcFade(1.0f) < 1.0f && DamageArcFade(1.0f) > DamageArcFade(1.4f));
	TestEqual(TEXT("and is gone when its life is over"), DamageArcFade(DamageArcLife), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheEdgesRedAsHealthFalls, "CheeseStrike.HUD.TheEdgesRedAsHealthFalls", HUDTests::Flags)

bool FTheEdgesRedAsHealthFalls::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("at full health, no red"), VignetteFor(100.0f, 100.0f, 0.0f).Alpha, 0.0f);
	TestEqual(TEXT("nor while health is fair"), VignetteFor(VignetteFrom, 100.0f, 0.0f).Alpha, 0.0f);
	const FVignette Hurt = VignetteFor(50.0f, 100.0f, 0.0f);
	const FVignette Worse = VignetteFor(30.0f, 100.0f, 0.0f);
	TestTrue(TEXT("hurt, red edges"), Hurt.Alpha > 0.0f);
	TestTrue(TEXT("stronger and reaching further in as health falls"), Worse.Alpha > Hurt.Alpha && Worse.Reach > Hurt.Reach);
	// Very low, they breathe: different at different moments, and never gone.
	float Least = 1.0f;
	float Most = 0.0f;
	for (float Seconds = 0.0f; Seconds < 1.0f; Seconds += 0.05f)
	{
		const float Alpha = VignetteFor(10.0f, 100.0f, Seconds).Alpha;
		Least = FMath::Min(Least, Alpha);
		Most = FMath::Max(Most, Alpha);
	}
	TestTrue(FString::Printf(TEXT("very low, they pulse (%.2f to %.2f)"), Least, Most), Most - Least > 0.05f && Least > Worse.Alpha * 0.9f);
	TestTrue(TEXT("gently"), Most - Least < 0.3f);
	const float Steady = VignetteFor(30.0f, 100.0f, 0.25f).Alpha;
	TestEqual(TEXT("above that, steady"), Steady, Worse.Alpha);
	TestEqual(TEXT("dead, none: the death screen shows"), VignetteFor(0.0f, 100.0f, 0.0f).Alpha, 0.0f);
	TestTrue(TEXT("tough cheese (200 health) reds by its share of health"), FMath::IsNearlyEqual(VignetteFor(100.0f, 200.0f, 0.0f).Alpha, Hurt.Alpha));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealthShowsAsBlocksNotANumber, "CheeseStrike.HUD.HealthShowsAsBlocksNotANumber", HUDTests::Flags)

bool FHealthShowsAsBlocksNotANumber::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const auto Sum = [](const TArray<float>& Fills) { float Total = 0.0f; for (float Fill : Fills) { Total += Fill; } return Total; };
	const TArray<float> Full = HealthFills(100.0f, 100.0f);
	TestEqual(TEXT("a row of blocks"), Full.Num(), HealthSegments);
	TestEqual(TEXT("at full health, every block full"), Sum(Full), static_cast<float>(HealthSegments));
	const TArray<float> Hurt = HealthFills(65.0f, 100.0f);
	TestTrue(TEXT("65 of 100: three full, the fourth a quarter, the last empty"), Hurt[2] == 1.0f && FMath::IsNearlyEqual(Hurt[3], 0.25f) && Hurt[4] == 0.0f);
	TestTrue(TEXT("the blocks hold the share of health"), FMath::IsNearlyEqual(Sum(Hurt) / HealthSegments, 0.65f));
	TestTrue(TEXT("filled from the left"), Hurt[0] >= Hurt[1] && Hurt[1] >= Hurt[2] && Hurt[2] >= Hurt[3] && Hurt[3] >= Hurt[4]);
	TestTrue(TEXT("a sliver alive never looks empty"), HealthFills(1.0f, 100.0f)[0] > 0.05f);
	TestEqual(TEXT("dead, empty"), Sum(HealthFills(0.0f, 100.0f)), 0.0f);
	TestEqual(TEXT("over full is full"), Sum(HealthFills(150.0f, 100.0f)), static_cast<float>(HealthSegments));
	TestTrue(TEXT("tough cheese fills by its share"), FMath::IsNearlyEqual(Sum(HealthFills(100.0f, 200.0f)), HealthSegments * 0.5f));
	TestTrue(TEXT("low at a quarter, as the edges pulse"), IsHealthLow(VignettePulseUnder, 100.0f) && !IsHealthLow(VignettePulseUnder + 1.0f, 100.0f));
	TestFalse(TEXT("dead isn't low"), IsHealthLow(0.0f, 100.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThePlayersRowSaysWhoIsAlive, "CheeseStrike.HUD.ThePlayersRowSaysWhoIsAlive", HUDTests::Flags)

bool FThePlayersRowSaysWhoIsAlive::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("alive"), StatusOf(false, false), EStatus::Alive);
	TestEqual(TEXT("killed and coming back"), StatusOf(false, true), EStatus::Respawning);
	TestEqual(TEXT("out is out, dead or not"), StatusOf(true, true), EStatus::Out);
	TestEqual(TEXT("down, alive"), StatusOf(false, false, true), EStatus::Down);
	TestEqual(TEXT("down and dead is dead"), StatusOf(false, true, true), EStatus::Respawning);
	TestEqual(TEXT("down and out is out"), StatusOf(true, true, true), EStatus::Out);
	TestEqual(TEXT("killed at 10 with a 3 s delay, at 10.5: back in 3"), SecondsToComeBack(3.0f, 10.5f, 10.0f), 3);
	TestEqual(TEXT("at 12.2: back in 1"), SecondsToComeBack(3.0f, 12.2f, 10.0f), 1);
	TestEqual(TEXT("late: 0, never less"), SecondsToComeBack(3.0f, 14.0f, 10.0f), 0);
	TestEqual(TEXT("a joiner's clock a little behind the death: the whole delay, never more"), SecondsToComeBack(3.0f, 9.8f, 10.0f), 3);

	TestEqual(TEXT("a Steam id from a Steam player's id"), SteamIdFrom(TEXT("STEAM"), TEXT("76561197960287930")), static_cast<uint64>(76561197960287930ull));
	TestEqual(TEXT("none from the Null service's"), SteamIdFrom(TEXT("NULL"), TEXT("76561197960287930")), static_cast<uint64>(0));
	TestEqual(TEXT("none from a Steam id that isn't a number"), SteamIdFrom(TEXT("STEAM"), TEXT("Host-12")), static_cast<uint64>(0));
	TestEqual(TEXT("none from nothing"), SteamIdFrom(TEXT("STEAM"), TEXT("")), static_cast<uint64>(0));

	TestEqual(TEXT("a bot's initials"), Initials(TEXT("Gorgon Zola")), FString(TEXT("GZ")));
	TestEqual(TEXT("one word's first two letters"), Initials(TEXT("Host")), FString(TEXT("HO")));
	TestEqual(TEXT("in capitals, whatever the spacing"), Initials(TEXT("  camem   bert ")), FString(TEXT("CB")));
	TestEqual(TEXT("something for nobody"), Initials(TEXT("")), FString(TEXT("?")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheScoreboardShowsStatusAndWhoPlays, "CheeseStrike.HUD.TheScoreboardShowsStatusAndWhoPlays", HUDTests::Flags)

bool FTheScoreboardShowsStatusAndWhoPlays::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("alive: the status column is blank"), StatusWord(EStatus::Alive), FString());
	TestEqual(TEXT("down in Execution"), StatusWord(EStatus::Down), FString(TEXT("DOWN")));
	TestEqual(TEXT("out of the round"), StatusWord(EStatus::Out), FString(TEXT("OUT")));
	TestEqual(TEXT("waiting to come back"), StatusWord(EStatus::Respawning), FString(TEXT("DEAD")));
	TestEqual(TEXT("down and out says out"), StatusWord(StatusOf(true, false, true)), FString(TEXT("OUT")));
	TestEqual(TEXT("an alive row is solid"), RowAlpha(EStatus::Alive), 1.0f);
	TestEqual(TEXT("so is a down one"), RowAlpha(EStatus::Down), 1.0f);
	TestTrue(TEXT("out fades more than dead"), RowAlpha(EStatus::Out) < RowAlpha(EStatus::Respawning) && RowAlpha(EStatus::Respawning) < 1.0f);

	TestEqual(TEXT("a bot is a bot"), KindOf(true), EPlayerKind::Bot);
	TestEqual(TEXT("anyone else a person"), KindOf(false), EPlayerKind::Person);
	const FBoardRow Bot = BoardRow(HUDTests::Line(TEXT("Gorgon Zola"), 4, 2, 7), EStatus::Down, true, false);
	TestEqual(TEXT("a row keeps the name"), Bot.Name, FString(TEXT("Gorgon Zola")));
	TestEqual(TEXT("the kills"), Bot.Kills, 4);
	TestEqual(TEXT("the deaths"), Bot.Deaths, 2);
	TestEqual(TEXT("the status"), Bot.Status, EStatus::Down);
	TestEqual(TEXT("a bot's row shows a robot"), Bot.Kind, EPlayerKind::Bot);
	TestFalse(TEXT("not mine"), Bot.bMine);
	const FBoardRow Me = BoardRow(HUDTests::Line(TEXT("Host"), 0, 0, 1), EStatus::Alive, false, true);
	TestEqual(TEXT("a player's row shows a person"), Me.Kind, EPlayerKind::Person);
	TestTrue(TEXT("mine"), Me.bMine);

	// Names are cut to their column, measured as ten pixels a letter.
	const auto Measure = [](const FString& Text) { return 10.0f * Text.Len(); };
	TestEqual(TEXT("a name that fits is whole"), FitName(TEXT("Host"), 100.0f, Measure), FString(TEXT("Host")));
	TestEqual(TEXT("exactly fitting is whole"), FitName(TEXT("0123456789"), 100.0f, Measure), FString(TEXT("0123456789")));
	TestEqual(TEXT("a long name is cut with dots, within the width"), FitName(TEXT("Sir Reginald Cheddarington"), 100.0f, Measure), FString(TEXT("Sir Reg...")));
	TestEqual(TEXT("no space left before the dots"), FitName(TEXT("Sir Reginald"), 70.0f, Measure), FString(TEXT("Sir...")));
	TestEqual(TEXT("never less than a letter"), FitName(TEXT("Cheddar"), 5.0f, Measure), FString(TEXT("C...")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLongNamesAreCutWithDots, "CheeseStrike.HUD.LongNamesAreCutWithDots", HUDTests::Flags)
bool FLongNamesAreCutWithDots::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// As long as a Steam name may be, with no space to break at; measured as ten pixels a letter.
	const FString Longest = TEXT("ReginaldCheddaringtonTheThirdEsq");
	TestEqual(TEXT("the longest name a Steam account has"), Longest.Len(), CheeseStrikeRules::MaxNameLength);
	const auto Measure = [](const FString& Text) { return 10.0f * Text.Len(); };
	const FString Cut = FitName(Longest, 200.0f, Measure);
	TestEqual(TEXT("cut with dots, with no space to cut at"), Cut, FString(TEXT("ReginaldCheddarin...")));
	TestTrue(TEXT("within the width"), Measure(Cut) <= 200.0f);
	TestEqual(TEXT("whole where it fits"), FitName(Longest, 320.0f, Measure), Longest);

	// In a line, only the name is cut; the rest of the line is kept, whatever its case.
	TestEqual(TEXT("an announcement's name cut"), FitNameIn(TEXT("REGINALDCHEDDARINGTONTHETHIRDESQ CALLS IN A CHEESE STRIKE"), Longest.ToUpper(), 100.0f, Measure),
		FString(TEXT("REGINAL... CALLS IN A CHEESE STRIKE")));
	TestEqual(TEXT("found as it is written"), FitNameIn(TEXT("KILLED BY REGINALDCHEDDARINGTONTHETHIRDESQ"), Longest, 100.0f, Measure), FString(TEXT("KILLED BY REGINAL...")));
	TestEqual(TEXT("a short name untouched"), FitNameIn(TEXT("KILLED BY HOST"), TEXT("Host"), 100.0f, Measure), FString(TEXT("KILLED BY HOST")));
	TestEqual(TEXT("a line without the name untouched"), FitNameIn(TEXT("ROUND 2"), Longest, 100.0f, Measure), FString(TEXT("ROUND 2")));
	TestEqual(TEXT("no name, nothing cut"), FitNameIn(TEXT("ROUND 2"), FString(), 10.0f, Measure), FString(TEXT("ROUND 2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheTopStripIsTheSameAtBothEnds, "CheeseStrike.HUD.TheTopStripIsTheSameAtBothEnds", HUDTests::Flags)
bool FTheTopStripIsTheSameAtBothEnds::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	// A panel at an awkward place, as the scoreboard's is, with its rounding and the strip's thickness at several scales.
	for (const float Scale : {0.75f, 1.0f, 1.37f, 2.0f})
	{
		const FVector2D Min(641.3f * Scale, 216.2f * Scale);
		const FVector2D Max(Min.X + 660.0f * Scale, Min.Y + 500.0f * Scale);
		const TArray<FVector2D> Strip = TopStrip(Min, Max, 12.0f * Scale, 3.0f * Scale);
		TestTrue(TEXT("a strip with an even number of points"), Strip.Num() >= 6 && Strip.Num() % 2 == 0);
		bool bMirrored = true;
		float Lowest = Min.Y;
		for (int32 Index = 0; Index < Strip.Num() / 2; ++Index)
		{
			const FVector2D& Left = Strip[Index];
			const FVector2D& Right = Strip[Strip.Num() - 1 - Index];
			bMirrored &= FMath::IsNearlyEqual(Left.Y, Right.Y, 0.001f) && FMath::IsNearlyEqual(Left.X - Min.X, Max.X - Right.X, 0.01f);
			Lowest = FMath::Max(Lowest, Left.Y);
		}
		TestTrue(TEXT("the right end is the left's mirror"), bMirrored);
		TestTrue(TEXT("and it is as thick as asked"), FMath::IsNearlyEqual(Lowest - Min.Y, 3.0f * Scale, 0.01f));
		bool bInside = true;
		for (const FVector2D& Point : Strip)
		{
			bInside &= InsideRoundedRect(Point, Min, Max, 12.0f * Scale) > -0.05f && Point.Y >= Min.Y - 0.001f;
		}
		TestTrue(TEXT("within the panel's rounding"), bInside);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundedShapesKeepToTheirCorners, "CheeseStrike.HUD.RoundedShapesKeepToTheirCorners", HUDTests::Flags)
bool FRoundedShapesKeepToTheirCorners::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const FVector2D Min(10.0f, 20.0f);
	const FVector2D Max(110.0f, 80.0f);
	const TArray<FVector2D> Outline = RoundedRect(Min, Max, 10.0f, 4);
	TestEqual(TEXT("four corners of five points each"), Outline.Num(), 20);
	TestTrue(TEXT("it starts at the left end of the top left corner's arc"), HUDTests::Near(Outline[0], FVector2D(10.0f, 30.0f)));
	TestTrue(TEXT("and comes round to the top"), HUDTests::Near(Outline[4], FVector2D(20.0f, 20.0f)));
	TestTrue(TEXT("the top right corner"), HUDTests::Near(Outline[5], FVector2D(100.0f, 20.0f)) && HUDTests::Near(Outline[9], FVector2D(110.0f, 30.0f)));
	TestTrue(TEXT("the bottom left corner ends going up the left side"), HUDTests::Near(Outline[19], FVector2D(10.0f, 70.0f)));
	bool bOnTheBox = true;
	float Area = 0.0f;
	for (int32 Index = 0; Index < Outline.Num(); ++Index)
	{
		bOnTheBox &= FMath::Abs(InsideRoundedRect(Outline[Index], Min, Max, 10.0f)) < 0.01f;
		Area += FVector2D::CrossProduct(Outline[Index], Outline[(Index + 1) % Outline.Num()]);
	}
	TestTrue(TEXT("every point on its edge"), bOnTheBox);
	TestTrue(TEXT("clockwise on the screen, y down"), Area > 0.0f);
	TestEqual(TEXT("square corners at no radius"), RoundedRect(Min, Max, 0.0f).Num(), 4);
	const TArray<FVector2D> Pill = RoundedRect(Min, Max, 500.0f, 4);
	TestTrue(TEXT("the radius held to half the shorter side: a pill"), HUDTests::Near(Pill[0], FVector2D(10.0f, 50.0f)) && HUDTests::Near(Pill[4], FVector2D(40.0f, 20.0f)));

	// Inside and outside: the middle, the middle of a side, a square corner (outside the rounding).
	TestTrue(TEXT("the middle is inside"), InsideRoundedRect(FVector2D(60.0f, 50.0f), Min, Max, 10.0f) > 0.0f);
	TestTrue(TEXT("a side's middle is on the edge"), FMath::IsNearlyZero(InsideRoundedRect(FVector2D(60.0f, 20.0f), Min, Max, 10.0f), 0.01f));
	TestTrue(TEXT("the square's corner is outside it"), InsideRoundedRect(Min, Min, Max, 10.0f) < 0.0f);
	TestTrue(TEXT("but on a square one's edge"), FMath::IsNearlyZero(InsideRoundedRect(Min, Min, Max, 0.0f), 0.01f));
	// Held inside: a point beyond the corner comes to the arc, on the line to the corner's middle.
	const FVector2D Held = HoldInsideRoundedRect(FVector2D(0.0f, 10.0f), Min, Max, 10.0f);
	TestTrue(TEXT("held to the rounded corner"), HUDTests::Near(Held, FVector2D(20.0f, 30.0f) + FVector2D(-1.0f, -1.0f).GetSafeNormal() * 10.0f));
	TestTrue(TEXT("held beyond a side, to the side"), HUDTests::Near(HoldInsideRoundedRect(FVector2D(60.0f, 0.0f), Min, Max, 10.0f), FVector2D(60.0f, 20.0f)));
	TestTrue(TEXT("inside, left where it is"), HUDTests::Near(HoldInsideRoundedRect(FVector2D(60.0f, 50.0f), Min, Max, 10.0f), FVector2D(60.0f, 50.0f)));

	// The minimap's pieces are cut to the rounded edge: a square over a corner loses the corner's tip.
	const TArray<FVector2D> Piece = {FVector2D(0.0f, 10.0f), FVector2D(40.0f, 10.0f), FVector2D(40.0f, 50.0f), FVector2D(0.0f, 50.0f)};
	const TArray<FVector2D> Cut = ClipToConvex(Piece, Outline);
	bool bInside = Cut.Num() >= 3;
	for (const FVector2D& Point : Cut)
	{
		bInside &= InsideRoundedRect(Point, Min, Max, 10.0f) > -0.5f;
	}
	TestTrue(TEXT("what is left is inside the rounded edge"), bInside);
	TestTrue(TEXT("and reaches its sides"), Cut.ContainsByPredicate([](const FVector2D& Point) { return HUDTests::Near(Point, FVector2D(40.0f, 20.0f)); }));
	TArray<FVector2D> Reversed = Outline;
	Algo::Reverse(Reversed);
	TestEqual(TEXT("the clip's winding either way"), ClipToConvex(Piece, Reversed).Num(), Cut.Num());
	TestEqual(TEXT("nothing left of a piece outside"), ClipToConvex({FVector2D(200.0f, 200.0f), FVector2D(210.0f, 200.0f), FVector2D(210.0f, 210.0f)}, Outline).Num(), 0);
	TestEqual(TEXT("a piece inside is whole"), ClipToConvex({FVector2D(50.0f, 40.0f), FVector2D(70.0f, 40.0f), FVector2D(60.0f, 60.0f)}, Outline).Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCrosshairSaysWhenAShotWouldDoNothing, "CheeseStrike.HUD.TheCrosshairSaysWhenAShotWouldDoNothing", HUDTests::Flags)
bool FTheCrosshairSaysWhenAShotWouldDoNothing::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("an enemy down out of reach: a shot does nothing"), CrosshairFor(true, true, 1000.0f), ECrosshair::NoEffect);
	TestEqual(TEXT("just past reach"), CrosshairFor(true, true, CheeseStrikeModes::FinishRange + 1.0f), ECrosshair::NoEffect);
	TestEqual(TEXT("within reach a shot finishes them"), CrosshairFor(true, true, CheeseStrikeModes::FinishRange - 1.0f), ECrosshair::Usual);
	TestEqual(TEXT("an enemy standing"), CrosshairFor(true, false, 1000.0f), ECrosshair::Usual);
	TestEqual(TEXT("no last stand in the mode: every hit hurts"), CrosshairFor(false, true, 1000.0f), ECrosshair::Usual);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDeathScreenShowsForEveryDeath, "CheeseStrike.HUD.TheDeathScreenShowsForEveryDeath", HUDTests::Flags)
bool FTheDeathScreenShowsForEveryDeath::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestTrue(TEXT("waiting to come back, it shows"), ShowsDeathScreen(false, 0.5f) && ShowsDeathScreen(false, 30.0f));
	TestEqual(TEXT("whole"), DeathScreenAlpha(false, 30.0f), 1.0f);
	TestTrue(TEXT("out of the round (finished in Execution), it shows too"), ShowsDeathScreen(true, 0.5f) && ShowsDeathScreen(true, OutDeathScreen - 0.1f));
	TestEqual(TEXT("whole at first"), DeathScreenAlpha(true, 1.0f), 1.0f);
	TestTrue(TEXT("fading at its end"), DeathScreenAlpha(true, OutDeathScreen - 0.25f) > 0.0f && DeathScreenAlpha(true, OutDeathScreen - 0.25f) < 1.0f);
	TestFalse(TEXT("then the notice of being out has the screen"), ShowsDeathScreen(true, OutDeathScreen + 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheScoreSummaryShowsYouAndTheLeader, "CheeseStrike.HUD.TheScoreSummaryShowsYouAndTheLeader", HUDTests::Flags)

bool FTheScoreSummaryShowsYouAndTheLeader::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const TArray<CheeseStrikeRules::FStanding> Sorted = CheeseStrikeRules::Sorted({HUDTests::Line(TEXT("Host"), 3, 2, 0), HUDTests::Line(TEXT("Gorgon Zola"), 7, 1, 1), HUDTests::Line(TEXT("Camem Bert"), 5, 4, 2)});
	const FScoreSummary Behind = FreeForAll(Sorted, TEXT("Host"), 20);
	TestEqual(TEXT("two blocks"), Behind.Lines.Num(), 2);
	TestEqual(TEXT("the limit"), Behind.Limit, 20);
	TestEqual(TEXT("in words"), Behind.Goal, FString(TEXT("FIRST TO 20")));
	if (Behind.Lines.Num() == 2)
	{
		TestTrue(TEXT("first, the player's own kills"), Behind.Lines[0].bMine && Behind.Lines[0].Score == 3 && Behind.Lines[0].Caption == TEXT("YOU"));
		TestTrue(TEXT("then the leader's"), !Behind.Lines[1].bMine && Behind.Lines[1].Score == 7 && Behind.Lines[1].Name == TEXT("Gorgon Zola") && Behind.Lines[1].Caption == TEXT("LEADER"));
	}
	const FScoreSummary Ahead = FreeForAll(Sorted, TEXT("Gorgon Zola"), 20);
	if (TestEqual(TEXT("leading, two blocks"), Ahead.Lines.Num(), 2))
	{
		TestTrue(TEXT("the player's"), Ahead.Lines[0].bMine && Ahead.Lines[0].Score == 7);
		TestTrue(TEXT("and the next best's"), Ahead.Lines[1].Score == 5 && Ahead.Lines[1].Name == TEXT("Camem Bert") && Ahead.Lines[1].Caption == TEXT("2ND"));
	}
	TestEqual(TEXT("alone, one block"), FreeForAll({HUDTests::Line(TEXT("Host"), 0, 0, 0)}, TEXT("Host"), 20).Lines.Num(), 1);
	const int32 Scores[CheeseStrikeRules::Teams] = {12, 9};
	const FScoreSummary Sides = Teams(Scores, 1, 30, TEXT("ROUND 2"));
	if (TestEqual(TEXT("a team mode: a block a side"), Sides.Lines.Num(), CheeseStrikeRules::Teams))
	{
		TestTrue(TEXT("The Gouda Guys' score in their colour"), Sides.Lines[0].Score == 12 && Sides.Lines[0].Team == 0 && !Sides.Lines[0].bMine && Sides.Lines[0].Caption == CheeseStrikeRules::TeamNames[0]);
		TestTrue(TEXT("the player's side marked"), Sides.Lines[1].Score == 9 && Sides.Lines[1].Team == 1 && Sides.Lines[1].bMine);
		TestTrue(TEXT("with the limit and the round"), Sides.Limit == 30 && Sides.Goal == TEXT("ROUND 2"));
	}
	const FScoreSummary Watching = FreeForAll(Sorted, TEXT("Somebody Else"), 20);
	if (TestEqual(TEXT("not playing, the leader only"), Watching.Lines.Num(), 1))
	{
		TestTrue(TEXT("as the leader"), Watching.Lines[0].Score == 7 && !Watching.Lines[0].bMine);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameTagsShrinkWithDistance, "CheeseStrike.HUD.NameTagsShrinkWithDistance", HUDTests::Flags)

bool FNameTagsShrinkWithDistance::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	const FTagLook Close = TagLook(500.0f);
	const FTagLook Middle = TagLook(3000.0f);
	const FTagLook Far = TagLook(9000.0f);
	TestTrue(TEXT("close up, full size and solid"), Close.Scale == 1.0f && Close.Alpha == 1.0f);
	TestTrue(TEXT("further, smaller"), Middle.Scale < Close.Scale && Far.Scale < Middle.Scale);
	TestTrue(TEXT("and fainter"), Far.Alpha < Middle.Alpha);
	TestTrue(TEXT("but never gone"), Far.Scale > 0.5f && Far.Alpha > 0.3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDeathScreenMelts, "CheeseStrike.HUD.TheDeathScreenMelts", HUDTests::Flags)

bool FTheDeathScreenMelts::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("at the moment of death, no sheet yet"), SheetDepth(0.0f), 0.0f);
	TestTrue(TEXT("then it comes down"), SheetDepth(0.2f) > 0.0f && SheetDepth(1.0f) >= SheetDepth(0.2f));
	int32 Long = 0;
	float Left = 1.0f;
	float Right = 0.0f;
	for (int32 Index = 0; Index < Drips; ++Index)
	{
		const FDrip Start = Drip(Index, 0.0f);
		TestEqual(TEXT("no drip has run at first"), Start.Length, 0.0f);
		float Last = 0.0f;
		for (float Seconds = 0.0f; Seconds <= 3.0f; Seconds += 0.1f)
		{
			const FDrip Now = Drip(Index, Seconds);
			if (Now.Length < Last)
			{
				AddError(FString::Printf(TEXT("drip %d runs back up at %.1f s"), Index, Seconds));
			}
			Last = Now.Length;
			TestEqual(TEXT("a drip stays where it is across"), Now.X, Start.X);
		}
		TestTrue(FString::Printf(TEXT("drip %d has run by the end of the melt (%.2f)"), Index, Drip(Index, MeltTime + 0.3f).Length), Drip(Index, MeltTime + 0.3f).Length > 0.05f);
		TestTrue(TEXT("and is no wider than a finger"), Start.Width > 0.0f && Start.Width < 0.05f);
		Long += Drip(Index, MeltTime + 0.3f).Length > 0.4f ? 1 : 0;
		Left = FMath::Min(Left, Start.X);
		Right = FMath::Max(Right, Start.X);
	}
	TestTrue(FString::Printf(TEXT("a few run far down the screen (%d)"), Long), Long >= 2 && Long < Drips / 2);
	TestTrue(FString::Printf(TEXT("they hang all across it (%.2f to %.2f)"), Left, Right), Left < 0.05f && Right > 0.95f);
	TestTrue(TEXT("the same drip at the same moment, every frame"), Drip(3, 0.9f).Length == Drip(3, 0.9f).Length);
	TestEqual(TEXT("the letters stand at first"), LetterSag(0, 0.2f), 0.0f);
	TestTrue(TEXT("then sag"), LetterSag(0, 2.0f) > 0.2f && LetterSag(0, 2.0f) <= 1.0f);
	TestTrue(TEXT("not all alike"), LetterSag(1, 2.0f) != LetterSag(2, 2.0f));
	return true;
}

#endif
