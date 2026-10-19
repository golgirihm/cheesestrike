// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The rules of a match, without a world: where the killed come back, who
// leads, when a match is over, and what a host may set.

#include "Misc/AutomationTest.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace RulesTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheKilledComeBackFurthestFromEnemies, "CheeseStrike.Rules.TheKilledComeBackFurthestFromEnemies", RulesTests::Flags)

bool FTheKilledComeBackFurthestFromEnemies::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	const TArray<FVector> Spots = {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0)};
	TestEqual(TEXT("with an enemy at one end, the other end"), FurthestFrom(Spots, TArray<FVector>{FVector(-100, 0, 0)}), 2);
	TestEqual(TEXT("with enemies at both ends, the middle"), FurthestFrom(Spots, TArray<FVector>{FVector(-100, 0, 0), FVector(2100, 0, 0)}), 1);
	TestEqual(TEXT("it is the nearest enemy that counts"), FurthestFrom(Spots, TArray<FVector>{FVector(2100, 0, 0), FVector(2100, 0, 0), FVector(2100, 0, 0), FVector(-300, 0, 0)}), 1);
	TestEqual(TEXT("with no enemies, the first"), FurthestFrom(Spots, TArray<FVector>()), 0);
	TestEqual(TEXT("with no spots, none"), FurthestFrom(TArray<FVector>(), TArray<FVector>{FVector::ZeroVector}), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheScoreboardIsInOrder, "CheeseStrike.Rules.TheScoreboardIsInOrder", RulesTests::Flags)

bool FTheScoreboardIsInOrder::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	using RulesTests::Line;
	const TArray<FStanding> InOrder = Sorted({Line(TEXT("Cam"), 3, 5, 3), Line(TEXT("Ash"), 5, 2, 1), Line(TEXT("Bo"), 5, 1, 2), Line(TEXT("Dee"), 3, 5, 4)});
	if (TestEqual(TEXT("everyone is on it"), InOrder.Num(), 4))
	{
		TestEqual(TEXT("most kills first, fewest deaths among equals"), InOrder[0].Name, FString(TEXT("Bo")));
		TestEqual(TEXT("then the other"), InOrder[1].Name, FString(TEXT("Ash")));
		TestEqual(TEXT("equals keep the order they came in"), InOrder[2].Name, FString(TEXT("Cam")));
		TestEqual(TEXT("and the last"), InOrder[3].Name, FString(TEXT("Dee")));
	}
	TestEqual(TEXT("the leader"), Leader({Line(TEXT("Cam"), 3, 5, 3), Line(TEXT("Ash"), 5, 2, 1)}), FString(TEXT("Ash")));
	TestEqual(TEXT("nobody leads nobody"), Leader({}), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAMatchEndsAtTheKillLimit, "CheeseStrike.Rules.AMatchEndsAtTheKillLimit", RulesTests::Flags)

bool FAMatchEndsAtTheKillLimit::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestFalse(TEXT("not before"), IsOver(19, 20));
	TestTrue(TEXT("at the limit"), IsOver(20, 20));
	TestTrue(TEXT("or past it"), IsOver(21, 20));
	TestFalse(TEXT("never without one"), IsOver(100, 0));
	TestEqual(TEXT("the usual limit"), KillLimit, 20);
	TestEqual(TEXT("the usual wait"), RespawnDelay, 3.0f);
	TestEqual(TEXT("and a fresh spawn's protection"), SpawnProtection, 3.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHostSetsTheLimits, "CheeseStrike.Rules.TheHostSetsTheLimits", RulesTests::Flags)

bool FTheHostSetsTheLimits::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestEqual(TEXT("the kill limit from the URL"), KillLimitFrom(TEXT("?listen?KillLimit=10")), 10);
	TestEqual(TEXT("the usual one without"), KillLimitFrom(TEXT("?listen")), KillLimit);
	TestEqual(TEXT("or with nonsense"), KillLimitFrom(TEXT("?KillLimit=lots")), KillLimit);
	TestEqual(TEXT("at least one"), KillLimitFrom(TEXT("?KillLimit=0")), 1);
	TestEqual(TEXT("the respawn delay from the URL"), RespawnDelayFrom(TEXT("?RespawnDelay=1")), 1.0f);
	TestEqual(TEXT("the usual one without"), RespawnDelayFrom(TEXT("")), RespawnDelay);
	TestEqual(TEXT("no shorter than the design allows"), RespawnDelayFrom(TEXT("?RespawnDelay=0")), 0.5f);
	TestEqual(TEXT("nor longer"), RespawnDelayFrom(TEXT("?RespawnDelay=60")), 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMatchHasItsOwnState, "CheeseStrike.Rules.TheMatchHasItsOwnState", RulesTests::Flags)

bool FTheMatchHasItsOwnState::RunTest(const FString& Parameters)
{
	const ACheeseStrikeGameMode* Mode = GetDefault<ACheeseStrikeGameMode>();
	TestEqual(TEXT("the match's state"), Mode->GameStateClass.Get(), static_cast<UClass*>(ACheeseStrikeGameState::StaticClass()));
	TestEqual(TEXT("each player's"), Mode->PlayerStateClass.Get(), static_cast<UClass*>(ACheeseStrikePlayerState::StaticClass()));
	TestEqual(TEXT("the usual kill limit"), Mode->GetKillLimit(), CheeseStrikeRules::KillLimit);
	TestEqual(TEXT("and respawn delay"), Mode->GetRespawnDelay(), CheeseStrikeRules::RespawnDelay);
	return true;
}

#endif
