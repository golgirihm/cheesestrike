// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The rules of a match, without a world: where the killed come back, who
// leads, when a match is over, and what a host may set.

#include "Misc/AutomationTest.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "UI/CheeseStrikeMenu.h"

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
	TestTrue(TEXT("a match told to start now does"), StartsAtOnce(TEXT("?listen?StartNow")));
	TestFalse(TEXT("otherwise it waits in the lobby"), StartsAtOnce(TEXT("?listen?KillLimit=5")));
	TestEqual(TEXT("the design's kill limits are offered"), static_cast<int32>(UE_ARRAY_COUNT(KillLimitChoices)), 5);
	TestTrue(TEXT("with the usual one among them"), KillLimitChoices[2] == KillLimit);
	TestTrue(TEXT("and the usual respawn delay among its choices"), RespawnDelayChoices[1] == RespawnDelay);
	TestEqual(TEXT("the countdown"), MatchCountdown, 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheSidesAreFilledWithBots, "CheeseStrike.Rules.TheSidesAreFilledWithBots", RulesTests::Flags)

bool FTheSidesAreFilledWithBots::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestEqual(TEXT("two sides"), Teams, 2);
	TestEqual(TEXT("of eight, for the design's sixteen"), TeamSize * Teams, 16);
	TestEqual(TEXT("named"), FString(TeamNames[0]), FString(TEXT("The Gouda Guys")));
	TestEqual(TEXT("and named"), FString(TeamNames[1]), FString(TEXT("Mozza Radicals")));
	const int32 Even[Teams] = {2, 2};
	const int32 Uneven[Teams] = {3, 2};
	TestEqual(TEXT("a newcomer joins the first of equal sides"), TeamToJoin(Even), 0);
	TestEqual(TEXT("or the smaller"), TeamToJoin(Uneven), 1);

	TestTrue(TEXT("there are names for every slot"), NumBotNames() >= TeamSize * Teams);
	const TArray<FString> Drawn = BotNames(7, 16);
	TestEqual(TEXT("as many as asked"), Drawn.Num(), 16);
	TSet<FString> Unique(Drawn);
	TestEqual(TEXT("no two alike"), Unique.Num(), 16);
	TestTrue(TEXT("the same seed draws the same"), BotNames(7, 16) == Drawn);
	TestTrue(TEXT("another seed another order"), BotNames(8, 16) != Drawn);
	TestEqual(TEXT("every slot without a say"), BotsFrom(TEXT("?listen")), -1);
	TestEqual(TEXT("or as many as the URL says"), BotsFrom(TEXT("?Bots=3")), 3);
	TestEqual(TEXT("none at all"), BotsFrom(TEXT("?Bots=0")), 0);

	// Two players on the first side, one on the second; the sides fill with bots after them.
	TArray<FRosterEntry> Players;
	Players.Add({TEXT("Ash"), 0, false});
	Players.Add({TEXT("Bo"), 1, false});
	Players.Add({TEXT("Cam"), 0, false});
	const TArray<FRosterEntry> First = TeamColumn(0, Players, Drawn, -1);
	const TArray<FRosterEntry> Second = TeamColumn(1, Players, Drawn, -1);
	TestEqual(TEXT("the first side is full"), First.Num(), TeamSize);
	TestEqual(TEXT("so is the second"), Second.Num(), TeamSize);
	TestTrue(TEXT("players first"), First[0].Name == TEXT("Ash") && First[1].Name == TEXT("Cam") && !First[1].bBot);
	TestTrue(TEXT("then bots, from the side's share of the names"), First[2].bBot && First[2].Name == Drawn[0] && Second[1].bBot && Second[1].Name == Drawn[TeamSize]);
	TestEqual(TEXT("asked for three bots, three in all"), TeamColumn(0, Players, Drawn, 3).Num() + TeamColumn(1, Players, Drawn, 3).Num(), Players.Num() + 3);
	TestEqual(TEXT("asked for none, players only"), TeamColumn(1, Players, Drawn, 0).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuFollowsTheMatch, "CheeseStrike.Rules.TheMenuFollowsTheMatch", RulesTests::Flags)

bool FTheMenuFollowsTheMatch::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::PageFor;
	TestTrue(TEXT("alone before a match, the front end"), PageFor(false, false, false, false, EMenuPage::Main) == EMenuPage::Main);
	TestTrue(TEXT("or the page chosen in it"), PageFor(false, false, false, false, EMenuPage::Host) == EMenuPage::Host && PageFor(false, false, false, false, EMenuPage::Find) == EMenuPage::Find);
	TestTrue(TEXT("but not a page that isn't the front end's"), PageFor(false, false, false, false, EMenuPage::Pause) == EMenuPage::Main);
	TestTrue(TEXT("in a hosted or joined match that waits, the lobby"), PageFor(false, false, true, false, EMenuPage::Host) == EMenuPage::Lobby);
	TestTrue(TEXT("once the match is on, nothing"), PageFor(true, false, true, false, EMenuPage::Main) == EMenuPage::None);
	TestTrue(TEXT("unless paused"), PageFor(true, false, true, true, EMenuPage::Main) == EMenuPage::Pause);
	TestTrue(TEXT("and once it is over, nothing, whatever else"), PageFor(true, true, true, true, EMenuPage::Main) == EMenuPage::None);
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
