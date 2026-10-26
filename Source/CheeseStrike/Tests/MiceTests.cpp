// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The mice: their rules as plain functions (Rules/CheeseStrikeMice.h), what
// the HUD calls them (vermin), and
// mice coming into The Creamery in the editor and eating it (see PlayTesting.h).

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeMice.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeHUDRules.h"

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
	TestEqual(TEXT("three at the least"), MiceFor(30), 3);
	TestEqual(TEXT("one for every twenty-five blocks: Crumb Kitchen's 308"), MiceFor(308), 12);
	TestEqual(TEXT("twenty-four at the most: The Creamery's thousand blocks"), MiceFor(1000), 24);
	TestTrue(TEXT("a match has mice"), HasMice(TEXT("?listen")));
	TestFalse(TEXT("unless told not to"), HasMice(TEXT("?Mice=0")));
	TestEqual(TEXT("two minutes in"), ArrivalFrom(TEXT("?listen")), 120.0f);
	TestEqual(TEXT("or when the URL says"), ArrivalFrom(TEXT("?MiceAt=5")), 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheVerminCountdownWaits, "CheeseStrike.Mice.TheCountdownWaitsHalfAMinute", MiceTests::Flags)

bool FTheVerminCountdownWaits::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestEqual(TEXT("the mice come two minutes in"), ArriveAfter, 120.0f);
	TestEqual(TEXT("the countdown shows a minute and a half in"), CountdownFrom(ArriveAfter), 90.0f);
	TestEqual(TEXT("and runs for half a minute"), Warning, 30.0f);
	TestEqual(TEXT("mice coming sooner (?MiceAt=20) have it from the start"), CountdownFrom(ArrivalFrom(TEXT("?MiceAt=20"))), 0.0f);
	// Second by second from the end of the start's count, as the HUD reads the game state.
	const float Arrival = ArriveAfter;
	int32 FirstShown = -1;
	FString FirstWords;
	for (int32 Second = 0; Second <= 120 && FirstShown < 0; ++Second)
	{
		const FString Words = CheeseStrikeHUD::VerminWords(Arrival - Second);
		if (!Words.IsEmpty())
		{
			FirstShown = Second;
			FirstWords = Words;
		}
	}
	TestEqual(TEXT("in a match, the first second it shows"), FirstShown, 90);
	TestEqual(TEXT("reading 0:30"), FirstWords, FString(TEXT("VERMIN INVASION IN 0:30")));
	TestEqual(TEXT("a moment before, nothing"), CheeseStrikeHUD::VerminWords(Arrival - (CountdownFrom(Arrival) - 0.1f)), FString());
	TestEqual(TEXT("a moment after, still 0:30"), CheeseStrikeHUD::VerminWords(Arrival - (CountdownFrom(Arrival) + 0.2f)), FString(TEXT("VERMIN INVASION IN 0:30")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMiceLeaveOverTheEdge, "CheeseStrike.Mice.LeaveOverTheEdge", MiceTests::Flags)

bool FTheMiceLeaveOverTheEdge::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestFalse(TEXT("they stay while there is cheese"), ShouldLeave(1));
	TestTrue(TEXT("and leave once it is all eaten"), ShouldLeave(0));
	// Inside, 5 m in from a wall 3 m high, its way out to the west.
	const FVector From(500.0f, 0.0f, 0.0f);
	const FVector Top(0.0f, 0.0f, 300.0f);
	const FVector Out(-1.0f, 0.0f, 0.0f);
	TestTrue(TEXT("setting off from where it stands"), OverTheEdge(From, Top, Out, 0.0f).GetValue().Equals(From, 1.0));
	const FVector Halfway = OverTheEdge(From, Top, Out, 0.5f * ClimbOutTime).GetValue();
	TestTrue(FString::Printf(TEXT("leaping up, over the wall's top halfway (%.0f cm)"), Halfway.Z), Halfway.Z > Top.Z);
	TestTrue(TEXT("on the wall's top as it climbs out"), OverTheEdge(From, Top, Out, ClimbOutTime).GetValue().Equals(Top, 1.0));
	const FVector Last = OverTheEdge(From, Top, Out, ClimbOutTime + DropOutTime - 0.01f).GetValue();
	TestTrue(FString::Printf(TEXT("then outside the wall (%.0f cm out)"), Top.X - Last.X), Top.X - Last.X > 0.9f * DropOutward);
	TestTrue(FString::Printf(TEXT("and below its top, out of sight (%.0f cm)"), Top.Z - Last.Z), Top.Z - Last.Z > 0.9f * DropBelow);
	// Never a jump in where it is: it moves no more than a leap's speed a frame.
	FVector Was = From;
	float Fastest = 0.0f;
	for (float Seconds = 1.0f / 60.0f; Seconds < ClimbOutTime + DropOutTime; Seconds += 1.0f / 60.0f)
	{
		const FVector Is = OverTheEdge(From, Top, Out, Seconds).GetValue();
		Fastest = FMath::Max(Fastest, static_cast<float>(FVector::Dist(Is, Was)) * 60.0f);
		Was = Is;
	}
	TestTrue(FString::Printf(TEXT("smoothly, no faster than 20 m/s (%.0f cm/s)"), Fastest), Fastest < 2000.0f);
	TestFalse(TEXT("and then it is gone"), OverTheEdge(From, Top, Out, ClimbOutTime + DropOutTime + 0.01f).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheVerminAreAnnounced, "CheeseStrike.HUD.TheVerminAreAnnounced", MiceTests::Flags)

bool FTheVerminAreAnnounced::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("nothing at the start, a minute before they come"), VerminWords(60.0f), FString());
	TestEqual(TEXT("nor until half a minute is left"), VerminWords(30.1f), FString());
	TestEqual(TEXT("then counting down from half a minute"), VerminWords(30.0f), FString(TEXT("VERMIN INVASION IN 0:30")));
	TestEqual(TEXT("in minutes and seconds"), VerminWords(28.4f), FString(TEXT("VERMIN INVASION IN 0:29")));
	TestEqual(TEXT("to the last second"), VerminWords(0.2f), FString(TEXT("VERMIN INVASION IN 0:01")));
	TestEqual(TEXT("then they have come"), VerminWords(-1.0f), FString(TEXT("THE VERMIN HAVE COME")));
	TestEqual(TEXT("and then nothing"), VerminWords(-VerminCameFor - 0.1f), FString());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceSpreadOut, "CheeseStrike.Mice.SpreadOut", MiceTests::Flags)

bool FMiceSpreadOut::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	// Another mouse at the origin, going for cheese 10 m east of it.
	const TArray<FVector2D> Others = {FVector2D(0.0f, 0.0f), FVector2D(1000.0f, 0.0f)};
	TestEqual(TEXT("nobody about: no cost"), CrowdCost(FVector2D(0.0f, 0.0f), {}), 0.0f);
	TestEqual(TEXT("right by another mouse: all of it"), CrowdCost(FVector2D(0.0f, -5000.0f), {FVector2D(0.0f, -5000.0f)}), Crowded);
	TestEqual(TEXT("far from them: none"), CrowdCost(FVector2D(0.0f, 5000.0f), Others), 0.0f);
	TestTrue(TEXT("less the further off"), CrowdCost(FVector2D(-500.0f, 0.0f), Others) > CrowdCost(FVector2D(-1500.0f, 0.0f), Others));
	TestTrue(TEXT("by its cheese as well as by it"), CrowdCost(FVector2D(1000.0f, 500.0f), Others) > CrowdCost(FVector2D(1000.0f, 500.0f), {Others[0]}));
	// A mouse 5 m west of the other: cheese beside it, between it and the other mouse, or 9 m off the other way.
	const FVector2D Mouse(-500.0f, 0.0f);
	auto Cost = [&](const FVector2D& Block) { return CheeseCost(FVector2D::Distance(Block, Mouse), 0.0f, false) + CrowdCost(Block, Others); };
	TestTrue(TEXT("it goes for cheese away from the others rather than the cheese between them"), Cost(FVector2D(-1400.0f, 0.0f)) < Cost(FVector2D(0.0f, 300.0f)));
	TestTrue(TEXT("but not across the map to get away"), Cost(FVector2D(-500.0f, 200.0f)) < Cost(FVector2D(-6000.0f, 0.0f)));
	TestTrue(TEXT("and keeps to the cheese it goes for, rather than turn for the same cost"), KeepGoing > 0.0f && KeepGoing < Crowded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceEatBlocksInStages, "CheeseStrike.Mice.EatBlocksInStages", MiceTests::Flags)

bool FMiceEatBlocksInStages::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	const float Full = CheeseStrikeRules::CheeseBlockHealth;
	TestEqual(TEXT("a block takes four bites"), FMath::CeilToInt(Full / Bite), 4);
	TestEqual(TEXT("four seconds of one mouse"), FMath::CeilToInt(Full / Bite) * BiteEvery, 4.0f);
	TestEqual(TEXT("whole"), Stage(Full, Full), 0);
	TestEqual(TEXT("bitten"), Stage(Full - Bite, Full), 1);
	TestEqual(TEXT("a third gone"), Stage(Full * 0.6f, Full), 2);
	TestEqual(TEXT("two thirds gone"), Stage(Full * 0.3f, Full), 3);
	TestEqual(TEXT("nearly gone"), Stage(1.0f, Full), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiceEatAtAPace, "CheeseStrike.Mice.EatAtAPace", MiceTests::Flags)

bool FMiceEatAtAPace::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	TestEqual(TEXT("all the cheese planned to be left as they come"), PlannedLeft(0.0f), 1.0f);
	TestEqual(TEXT("none some way before five minutes, for the walks to the last blocks"), PlannedLeft(PlannedWithin), 0.0f);
	TestTrue(TEXT("which is before six minutes"), PlannedWithin < EatenWithin && EatenWithin == 328.0f);
	TestEqual(TEXT("on the plan, a bite is a bite"), BiteFor(0.5f, PlannedWithin / 2.0f), Bite);
	TestTrue(TEXT("behind it, bigger"), BiteFor(0.8f, PlannedWithin / 2.0f) > Bite);
	TestTrue(TEXT("ahead of it, smaller"), BiteFor(0.2f, PlannedWithin / 2.0f) < Bite);
	TestEqual(TEXT("but no smaller than a quarter"), BiteFor(0.01f, PlannedWithin / 2.0f), Bite * MinPace);
	TestEqual(TEXT("nor bigger than four"), BiteFor(1.0f, PlannedWithin), Bite * MaxPace);
	TestTrue(TEXT("which is a whole block at a bite"), FMath::IsNearlyEqual(Bite * MaxPace, CheeseStrikeRules::CheeseBlockHealth));

	// Maps of every size, as the mice play them: so many blocks, so many mice, each block a walk away (seconds,
	// half as long again by the end, the cheese sparse; a mouse mostly eats the block beside the one it ate) and
	// then eaten bite by bite. Every one is eaten within a minute and a
	// half, and steadily: not most of it in the first third of that.
	struct FMap { const TCHAR* Name; int32 Blocks; float Walk; };
	const FMap Maps[] = {{TEXT("a small map"), 120, 2.0f}, {TEXT("Crumb Kitchen"), 308, 1.5f}, {TEXT("Gruyère Grotto"), 352, 1.5f}, {TEXT("The Labyrinth"), 680, 1.0f}, {TEXT("Rind Market"), 1092, 0.7f}};
	for (const FMap& Map : Maps)
	{
		const int32 Mice = MiceFor(Map.Blocks);
		const float Full = Map.Blocks * CheeseStrikeRules::CheeseBlockHealth;
		TArray<float> Block;
		TArray<float> WalkLeft;
		Block.Init(0.0f, Mice);
		WalkLeft.Init(0.0f, Mice);
		float Health = Full;
		int32 Whole = Map.Blocks;
		float LeftAfterAThird = 1.0f;
		float GoneAt = -1.0f;
		for (float Time = 0.0f; Time < 600.0f && GoneAt < 0.0f; Time += BiteEvery)
		{
			for (int32 Mouse = 0; Mouse < Mice; ++Mouse)
			{
				if (Block[Mouse] <= 0.0f)
				{
					if (Whole == 0)
					{
						continue;
					}
					--Whole;
					Block[Mouse] = CheeseStrikeRules::CheeseBlockHealth;
					// The sparser the cheese left, the longer the walk to the next.
					WalkLeft[Mouse] = Map.Walk * (1.5f - 0.5f * Health / Full);
				}
				if (WalkLeft[Mouse] > 0.0f)
				{
					WalkLeft[Mouse] -= BiteEvery;
					continue;
				}
				const float Taken = FMath::Min(Block[Mouse], BiteFor(Health / Full, Time));
				Block[Mouse] -= Taken;
				Health -= Taken;
			}
			if (Time <= EatenWithin / 3.0f)
			{
				LeftAfterAThird = Health / Full;
			}
			if (Health <= 0.5f)
			{
				GoneAt = Time;
			}
		}
		AddInfo(FString::Printf(TEXT("%s, %d mice: all eaten after %.0f s, %.0f%% left after %.0f s"), Map.Name, Mice, GoneAt, 100.0f * LeftAfterAThird, EatenWithin / 3.0f));
		TestTrue(FString::Printf(TEXT("%s is eaten within six minutes (%.0f s)"), Map.Name, GoneAt), GoneAt > 0.0f && GoneAt <= 360.0f);
		TestTrue(FString::Printf(TEXT("%s is not eaten much before three minutes (%.0f s)"), Map.Name, GoneAt), GoneAt >= 180.0f);
		TestTrue(FString::Printf(TEXT("%s is eaten steadily: a good part of it left after a third of that (%.0f%%)"), Map.Name, 100.0f * LeftAfterAThird), LeftAfterAThird > 0.4f && LeftAfterAThird < 0.9f);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAScaredMouseEatsElsewhere, "CheeseStrike.Mice.AScaredMouseEatsElsewhere", MiceTests::Flags)

bool FAScaredMouseEatsElsewhere::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMice;
	// Scared at the west end of a map whose middle is at the origin, the mouse still there.
	const FVector2D Middle(0.0, 0.0);
	const FVector2D Scared(-3000.0, 0.0);
	const FVector2D Near(-2500.0, 500.0);
	const FVector2D Far(2500.0, 0.0);
	const float NearCost = CheeseCost(FVector2D::Distance(Near, Scared), 0.0f, false) + ScareCost(Near, Scared, Middle);
	const float FarCost = CheeseCost(FVector2D::Distance(Far, Scared), 0.0f, false) + ScareCost(Far, Scared, Middle);
	TestTrue(FString::Printf(TEXT("a block across the map (%.0f) beats one by the scare (%.0f)"), FarCost, NearCost), FarCost < NearCost);
	TestEqual(TEXT("the far side costs nothing more"), ScareCost(Far, Scared, Middle), 0.0f);
	TestTrue(TEXT("on the scare's side, the nearer the dearer"), ScareCost(FVector2D(-2000.0, 0.0), Scared, Middle) < ScareCost(FVector2D(-2900.0, 0.0), Scared, Middle));
	TestTrue(TEXT("it avoids it just after the scare"), StillAvoids(1.0f, 0));
	TestFalse(TEXT("but not after a minute"), StillAvoids(AvoidFor + 1.0f, 0));
	TestFalse(TEXT("nor once it has eaten a few blocks elsewhere"), StillAvoids(10.0f, AvoidUntilEaten));
	TestFalse(TEXT("nor if never scared"), StillAvoids(-1.0f, 0));
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
	WaitUntil(TEXT("twenty-four mice over the edge"), 15.0f, []() { return All<ACheeseStrikeMouse>().Num() == CheeseStrikeMice::MaxMice; });
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMiceLeaveWhenTheCheeseIsGone, "CheeseStrike.Play.TheMiceLeaveWhenTheCheeseIsGone", MiceTests::Flags)

bool FTheMiceLeaveWhenTheCheeseIsGone::RunTest(const FString& Parameters)
{
	using namespace PlayTesting;
	using namespace MiceTests;
	// Each mouse, and whether it was seen going home and going over the edge.
	static TMap<TWeakObjectPtr<ACheeseStrikeMouse>, uint8> Seen;
	static int32 Came = 0;
	Seen.Reset();
	Came = 0;
	FSession Session;
	Session.Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery");
	Session.Options = TEXT("?Mice=1?MiceAt=2");
	StartPlaying(Session);
	WaitUntil(TEXT("twenty-four mice over the edge"), 15.0f, []() { return All<ACheeseStrikeMouse>().Num() == CheeseStrikeMice::MaxMice; });
	WaitUntil(TEXT("the mice in and eating"), 15.0f, []()
	{
		for (ACheeseStrikeMouse* Mouse : All<ACheeseStrikeMouse>())
		{
			if (Mouse->GetDoing() == EMouseDoing::Entering)
			{
				return false;
			}
		}
		return true;
	});
	Wait(2.0f);
	Then([this]()
	{
		for (ACheeseStrikeMouse* Mouse : All<ACheeseStrikeMouse>())
		{
			TestFalse(FString::Printf(TEXT("%s stays while there is cheese"), *Mouse->GetName()), Mouse->IsLeaving());
			Seen.Add(Mouse, 0);
		}
		Came = Seen.Num();
		// Everything they may eat, eaten at once.
		int32 Eaten = 0;
		for (ACheeseStrikeCheese* Cheese : All<ACheeseStrikeCheese>())
		{
			if (Cheese->IsEdible())
			{
				Cheese->Bite(CheeseStrikeRules::CheeseBlockHealth);
				++Eaten;
			}
		}
		AddInfo(FString::Printf(TEXT("%d mice; %d blocks eaten at once"), Came, Eaten));
	});
	// Within a couple of seconds every mouse sets off; then each goes over the edge, and they are all gone.
	WaitUntil(TEXT("every mouse going home"), 5.0f, []()
	{
		bool bAll = true;
		for (ACheeseStrikeMouse* Mouse : All<ACheeseStrikeMouse>())
		{
			bAll &= Mouse->IsLeaving();
		}
		return bAll;
	});
	WaitUntil(TEXT("the mice all gone"), 45.0f, []()
	{
		for (ACheeseStrikeMouse* Mouse : All<ACheeseStrikeMouse>())
		{
			if (uint8* Was = Seen.Find(Mouse))
			{
				*Was |= Mouse->IsLeaving() ? 1 : 0;
				*Was |= Mouse->GetDoing() == EMouseDoing::Exiting ? 2 : 0;
			}
		}
		return All<ACheeseStrikeMouse>().Num() == 0;
	});
	Then([this]()
	{
		int32 Home = 0;
		int32 OverTheEdge = 0;
		for (const TPair<TWeakObjectPtr<ACheeseStrikeMouse>, uint8>& Each : Seen)
		{
			Home += (Each.Value & 1) != 0 ? 1 : 0;
			OverTheEdge += (Each.Value & 2) != 0 ? 1 : 0;
		}
		TestEqual(TEXT("no mouse is left"), All<ACheeseStrikeMouse>().Num(), 0);
		TestEqual(TEXT("every mouse went home"), Home, Came);
		TestEqual(TEXT("every mouse went over the edge, none vanishing where it stood"), OverTheEdge, Came);
	});
	Wait(5.0f);
	Then([this]() { TestEqual(TEXT("and none comes back"), All<ACheeseStrikeMouse>().Num(), 0); });
	StopPlaying();
	return true;
}

#endif
