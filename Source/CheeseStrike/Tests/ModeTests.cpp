// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The modes and the streaks, without a world: what each mode is and how it
// is won, who takes a round of Elimination, who may hurt whom, where a side
// comes in, when streaks are earned, and where a Cheese Strike's wheels
// land and what they do.

#include "Misc/AutomationTest.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Weapons/CheeseStrikeArsenal.h"

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
	TestEqual(TEXT("five modes"), NumModes, 5);
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
	TestEqual(TEXT("two minutes a round"), RoundTimeFrom(TEXT("")), 120.0f);
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
	TestTrue(TEXT("time up: over, whatever stands"), RoundOver(Side(4, 2, 100), Side(4, 1, 100), true, Winner));
	TestEqual(TEXT("to the side with more left standing"), Winner, 0);
	TestTrue(TEXT("as many standing: to the one with more health"), RoundOver(Side(4, 2, 90), Side(4, 2, 160), true, Winner) && Winner == 1);
	TestTrue(TEXT("level on both: a draw"), RoundOver(Side(4, 2, 100), Side(4, 2, 100), true, Winner) && Winner == NoTeam);
	TestTrue(TEXT("and a lone player, at the time, takes it"), RoundOver(Side(1, 1, 100), Side(0, 0, 0), true, Winner) && Winner == 0);
	TestTrue(TEXT("rounds are short and the break shorter"), RoundTime >= 60.0f && RoundBreak > 0.0f && RoundBreak <= 10.0f);
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
	const TArray<FVector> Line = StrikeLine(Spot, 90.0f);
	if (TestEqual(TEXT("seven wheels"), Line.Num(), StrikeWheels))
	{
		TestTrue(TEXT("the middle one on the mark"), Line[StrikeWheels / 2].Equals(Spot, 0.1f));
		TestTrue(TEXT("along the way the caller faced (+Y)"), FMath::IsNearlyEqual(Line.Last().Y - Line[0].Y, StrikeSpacing * (StrikeWheels - 1), 0.5f) && FMath::IsNearlyEqual(Line.Last().X, Line[0].X, 0.5f));
		TestTrue(TEXT("evenly spaced"), FMath::IsNearlyEqual(FVector::Dist(Line[0], Line[1]), StrikeSpacing, 0.5f));
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
	TestTrue(TEXT("without a last stand, a hit hurts"), LastStandHit(false, false, false, 10.0f, 50.0f, false) == ELastStandHit::Hurt);
	TestTrue(TEXT("with one, a hit that doesn't kill hurts"), LastStandHit(true, false, false, 100.0f, 50.0f, false) == ELastStandHit::Hurt);
	TestTrue(TEXT("a hit that would kill puts them down"), LastStandHit(true, false, false, 30.0f, 50.0f, false) == ELastStandHit::Down);
	TestTrue(TEXT("just to nothing, too"), LastStandHit(true, false, false, 50.0f, 50.0f, false) == ELastStandHit::Down);
	TestTrue(TEXT("but once a life: the second kills"), LastStandHit(true, false, true, 30.0f, 50.0f, false) == ELastStandHit::Hurt);
	TestTrue(TEXT("down, nothing from afar harms them"), LastStandHit(true, true, true, 30.0f, 500.0f, false) == ELastStandHit::Absorbed);
	TestTrue(TEXT("an enemy up close finishes them"), LastStandHit(true, true, true, 30.0f, 1.0f, true) == ELastStandHit::Finish);
	TestTrue(TEXT("finishing is close: a couple of metres"), FinishRange > 100.0f && FinishRange <= 300.0f);

	TestTrue(TEXT("Interact on a teammate down revives"), InteractWith(true, true, 100.0f) == EInteract::Revive);
	TestTrue(TEXT("on an enemy down finishes"), InteractWith(true, false, 100.0f) == EInteract::Execute);
	TestTrue(TEXT("on someone standing, nothing"), InteractWith(false, false, 100.0f) == EInteract::None);
	TestTrue(TEXT("nor out of reach"), InteractWith(true, true, InteractRange + 1.0f) == EInteract::None);
	TestTrue(TEXT("reviving takes longer than finishing"), InteractTime(EInteract::Revive) > InteractTime(EInteract::Execute) && InteractTime(EInteract::Execute) > 0.0f);
	TestEqual(TEXT("nothing takes no time"), InteractTime(EInteract::None), 0.0f);
	TestTrue(TEXT("revived with some health, not all"), ReviveHealth > 0.0f && ReviveHealth < 100.0f);
	TestTrue(TEXT("left alone, they last a while"), LastStandTime >= 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGunGameClimbsTheLadder, "CheeseStrike.Rules.GunGameClimbsTheLadder", ModeTests::Flags)

bool FGunGameClimbsTheLadder::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeModes;
	const int32 Steps = CheeseStrikeArsenal::GunGameLadder().Num();
	TestTrue(TEXT("the ladder has the knife last"), Steps > 2 && CheeseStrikeArsenal::GunGameLadder().Last() == CheeseStrikeArsenal::Knife());
	const FGunGameKill First = GunGameKill(0, false, Steps);
	TestTrue(TEXT("a kill moves the killer a step up"), First.KillerLevel == 1 && !First.bWins && !First.bVictimDemoted);
	const FGunGameKill Knifed = GunGameKill(3, true, Steps);
	TestTrue(TEXT("a kill with the knife before its step: up, and the killed down a step"), Knifed.KillerLevel == 4 && Knifed.bVictimDemoted && !Knifed.bWins);
	TestTrue(TEXT("on the last step but one, up to the knife"), GunGameKill(Steps - 2, false, Steps).KillerLevel == Steps - 1);
	TestTrue(TEXT("a kill on the last step wins"), GunGameKill(Steps - 1, true, Steps).bWins);
	TestFalse(TEXT("and humiliates nobody"), GunGameKill(Steps - 1, true, Steps).bVictimDemoted);
	TestTrue(TEXT("the knife is carried until it is the step"), GunGameKeepsKnife(0, Steps) && GunGameKeepsKnife(Steps - 2, Steps) && !GunGameKeepsKnife(Steps - 1, Steps));
	TestTrue(TEXT("a streak's name is known"), CheeseStrikeStreaks::IsStreakName(TEXT("Cheese Strike")) && !CheeseStrikeStreaks::IsStreakName(TEXT("")));
	return true;
}

#endif
