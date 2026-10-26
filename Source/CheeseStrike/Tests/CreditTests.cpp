// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Assists and King of the Hill's time, as plain rules: who helped with a kill,
// whose turn it is for the hill's time, and how the podium ranks a hill's
// players by the time they captured.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeCredit.h"
#include "Rules/CheeseStrikePodiums.h"

namespace CreditTests
{
	using namespace CheeseStrikeCredit;
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssistsAreTheRecentHitsButNotTheKillerOrVictim, "CheeseStrike.Credit.Assists", CreditTests::Flags)

bool FAssistsAreTheRecentHitsButNotTheKillerOrVictim::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeCredit;
	const TArray<FHit> Hits = {{1, 5.0f}, {2, 8.0f}, {3, 9.0f}, {2, 9.5f}, {4, 1.0f}, {-1, 9.0f}, {0, 9.0f}};
	const TArray<int32> Found = Assisters(Hits, 1, 0, 12.0f);
	TestEqual(TEXT("the killer gets no assist"), Found.Contains(1), false);
	TestEqual(TEXT("the victim gets none"), Found.Contains(0), false);
	TestEqual(TEXT("a hit from long ago isn't one"), Found.Contains(4), false);
	TestEqual(TEXT("a nobody's hit isn't one"), Found.Contains(-1), false);
	TestEqual(TEXT("two who hit recently assist, each once"), Found.Num(), 2);
	TestTrue(TEXT("the one who hit twice is there once"), Found.Contains(2) && Found.Contains(3));
	TestEqual(TEXT("nobody killed: everyone who hit recently"), Assisters(Hits, -1, 0, 12.0f).Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHillsTimeGoesRoundTheHoldersInTurn, "CheeseStrike.Credit.HillTimeInTurns", CreditTests::Flags)

bool FTheHillsTimeGoesRoundTheHoldersInTurn::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeCredit;
	TestEqual(TEXT("the first three seconds to the first holder"), CreditIndex(0.0f, 2), 0);
	TestEqual(TEXT("then the next"), CreditIndex(CreditRotation, 2), 1);
	TestEqual(TEXT("and back round"), CreditIndex(2.0f * CreditRotation, 2), 0);
	TestEqual(TEXT("one holder: always them"), CreditIndex(100.0f, 1), 0);
	TestEqual(TEXT("nobody: none"), CreditIndex(5.0f, 0), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHillRanksByTimeCaptured, "CheeseStrike.Credit.HillRanksByTime", CreditTests::Flags)

bool FTheHillRanksByTimeCaptured::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikePodiums;
	FContender Long;
	Long.Standing.Name = TEXT("Long");
	Long.Standing.Kills = 1;
	Long.HillSeconds = 90;
	FContender Many;
	Many.Standing.Name = TEXT("Many");
	Many.Standing.Kills = 9;
	Many.HillSeconds = 30;
	const CheeseStrikeRules::FStanding LongRank = RankedStanding(Long, CheeseStrikeModes::EWinBy::Hill, true);
	const CheeseStrikeRules::FStanding ManyRank = RankedStanding(Many, CheeseStrikeModes::EWinBy::Hill, true);
	TestTrue(TEXT("more time captured ranks above more kills"), LongRank.Kills > ManyRank.Kills);
	FContender Tied = Long;
	Tied.Standing.Name = TEXT("Tied");
	Tied.Standing.Kills = 5;
	TestTrue(TEXT("with the same time, kills break the tie"), RankedStanding(Tied, CheeseStrikeModes::EWinBy::Hill, true).Kills > LongRank.Kills);
	return true;
}
