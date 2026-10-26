// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The modes and the streaks, without a world: what each mode is and how it
// is won, who takes a round of Elimination, who may hurt whom, where a side
// comes in, when streaks are earned, and where a Cheese Strike's wheels
// land and what they do.

#include "Misc/AutomationTest.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeDowned.h"
#include "Rules/CheeseStrikeExecution.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "UI/CheeseStrikeHUDRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ModeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	CheeseStrikeModes::FSide Side(int32 Members, int32 Alive, float Health)
	{
		CheeseStrikeModes::FSide Each;
		Each.Members = Members;
		Each.Alive = Alive;
		Each.Health = Health;
		return Each;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheModesAreNamedAndPicked, "CheeseStrike.Rules.TheModesAreNamedAndPicked", ModeTests::Flags)

bool FTheModesAreNamedAndPicked::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	TestEqual(TEXT("seven modes"), NumModes, 7);
	TestEqual(TEXT("free-for-all first"), FString(Info(EMode::FreeForAll).Name), FString(TEXT("Free-for-all")));
	TestFalse(TEXT("on no teams"), Info(EMode::FreeForAll).bTeams);
	TestTrue(TEXT("won by a player's kills"), Info(EMode::FreeForAll).WinBy == EWinBy::PlayerKills);
	TestTrue(TEXT("Deathmatch is two teams"), Info(EMode::Deathmatch).bTeams);
	TestTrue(TEXT("won by a team's kills"), Info(EMode::Deathmatch).WinBy == EWinBy::TeamKills);
	TestTrue(TEXT("Elimination is two teams"), Info(EMode::Elimination).bTeams);
	TestTrue(TEXT("won by rounds"), Info(EMode::Elimination).WinBy == EWinBy::Rounds);
	TestFalse(TEXT("with no coming back within one"), Info(EMode::Elimination).bRespawns);
	TestTrue(TEXT("the others have the killed come back"), Info(EMode::FreeForAll).bRespawns && Info(EMode::Deathmatch).bRespawns && Info(EMode::GunGame).bRespawns);
	TestTrue(TEXT("Execution is rounds of two teams"), Info(EMode::Execution).bTeams && Info(EMode::Execution).WinBy == EWinBy::Rounds && !Info(EMode::Execution).bRespawns);
	TestTrue(TEXT("with a last stand, which only it has"), Info(EMode::Execution).bLastStand && !Info(EMode::Elimination).bLastStand && !Info(EMode::FreeForAll).bLastStand);
	TestTrue(TEXT("Gun Game is everyone for themself, up the ladder"), !Info(EMode::GunGame).bTeams && Info(EMode::GunGame).WinBy == EWinBy::Ladder);
	TestTrue(TEXT("without streaks, which the other modes have"), !Info(EMode::GunGame).bStreaks && Info(EMode::FreeForAll).bStreaks && Info(EMode::Execution).bStreaks);
	for (int32 Index = 0; Index < NumModes; ++Index)
	{
		TestTrue(FString::Printf(TEXT("mode %d is where its number says"), Index), Modes[Index].Mode == ModeAt(Index));
	}

	TestTrue(TEXT("a URL without a mode is free-for-all"), ModeFrom(TEXT("?listen?KillLimit=5")) == EMode::FreeForAll);
	TestTrue(TEXT("one names it by its key"), ModeFrom(TEXT("?Mode=Deathmatch")) == EMode::Deathmatch);
	TestTrue(TEXT("in any case"), ModeFrom(TEXT("?mode=elimination")) == EMode::Elimination);
	TestTrue(TEXT("or by its number"), ModeFrom(TEXT("?Mode=2")) == EMode::Elimination);
	TestTrue(TEXT("Execution by its key"), ModeFrom(TEXT("?Mode=Execution")) == EMode::Execution);
	TestTrue(TEXT("and Gun Game"), ModeFrom(TEXT("?Mode=GunGame")) == EMode::GunGame);
	TestTrue(TEXT("and nonsense is free-for-all"), ModeFrom(TEXT("?Mode=Hopscotch")) == EMode::FreeForAll && ModeFrom(TEXT("?Mode=9")) == EMode::FreeForAll);

	TestEqual(TEXT("free-for-all to the usual 20"), KillLimitFor(EMode::FreeForAll, TEXT("")), CheeseStrikeRules::KillLimit);
	TestEqual(TEXT("teams to more, there being more of them"), KillLimitFor(EMode::Deathmatch, TEXT("")), 50);
	TestEqual(TEXT("unless the host says"), KillLimitFor(EMode::Deathmatch, TEXT("?KillLimit=10")), 10);
	TestEqual(TEXT("first to four rounds, as the design has it"), RoundLimitFrom(TEXT("")), 4);
	TestEqual(TEXT("or as many as the host says"), RoundLimitFrom(TEXT("?Rounds=2")), 2);
	TestEqual(TEXT("but at least one"), RoundLimitFrom(TEXT("?Rounds=0")), 1);
	TestEqual(TEXT("three minutes a round"), RoundTimeFrom(TEXT("")), 180.0f);
	TestEqual(TEXT("the lobby offers one to five minutes, three among them"), static_cast<int32>(UE_ARRAY_COUNT(RoundTimeChoices)), 5);
	TestEqual(TEXT("and the default is one of them"), RoundTimeChoices[2], RoundTime);
	TestEqual(TEXT("or the host's"), RoundTimeFrom(TEXT("?RoundTime=90")), 90.0f);
	TestEqual(TEXT("within reason"), RoundTimeFrom(TEXT("?RoundTime=5")), 30.0f);

	TestEqual(TEXT("the scoreboard's title"), Title(EMode::Deathmatch), FString(TEXT("DEATHMATCH")));
	TestEqual(TEXT("what free-for-all plays to"), Goal(EMode::FreeForAll, 20, 4), FString(TEXT("first to 20 kills")));
	TestEqual(TEXT("what Deathmatch plays to"), Goal(EMode::Deathmatch, 50, 4), FString(TEXT("first team to 50 kills")));
	TestEqual(TEXT("what Elimination plays to"), Goal(EMode::Elimination, 20, 4), FString(TEXT("first to win 4 rounds")));
	TestEqual(TEXT("and Execution"), Goal(EMode::Execution, 20, 3), FString(TEXT("first to win 3 rounds")));
	TestEqual(TEXT("what Gun Game plays to"), Goal(EMode::GunGame, 20, 4), FString::Printf(TEXT("first through all %d weapons"), CheeseStrikeArsenal::GunGameLadder().Num()));
	TestEqual(TEXT("a mode without a kill limit of its own plays to the usual if asked"), KillLimitFor(EMode::GunGame, TEXT("")), CheeseStrikeRules::KillLimit);
	TestTrue(TEXT("the teams' colours differ"), !TeamColour(0).Equals(TeamColour(1)));

	// The match's state says which mode is on.
	const ACheeseStrikeGameState* State = GetDefault<ACheeseStrikeGameState>();
	TestTrue(TEXT("a match is free-for-all to begin with"), State->GetMode() == EMode::FreeForAll && !State->HasTeams());
	TestEqual(TEXT("and its teams have no score"), State->GetTeamScore(0) + State->GetTeamScore(1), 0);
	TestTrue(TEXT("the game mode too"), GetDefault<ACheeseStrikeGameMode>()->GetMode() == EMode::FreeForAll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATeamWinsAtTheKillLimit, "CheeseStrike.Rules.ATeamWinsAtTheKillLimit", ModeTests::Flags)

bool FATeamWinsAtTheKillLimit::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	const int32 Level[] = {9, 9};
	const int32 Second[] = {9, 10};
	const int32 Past[] = {11, 3};
	TestEqual(TEXT("nobody before the limit"), TeamAtLimit(Level, 10), NoTeam);
	TestEqual(TEXT("the team that reaches it"), TeamAtLimit(Second, 10), 1);
	TestEqual(TEXT("or passes it"), TeamAtLimit(Past, 10), 0);
	TestEqual(TEXT("never without a limit"), TeamAtLimit(Past, 0), NoTeam);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FARoundGoesToTheSideLeftStanding, "CheeseStrike.Rules.ARoundGoesToTheSideLeftStanding", ModeTests::Flags)

bool FARoundGoesToTheSideLeftStanding::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	using ModeTests::Side;
	int32 Winner = 7;
	TestFalse(TEXT("both sides standing: on"), RoundOver(Side(4, 2, 150), Side(4, 1, 80), false, Winner));
	TestTrue(TEXT("one side gone: over"), RoundOver(Side(4, 0, 0), Side(4, 1, 80), false, Winner));
	TestEqual(TEXT("to the other"), Winner, 1);
	TestTrue(TEXT("the other way about"), RoundOver(Side(4, 3, 300), Side(4, 0, 0), false, Winner) && Winner == 0);
	TestTrue(TEXT("both gone at once"), RoundOver(Side(2, 0, 0), Side(2, 0, 0), false, Winner));
	TestEqual(TEXT("is a draw"), Winner, NoTeam);
	TestFalse(TEXT("a side with nobody on it loses nothing: there is nobody to beat"), RoundOver(Side(1, 1, 100), Side(0, 0, 0), false, Winner));
	TestTrue(TEXT("time up with both sides in: a draw, whatever stands"), RoundOver(Side(4, 2, 100), Side(4, 1, 100), true, Winner) && Winner == NoTeam);
	TestTrue(TEXT("as many standing, health or not: a draw"), RoundOver(Side(4, 2, 90), Side(4, 2, 160), true, Winner) && Winner == NoTeam);
	TestTrue(TEXT("time up with one side with nobody in: the other takes it"), RoundOver(Side(4, 2, 100), Side(4, 0, 0), true, Winner) && Winner == 0);
	TestTrue(TEXT("and a lone player, at the time, takes it"), RoundOver(Side(1, 1, 100), Side(0, 0, 0), true, Winner) && Winner == 0);
	TestTrue(TEXT("rounds are short and the break shorter"), RoundTime >= 60.0f && RoundBreak > 0.0f && RoundBreak <= 10.0f);
	// Execution: someone down is in until executed, even with everyone of their side down: nobody is out for being down.
	FSide OneDown = Side(3, 2, 60);
	OneDown.Down = 1;
	TestFalse(TEXT("one down, one up: still in"), OneDown.IsOut() || RoundOver(OneDown, Side(3, 1, 100), false, Winner));
	FSide AllDown = Side(3, 2, 0);
	AllDown.Down = 2;
	TestFalse(TEXT("everyone left down: still in"), AllDown.IsOut());
	TestFalse(TEXT("and the round goes on"), RoundOver(Side(3, 1, 40), AllDown, false, Winner));
	FSide LoneDown = Side(1, 1, 0);
	LoneDown.Down = 1;
	TestFalse(TEXT("a side of one, down: still in"), RoundOver(LoneDown, Side(1, 1, 100), false, Winner));
	TestFalse(TEXT("both sides all down at once: on, until someone is executed or gets up"), RoundOver(LoneDown, AllDown, false, Winner));
	TestTrue(TEXT("once the last of them is executed, out"), Side(1, 0, 0).IsOut() && RoundOver(Side(1, 0, 0), Side(1, 1, 100), false, Winner) && Winner == 1);
	// Time up with someone down: no overtime, someone down is still in, so the round is over as it stands.
	TestTrue(TEXT("time up with someone down: over, a draw while both sides have someone in"), RoundOver(Side(3, 1, 40), AllDown, true, Winner) && Winner == NoTeam);
	TestTrue(TEXT("a lone player down, getting up by themself, is still in: a draw"), RoundOver(LoneDown, Side(1, 1, 100), true, Winner) && Winner == NoTeam);
	TestTrue(TEXT("time up with a side with nobody in: the other takes it"), RoundOver(Side(3, 0, 0), Side(3, 1, 100), true, Winner) && Winner == 1);
	FSide OtherTwo = Side(3, 2, 30);
	TestTrue(TEXT("as many standing, health or not: a draw"), RoundOver(Side(3, 2, 60), OtherTwo, true, Winner) && Winner == NoTeam);
	// The time is told as it runs out: at 30 s left and at 10, and no more.
	TestEqual(TEXT("30 s left is told"), RoundWarning(30.2f), FString(TEXT("30 seconds left in the round")));
	TestEqual(TEXT("and 10"), RoundWarning(9.8f), FString(TEXT("10 seconds left in the round")));
	TestEqual(TEXT("not between"), RoundWarning(20.0f), FString());
	TestEqual(TEXT("the first warning 90 s into a two-minute round"), NextRoundWarning(120.0f, 0.0f), 90.0f);
	TestEqual(TEXT("counted after the count before it"), NextRoundWarning(123.0f, 3.0f), 93.0f);
	TestEqual(TEXT("then 20 s on"), NextRoundWarning(30.0f, 0.0f), 20.0f);
	TestEqual(TEXT("and none after the last"), NextRoundWarning(10.0f, 0.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFriendlyFireIsOffInTeams, "CheeseStrike.Rules.FriendlyFireIsOffInTeams", ModeTests::Flags)

bool FFriendlyFireIsOffInTeams::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	TestTrue(TEXT("in free-for-all anyone hurts anyone"), CanHurt(false, 0, 0, false, false));
	TestFalse(TEXT("but never themself"), CanHurt(false, 0, 0, true, false));
	TestTrue(TEXT("in a team mode, an enemy"), CanHurt(true, 0, 1, false, false));
	TestFalse(TEXT("but not a teammate"), CanHurt(true, 1, 1, false, false));
	TestFalse(TEXT("and nobody between rounds"), CanHurt(true, 0, 1, false, true));
	TestTrue(TEXT("enemies in free-for-all: everyone else"), AreEnemies(false, 0, 0, false));
	TestFalse(TEXT("but not oneself"), AreEnemies(false, 0, 1, true));
	TestTrue(TEXT("in teams: the other team"), AreEnemies(true, 0, 1, false));
	TestFalse(TEXT("not one's own"), AreEnemies(true, 1, 1, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesComeInAtTheirOwnStarts, "CheeseStrike.Rules.TheSidesComeInAtTheirOwnStarts", ModeTests::Flags)

bool FTheSidesComeInAtTheirOwnStarts::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	TestEqual(TEXT("the Creamery's marks for the first side"), FString(TeamStartTags[0]), FString(TEXT("Gouda")));
	TestEqual(TEXT("and the second"), FString(TeamStartTags[1]), FString(TEXT("Mozza")));
	// Unmarked starts in a row, mixed up: each side gets an end.
	const TArray<FVector> Row = {FVector(0, 0, 0), FVector(3000, 0, 0), FVector(500, 100, 0), FVector(2500, -100, 0), FVector(1000, 0, 0), FVector(2000, 0, 0)};
	const TArray<int32> Sides = SidesOf(Row);
	if (TestEqual(TEXT("every start has a side"), Sides.Num(), Row.Num()))
	{
		TestTrue(TEXT("one end is one side's"), Sides[0] == Sides[2] && Sides[2] == Sides[4]);
		TestTrue(TEXT("the other end the other's"), Sides[1] == Sides[3] && Sides[3] == Sides[5]);
		TestTrue(TEXT("and they differ"), Sides[0] != Sides[1]);
		TestEqual(TEXT("half each"), Sides.FilterByPredicate([](int32 Each) { return Each == 0; }).Num(), 3);
	}
	TestEqual(TEXT("one start alone is the first side's"), SidesOf(TArray<FVector>{FVector::ZeroVector})[0], 0);

	const TArray<FVector> Spots = {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0)};
	TestEqual(TEXT("furthest from the enemy"), ChooseStart(Spots, TArray<FVector>{FVector(-100, 0, 0)}, TArray<FVector>(), 120.0f), 2);
	TestEqual(TEXT("but not where someone already stands"), ChooseStart(Spots, TArray<FVector>{FVector(-100, 0, 0)}, TArray<FVector>{FVector(2050, 0, 0)}, 120.0f), 1);
	TestEqual(TEXT("with nobody about, the first"), ChooseStart(Spots, TArray<FVector>(), TArray<FVector>(), 120.0f), 0);
	TestEqual(TEXT("and the next free one as teammates come in"), ChooseStart(Spots, TArray<FVector>(), TArray<FVector>{FVector(0, 0, 0)}, 120.0f), 1);
	TestEqual(TEXT("every start taken: furthest from the enemy all the same"), ChooseStart(Spots, TArray<FVector>{FVector(2100, 0, 0)}, Spots, 120.0f), 0);
	TestEqual(TEXT("no starts, none"), ChooseStart(TArray<FVector>(), TArray<FVector>(), TArray<FVector>(), 120.0f), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStreaksComeAtThreeFiveAndTen, "CheeseStrike.Rules.StreaksComeAtThreeFiveAndTen", ModeTests::Flags)

bool FStreaksComeAtThreeFiveAndTen::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStreaks;
	TestTrue(TEXT("three kills: the Unmanned Aerial Cheese"), EarnedAt(3) == EStreak::AerialCheese);
	TestTrue(TEXT("five: the Cheese Strike"), EarnedAt(5) == EStreak::CheeseStrike);
	TestTrue(TEXT("ten: the Gouda Overload"), EarnedAt(10) == EStreak::GoudaOverload);
	TestTrue(TEXT("nothing in between"), EarnedAt(1) == EStreak::None && EarnedAt(4) == EStreak::None && EarnedAt(11) == EStreak::None);
	TestTrue(TEXT("on the way to the first"), NextAfter(0) == EStreak::AerialCheese && NextAfter(2) == EStreak::AerialCheese);
	TestTrue(TEXT("then the next"), NextAfter(3) == EStreak::CheeseStrike && NextAfter(9) == EStreak::GoudaOverload);
	TestTrue(TEXT("and none past the last"), NextAfter(10) == EStreak::None);
	TestEqual(TEXT("as everyone is told"), CalledIn(TEXT("Camem Bert"), EStreak::CheeseStrike), FString(TEXT("Camem Bert called in a Cheese Strike")));
	TestEqual(TEXT("with its article"), CalledIn(TEXT("Ash"), EStreak::AerialCheese), FString(TEXT("Ash called in an Unmanned Aerial Cheese")));
	TestEqual(TEXT("each"), CalledIn(TEXT("Ash"), EStreak::GoudaOverload), FString(TEXT("Ash called in the Gouda Overload")));

	FRun Run;
	for (int32 Kill = 1; Kill <= 4; ++Kill)
	{
		Run.AddKill(true);
	}
	TestEqual(TEXT("four in a row"), Run.Kills, 4);
	TestTrue(TEXT("with the drone ready"), Run.Next() == EStreak::AerialCheese && Run.Ready.Num() == 1);
	TestTrue(TEXT("a streak's kill doesn't run it on"), Run.AddKill(false) == EStreak::None && Run.Kills == 4);
	TestTrue(TEXT("the fifth of one's own earns the strike"), Run.AddKill(true) == EStreak::CheeseStrike);
	TestTrue(TEXT("which waits behind the drone"), Run.Ready.Num() == 2 && Run.Next() == EStreak::AerialCheese);
	TestFalse(TEXT("only the next can be called in"), Run.Take(EStreak::CheeseStrike));
	TestTrue(TEXT("the drone first"), Run.Take(EStreak::AerialCheese) && Run.Next() == EStreak::CheeseStrike);
	Run.Died();
	TestEqual(TEXT("killed, the run starts again"), Run.Kills, 0);
	TestTrue(TEXT("and what was earned is kept"), Run.Next() == EStreak::CheeseStrike);
	TestTrue(TEXT("until it is called in"), Run.Take(EStreak::CheeseStrike) && Run.Next() == EStreak::None);
	TestFalse(TEXT("nothing to call in, nothing called"), Run.Take(EStreak::None));
	for (int32 Kill = 1; Kill <= 10; ++Kill)
	{
		Run.AddKill(true);
	}
	TestEqual(TEXT("ten in a row earns all three"), Run.Ready.Num(), 3);
	TestTrue(TEXT("the Overload last"), Run.Ready.Last() == EStreak::GoudaOverload);
	TestEqual(TEXT("the drone shows the enemy for 25 s"), RevealTime, 25.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheWheelsFallAndBurst, "CheeseStrike.Rules.TheWheelsFallAndBurst", ModeTests::Flags)

bool FTheWheelsFallAndBurst::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeStreaks;
	const FVector Spot(1000.0f, 2000.0f, 50.0f);
	const TArray<FVector> Area = StrikeArea(Spot, 90.0f);
	if (TestEqual(TEXT("seven wheels"), Area.Num(), StrikeWheels))
	{
		TestTrue(TEXT("the first on the mark"), Area[0].Equals(Spot, 0.1f));
		TestTrue(TEXT("the next the way the caller faced (+Y)"), Area[1].Equals(Spot + FVector(0.0f, StrikeRadius * StrikeRing, 0.0f), 0.5f));
		bool bRing = true;
		for (int32 Index = 1; Index < Area.Num(); ++Index)
		{
			bRing &= FMath::IsNearlyEqual(FVector::Dist(Area[Index], Spot), StrikeRadius * StrikeRing, 0.5f) && FMath::IsNearlyEqual(Area[Index].Z, Spot.Z, 0.01f);
		}
		TestTrue(TEXT("the rest round it, inside the area, at the mark's height"), bRing && StrikeRadius * StrikeRing < StrikeRadius);
		TestTrue(TEXT("evenly"), FMath::IsNearlyEqual(FVector::Dist(Area[1], Area[2]), StrikeRadius * StrikeRing, 0.5f));
		// Every point of the area is within a burst of some wheel.
		float Worst = 0.0f;
		for (float X = -StrikeRadius; X <= StrikeRadius; X += 25.0f)
		{
			for (float Y = -StrikeRadius; Y <= StrikeRadius; Y += 25.0f)
			{
				if (X * X + Y * Y > StrikeRadius * StrikeRadius)
				{
					continue;
				}
				float Nearest = BIG_NUMBER;
				for (const FVector& Wheel : Area)
				{
					Nearest = FMath::Min(Nearest, static_cast<float>(FVector::Dist(Wheel, Spot + FVector(X, Y, 0.0f))));
				}
				Worst = FMath::Max(Worst, Nearest);
			}
		}
		TestTrue(FString::Printf(TEXT("no point of the area out of every burst (the furthest %.0f cm from a wheel)"), Worst), Worst < StrikeBlast.Radius);
	}
	TestEqual(TEXT("a direct hit does the most"), BlastDamage(StrikeBlast, 0.0f), StrikeBlast.Centre);
	TestEqual(TEXT("a strike's direct hit kills"), StrikeBlast.Centre >= 100.0f, true);
	TestEqual(TEXT("the edge the least"), BlastDamage(StrikeBlast, StrikeBlast.Radius), StrikeBlast.Edge);
	TestTrue(TEXT("falling off between"), FMath::IsNearlyEqual(BlastDamage(StrikeBlast, StrikeBlast.Radius / 2.0f), (StrikeBlast.Centre + StrikeBlast.Edge) / 2.0f, 0.01f));
	TestEqual(TEXT("and nothing past it"), BlastDamage(StrikeBlast, StrikeBlast.Radius + 1.0f), 0.0f);
	TestTrue(TEXT("one Overload wheel doesn't kill: it takes two"), OverloadBlast.Centre < 100.0f && OverloadBlast.Centre * 2.0f >= 100.0f);
	const float Fall = FallTime();
	TestTrue(FString::Printf(TEXT("a wheel falls in about a second (%.2f s)"), Fall), Fall > 0.6f && Fall < 1.6f);
	TestTrue(TEXT("from its height"), FMath::IsNearlyEqual(HeightBefore(Fall), DropHeight, 1.0f));
	TestTrue(TEXT("to the ground"), FMath::IsNearlyEqual(HeightBefore(0.0f), 0.0f, 0.01f));
	TestTrue(TEXT("faster as it goes: more of the way in its last quarter second than its first"), DropHeight - HeightBefore(Fall * 0.75f) < HeightBefore(Fall * 0.25f));
	TestTrue(TEXT("the whistle gives time to move before the first lands"), StrikeWarning > Fall);
	TestTrue(TEXT("the Overload's warning longer"), OverloadWarning >= 3.0f);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FALastStandTakesTheKillingHit, "CheeseStrike.Rules.ALastStandTakesTheKillingHit", ModeTests::Flags)

bool FALastStandTakesTheKillingHit::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	TestTrue(TEXT("without a last stand, a hit hurts"), LastStandHit(false, false, 10.0f, 50.0f, false) == ELastStandHit::Hurt);
	TestTrue(TEXT("with one, a hit that doesn't kill hurts"), LastStandHit(true, false, 100.0f, 50.0f, false) == ELastStandHit::Hurt);
	TestTrue(TEXT("a hit that would kill puts them down"), LastStandHit(true, false, 30.0f, 50.0f, false) == ELastStandHit::Down);
	TestTrue(TEXT("just to nothing, too"), LastStandHit(true, false, 50.0f, 50.0f, false) == ELastStandHit::Down);
	TestTrue(TEXT("and again after getting up (revived or by themself, with little health): never only once a life"), LastStandHit(true, false, SelfReviveHealth, 50.0f, false) == ELastStandHit::Down);
	TestTrue(TEXT("down, nothing from afar harms them"), LastStandHit(true, true, 30.0f, 500.0f, false) == ELastStandHit::Absorbed);
	TestTrue(TEXT("an enemy up close finishes them"), LastStandHit(true, true, 30.0f, 1.0f, true) == ELastStandHit::Finish);
	TestTrue(TEXT("finishing is close: a couple of metres"), FinishRange > 100.0f && FinishRange <= 300.0f);

	TestTrue(TEXT("Interact on a teammate down revives"), InteractWith(true, true, 100.0f) == EInteract::Revive);
	TestTrue(TEXT("on an enemy down finishes"), InteractWith(true, false, 100.0f) == EInteract::Execute);
	TestTrue(TEXT("on someone standing, nothing"), InteractWith(false, false, 100.0f) == EInteract::None);
	TestTrue(TEXT("nor out of reach"), InteractWith(true, true, InteractRange + 1.0f) == EInteract::None);
	TestEqual(TEXT("executing is a press: it takes no holding"), InteractTime(EInteract::Execute), 0.0f);
	TestTrue(TEXT("reviving is held for a while"), InteractTime(EInteract::Revive) >= 2.0f);
	TestEqual(TEXT("nothing takes no time"), InteractTime(EInteract::None), 0.0f);
	TestEqual(TEXT("the prompt says to press to execute"), InteractPrompt(EInteract::Execute, TEXT("E"), TEXT("Bob")), FString(TEXT("PRESS E  ·  EXECUTE BOB")));
	TestEqual(TEXT("and to hold to revive"), InteractPrompt(EInteract::Revive, TEXT("LB"), TEXT("Ann")), FString(TEXT("HOLD LB  ·  REVIVE ANN")));
	TestEqual(TEXT("and nothing for nothing"), InteractPrompt(EInteract::None, TEXT("E"), TEXT("Ann")), FString());
	TestTrue(TEXT("revived with some health, not all"), ReviveHealth > 0.0f && ReviveHealth < 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDownGetUpByThemselves, "CheeseStrike.Rules.TheDownGetUpByThemselves", ModeTests::Flags)

bool FTheDownGetUpByThemselves::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	TestEqual(TEXT("left alone, up after 20 s"), SelfReviveTime, 20.0f);
	TestTrue(FString::Printf(TEXT("hurt: up with %.0f health, less than a revive gives"), SelfReviveHealth), SelfReviveHealth > 0.0f && SelfReviveHealth < ReviveHealth && SelfReviveHealth <= 30.0f);
	TestEqual(TEXT("just down: 20 s to go"), SelfReviveLeft(100.0f, 100.0f), 20.0f);
	TestEqual(TEXT("halfway"), SelfReviveLeft(100.0f, 110.0f), 10.0f);
	TestEqual(TEXT("then none"), SelfReviveLeft(100.0f, 125.0f), 0.0f);
	TestEqual(TEXT("the countdown shows whole seconds, rounded up"), SelfReviveSeconds(19.2f), 20);
	TestEqual(TEXT("down to 1"), SelfReviveSeconds(0.3f), 1);
	TestEqual(TEXT("and 0 at the end"), SelfReviveSeconds(0.0f), 0);
	TestTrue(TEXT("shown to those near enough to do something about it"), SelfReviveShownWithin >= InteractRange * 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExecutionsAreStabsAndStomps, "CheeseStrike.Rules.ExecutionsAreStabsAndStomps", ModeTests::Flags)

bool FExecutionsAreStabsAndStomps::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeExecution;
	// One or the other, as the dice fall, half the time each.
	int32 Stabs = 0;
	int32 Stomps = 0;
	for (int32 Roll = 0; Roll < 100; ++Roll)
	{
		const EKind Kind = Pick(Roll);
		Stabs += Kind == EKind::Stab ? 1 : 0;
		Stomps += Kind == EKind::Stomp ? 1 : 0;
	}
	TestEqual(TEXT("half stabs"), Stabs, 50);
	TestEqual(TEXT("half stomps"), Stomps, 50);
	TestTrue(TEXT("as replicated and back"), KindAt(static_cast<int32>(EKind::Stomp)) == EKind::Stomp && KindAt(0) == EKind::None && KindAt(9) == EKind::None);
	for (const EKind Kind : {EKind::Stab, EKind::Stomp})
	{
		const TCHAR* Called = Name(Kind);
		TestTrue(FString::Printf(TEXT("the %s lasts 2 to 3 s (%.1f)"), Called, Seconds(Kind)), Seconds(Kind) >= 2.0f && Seconds(Kind) <= 3.0f);
		TestTrue(FString::Printf(TEXT("the %s kills partway, before it ends"), Called), KillAt(Kind) > 0.5f && KillAt(Kind) < Seconds(Kind));
		TestTrue(FString::Printf(TEXT("the %s plays from the press to its end"), Called), IsPlaying(Kind, 0.0f) && IsPlaying(Kind, Seconds(Kind) - 0.01f) && !IsPlaying(Kind, Seconds(Kind) + 0.01f));
		for (const float Side : {1.0f, -1.0f})
		{
			// The executor stands beside the one down, on the side given, close enough to reach the blow, and faces it.
			const FVector Spot = SpotInBody(Kind, Side);
			const FVector Aim = AimPoint(Kind);
			TestTrue(FString::Printf(TEXT("the %s's executor stands on side %.0f"), Called, Side), Spot.Y * Side > 30.0f && FMath::Abs(Spot.X - Aim.X) < 1.0f);
			TestTrue(FString::Printf(TEXT("within reach of the blow (%.0f cm)"), FVector::Dist2D(Spot, Aim)), FVector::Dist2D(Spot, Aim) < CheeseStrikeModes::InteractRange * 0.4f);
			TestTrue(TEXT("facing it"), FMath::IsNearlyEqual(FMath::Abs(FacingYaw(Spot, Aim)), 90.0f, 1.0f) && FacingYaw(Spot, Aim) * Side < 0.0f);
			// The camera watches from the far side, up, off the floor.
			const FVector Camera = CameraInBody(Kind, Side, 0.0f);
			TestTrue(FString::Printf(TEXT("the %s's camera is on the far side, %.0f cm up"), Called, Camera.Z), Camera.Y * Side < 0.0f && Camera.Z > 100.0f);
			TestTrue(TEXT("a couple of metres off"), FVector::Dist(Camera, LookAtInBody(Kind, Side)) > 200.0f && FVector::Dist(Camera, LookAtInBody(Kind, Side)) < 450.0f);
			TestTrue(TEXT("and drifts round as it plays"), FVector::Dist(Camera, CameraInBody(Kind, Side, Seconds(Kind))) > 30.0f);
		}
	}
	TestTrue(TEXT("the stab is one thrust: drawn back, held a beat, in, held there, and out"), StabCocked < StabThrust && StabThrust < StabLands && StabLands < StabOut && StabOut < StabFree && StabFree < StabSeconds);
	TestTrue(TEXT("the thrust itself is quick"), StabLands - StabThrust <= 0.25f);
	TestTrue(TEXT("and the knife stays in a moment"), StabOut - StabLands >= 0.4f);
	TestTrue(TEXT("the one stabbed dies as it comes out"), KillAt(EKind::Stab) > StabOut && KillAt(EKind::Stab) <= StabFree);
	TestTrue(TEXT("the right side first, the knife hand toward the camera"), PreferredSide > 0.0f && CameraInBody(EKind::Stab, PreferredSide, 0.0f).X > AimPoint(EKind::Stab).X);
	TestEqual(TEXT("the stomp kills as the boot lands"), KillAt(EKind::Stomp), StompLands);
	TestTrue(TEXT("the stab goes for the chest, the stomp the head"), AimPoint(EKind::Stab).Z < AimPoint(EKind::Stomp).Z && AimPoint(EKind::Stab).X > AimPoint(EKind::Stomp).X);
	TestEqual(TEXT("an envelope is nothing before"), Envelope(0.0f, 1.0f, 2.0f, 3.0f, 4.0f), 0.0f);
	TestEqual(TEXT("all between"), Envelope(2.5f, 1.0f, 2.0f, 3.0f, 4.0f), 1.0f);
	TestEqual(TEXT("and nothing after"), Envelope(4.5f, 1.0f, 2.0f, 3.0f, 4.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheDownedFightWorse, "CheeseStrike.Rules.TheDownedFightWorse", ModeTests::Flags)

bool FTheDownedFightWorse::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	const FWeaponStats& Pistol = CheeseStrikeWeapons::Pistol();
	const FWeaponStats Down = DownedWeapon(Pistol);
	TestTrue(FString::Printf(TEXT("down, the pistol barely hurts (%.1f, standing %.1f)"), Down.Damage, Pistol.Damage), Down.Damage <= Pistol.Damage * 0.4f && Down.Damage > 0.0f);
	TestTrue(FString::Printf(TEXT("fires slowly (%.2f s, standing %.2f s)"), Down.FireInterval, Pistol.FireInterval), Down.FireInterval >= Pistol.FireInterval * 2.99f && Down.FireInterval >= 0.7f);
	TestTrue(TEXT("spreads far wider from the hip"), Down.HipSpreadDegrees >= Pistol.HipSpreadDegrees * 1.8f);
	TestTrue(TEXT("and kicks harder"), Down.RecoilUp > Pistol.RecoilUp * 2.0f);
	TestEqual(TEXT("and its first shot spreads as much as any: no steady first shot"), CheeseStrikeWeapons::SpreadFor(Down, 0, false), Down.HipSpreadDegrees);
	TestEqual(TEXT("standing, the first shot goes where it is aimed"), CheeseStrikeWeapons::SpreadFor(Pistol, 0, false), 0.0f);
	// Every shot landing, down takes many times as long to kill a full-health enemy: never a threat to one who comes to execute them.
	const float Standing = CheeseStrikeWeapons::TimeToKill(Pistol);
	const float Lying = CheeseStrikeWeapons::TimeToKill(Down);
	TestTrue(FString::Printf(TEXT("killing takes %.1f s down, %.1f s standing"), Lying, Standing), Lying > Standing * 8.0f && Lying > 10.0f);
	TestEqual(TEXT("the same weapon, the same rounds"), Down.MagazineSize, Pistol.MagazineSize);
	// The view sways heavily, both ways, and never further than its bounds.
	float MostYaw = 0.0f;
	float MostPitch = 0.0f;
	for (float Second = 0.0f; Second < 20.0f; Second += 0.05f)
	{
		const FRotator Sway = DownedSway(Second);
		MostYaw = FMath::Max(MostYaw, static_cast<float>(FMath::Abs(Sway.Yaw)));
		MostPitch = FMath::Max(MostPitch, static_cast<float>(FMath::Abs(Sway.Pitch)));
	}
	TestTrue(FString::Printf(TEXT("the view sways %.1f degrees across"), MostYaw), MostYaw > DownedSwayYaw * 0.7f && MostYaw <= DownedSwayYaw + 0.01f);
	TestTrue(FString::Printf(TEXT("and %.1f up and down"), MostPitch), MostPitch > DownedSwayPitch * 0.7f && MostPitch <= DownedSwayPitch + 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGunGameClimbsTheLadder, "CheeseStrike.Rules.GunGameClimbsTheLadder", ModeTests::Flags)

bool FGunGameClimbsTheLadder::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	const int32 Steps = CheeseStrikeArsenal::GunGameLadder().Num();
	TestTrue(TEXT("the ladder has the knife last"), Steps > 2 && CheeseStrikeArsenal::GunGameLadder().Last() == CheeseStrikeArsenal::Knife());
	const FGunGameKill First = GunGameKill(0, Steps);
	TestTrue(TEXT("a kill moves the killer a step up"), First.KillerLevel == 1 && !First.bWins);
	TestTrue(TEXT("on the last step but one, up to the knife"), GunGameKill(Steps - 2, Steps).KillerLevel == Steps - 1);
	TestTrue(TEXT("a kill on the last step wins"), GunGameKill(Steps - 1, Steps).bWins);
	// The bottom of the HUD: the player's step and the leader's, out of the ladder's.
	TArray<CheeseStrikeRules::FStanding> BySteps;
	BySteps.Add({TEXT("Ash"), 5, 0, 1});
	BySteps.Add({TEXT("Bo"), 2, 3, 2});
	const CheeseStrikeHUD::FScoreSummary Summary = CheeseStrikeHUD::GunGame(BySteps, TEXT("Bo"), Steps);
	TestEqual(TEXT("racing to the ladder's end"), Summary.Limit, Steps);
	TestTrue(TEXT("the player's step and the leader's"), Summary.Lines.Num() == 2 && Summary.Lines[0].bMine && Summary.Lines[0].Score == 2 && Summary.Lines[1].Score == 5);
	TestTrue(TEXT("a streak's name is known"), CheeseStrikeStreaks::IsStreakName(TEXT("Cheese Strike")) && !CheeseStrikeStreaks::IsStreakName(TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADownedPlayerAimsAheadAndScootsRound, "CheeseStrike.Rules.ADownedPlayerAimsAheadAndScootsRound", ModeTests::Flags)

bool FADownedPlayerAimsAheadAndScootsRound::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeDowned;
	// The cone: a body facing 90 degrees aims only so far either side of it, and so far up and down.
	TestTrue(TEXT("a cone toward the feet of 45 to 60 degrees each way"), ConeYaw >= 45.0f && ConeYaw <= 60.0f);
	TestTrue(TEXT("and a little up and down"), LowestPitch < 0.0f && LowestPitch >= -25.0f && HighestPitch > 15.0f && HighestPitch <= 40.0f);
	TestTrue(TEXT("within it, the view is kept"), ClampView(FRotator(10.0f, 100.0f, 0.0f), 90.0f).Equals(FRotator(10.0f, 100.0f, 0.0f), 0.01f));
	TestTrue(TEXT("past its side, at its edge"), FMath::IsNearlyEqual(ClampView(FRotator(0.0f, 170.0f, 0.0f), 90.0f).Yaw, 90.0f + ConeYaw, 0.01f));
	TestTrue(TEXT("the other side, the short way round"), FMath::IsNearlyEqual(ClampView(FRotator(0.0f, -100.0f, 0.0f), 170.0f).Yaw, FRotator::NormalizeAxis(170.0f + ConeYaw), 0.01f));
	TestTrue(TEXT("too low, at the lowest"), FMath::IsNearlyEqual(ClampView(FRotator(-60.0f, 90.0f, 0.0f), 90.0f).Pitch, LowestPitch, 0.01f));
	TestTrue(TEXT("too high, at the highest"), FMath::IsNearlyEqual(ClampView(FRotator(80.0f, 90.0f, 0.0f), 90.0f).Pitch, HighestPitch, 0.01f));
	TestTrue(TEXT("a pitch of 350 is 10 down"), FMath::IsNearlyEqual(ClampView(FRotator(350.0f, 90.0f, 0.0f), 90.0f).Pitch, FMath::Max(-10.0f, LowestPitch), 0.01f));
	TestTrue(TEXT("in the cone"), InCone(FRotator(5.0f, 90.0f + ConeYaw - 1.0f, 0.0f), 90.0f));
	TestFalse(TEXT("out of it"), InCone(FRotator(0.0f, 90.0f + ConeYaw + 2.0f, 0.0f), 90.0f));
	// Turning round: not for a look a little aside, slowly after the view beyond that.
	TestEqual(TEXT("the body stays put for a look a little aside"), TurnBody(90.0f, 90.0f + TurnFree - 1.0f, 0.5f), 90.0f);
	TestTrue(TEXT("and turns slowly after a look further"), FMath::IsNearlyEqual(TurnBody(90.0f, 90.0f + ConeYaw, 0.1f), 90.0f + TurnRate * 0.1f, 0.01f));
	TestTrue(TEXT("until the view is within reach of its facing again"), FMath::IsNearlyEqual(TurnBody(90.0f, 90.0f + TurnFree + 1.0f, 1.0f), 91.0f, 0.01f));
	TestTrue(TEXT("either way, through the back"), FMath::IsNearlyEqual(TurnBody(175.0f, -140.0f, 0.1f), FRotator::NormalizeAxis(175.0f + TurnRate * 0.1f), 0.01f));
	TestTrue(TEXT("slowly: a half turn takes seconds"), 180.0f / TurnRate >= 3.0f);
	TestTrue(TEXT("getting up, it turns to the view, quickly"), FMath::IsNearlyEqual(TurnRising(90.0f, 100.0f, 0.5f), 100.0f, 0.01f));
	// Scooting: slowly, along the body only, and not into what its legs are against.
	TestTrue(TEXT("a scoot is well under a crouched walk"), ScootSpeed < 0.4f * 225.0f && ScootSpeed > 20.0f);
	const FVector Facing(0.0f, 1.0f, 0.0f);
	TestTrue(TEXT("toward the feet, with room"), ScootInput(FVector(0.0f, 1.0f, 0.0f), Facing, 100.0f).Equals(Facing, 0.001f));
	TestTrue(TEXT("toward the head"), ScootInput(FVector(0.0f, -1.0f, 0.0f), Facing, 0.0f).Equals(-Facing, 0.001f));
	TestTrue(TEXT("never sideways"), ScootInput(FVector(1.0f, 0.0f, 0.0f), Facing, 100.0f).IsNearlyZero(0.001f));
	TestTrue(TEXT("nor toward the feet with the legs against a wall"), ScootInput(FVector(0.0f, 1.0f, 0.0f), Facing, LegRoomToScoot - 1.0f).IsNearlyZero(0.001f));
	TestEqual(TEXT("the legs reach out all the way with room"), LegReach(500.0f), LegLength);
	TestEqual(TEXT("short of the wall"), LegReach(80.0f), 80.0f - ToeGap);
	TestEqual(TEXT("and fold up no further than so far"), LegReach(0.0f), LegReachLeast);
	// The body lies along its facing: the hips ahead of the capsule's axis, the legs further, the head raised behind.
	const FShape Body = Shape(FVector(0.0f, 0.0f, 100.0f), 90.0f, 500.0f);
	TestTrue(TEXT("the hips on the floor, ahead of the axis"), Body.Hips.Y > 10.0f && Body.Hips.Z - 100.0f < 20.0f && FMath::Abs(Body.Hips.X) < 0.01f);
	TestTrue(TEXT("the feet out ahead of the hips"), Body.Feet.Y - Body.Hips.Y > 80.0f && Body.Feet.Z - 100.0f < 20.0f);
	TestTrue(TEXT("the head raised, over the shoulders, the shoulders over the hips"), Body.Head.Z > Body.Shoulders.Z && Body.Shoulders.Z > Body.Hips.Z + 10.0f);
	TestTrue(TEXT("the head about 40 cm up, within the capsule's radius of its axis"), Body.Head.Z - 100.0f > 35.0f && Body.Head.Z - 100.0f < 55.0f && FVector::Dist2D(Body.Head, FVector::ZeroVector) < 34.0f);
	TestTrue(TEXT("its middle between the shoulders and the hips"), Middle(Body).Y > Body.Shoulders.Y && Middle(Body).Y < Body.Hips.Y);
	return true;
}

#endif
