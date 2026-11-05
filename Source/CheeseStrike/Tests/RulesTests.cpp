// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The rules of a match, without a world: where the killed come back, who
// leads, when a match is over, and what a host may set.

#include "Misc/AutomationTest.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRange.h"
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
	TestEqual(TEXT("the usual wait"), RespawnDelay, 5.0f);
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
	TestTrue(TEXT("and the usual respawn delay among its choices"), RespawnDelayChoices[2] == RespawnDelay);
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
	TestEqual(TEXT("the usual seven without a say"), BotsFrom(TEXT("?listen")), DefaultBots);
	TestEqual(TEXT("or every slot, if asked"), BotsFrom(TEXT("?Bots=-1")), -1);
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
	// The first bot to the smaller side (the second), the next to the first, and so on, each named in the order they come.
	TestTrue(TEXT("then bots, named in the order they were added"), First[2].bBot && First[2].Name == Drawn[1] && Second[1].bBot && Second[1].Name == Drawn[0]);
	TestEqual(TEXT("asked for three bots, three in all"), TeamColumn(0, Players, Drawn, 3).Num() + TeamColumn(1, Players, Drawn, 3).Num(), Players.Num() + 3);
	TestEqual(TEXT("asked for none, players only"), TeamColumn(1, Players, Drawn, 0).Num(), 1);
	TestEqual(TEXT("three bots go to the smaller side first: one to the first"), TeamColumn(0, Players, Drawn, 3).Num(), 3);
	TestEqual(TEXT("and two to the second, three a side"), TeamColumn(1, Players, Drawn, 3).Num(), 3);
	TestEqual(TEXT("bots in a match with 3 players, filling in"), BotsIn(3, -1), 13);
	TestEqual(TEXT("or as many as asked"), BotsIn(3, 5), 5);
	TestEqual(TEXT("but never more than the seats"), BotsIn(3, 20), 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHostPicksAMap, "CheeseStrike.Rules.AHostPicksAMap", RulesTests::Flags)

bool FAHostPicksAMap::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeRules;
	TestTrue(TEXT("there is more than one map"), NumMaps() >= 2);
	TestEqual(TEXT("the first is the Creamery"), FString(Maps[0].Name), FString(TEXT("The Creamery")));
	TestEqual(TEXT("a level is known by its package"), MapIndexFor(TEXT("/Game/CheeseStrike/Maps/Lvl_Creamery")), 0);
	// The Proving Ground is one of them, wherever the list has it: the game's own, built by a script, not the template's
	// shooter level, which is the front end's and the play tests' playground and no map of the game's.
	int32 Proving = INDEX_NONE;
	for (int32 Index = 0; Index < NumMaps(); ++Index)
	{
		Proving = FString(Maps[Index].Level) == TEXT("/Game/CheeseStrike/Maps/Lvl_ProvingGround") ? Index : Proving;
		TestTrue(FString::Printf(TEXT("%s is a map of the game's own"), Maps[Index].Name), FString(Maps[Index].Level).StartsWith(TEXT("/Game/CheeseStrike/Maps/")));
	}
	TestTrue(TEXT("the Proving Ground is one"), Proving > 0);
	TestEqual(TEXT("or by its object path"), MapIndexFor(TEXT("/Game/CheeseStrike/Maps/Lvl_ProvingGround.Lvl_ProvingGround")), Proving);
	TestEqual(TEXT("the template's shooter level is none"), MapIndexFor(TEXT("/Game/Variant_Shooter/Lvl_Shooter")), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("or as it is played in the editor"), MapIndexFor(TEXT("/Game/CheeseStrike/Maps/UEDPIE_1_Lvl_Creamery")), 0);
	TestEqual(TEXT("and a level that isn't one of them is none"), MapIndexFor(TEXT("/Game/Elsewhere/Lvl_Other")), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the URL picks a map"), MapFrom(TEXT("?Map=1"), 0), 1);
	TestEqual(TEXT("but not one there isn't"), MapFrom(TEXT("?Map=9"), 0), 0);
	TestEqual(TEXT("and without it, the one given"), MapFrom(TEXT("?KillLimit=5"), 1), 1);
	// The Practice Range is a map (for the cook, the URL and PRACTICE), but not one the lobby offers or a match is hosted on.
	const int32 Range = MapIndexFor(CheeseStrikeRange::Level);
	TestTrue(TEXT("the Practice Range is a map"), Range != INDEX_NONE);
	TestFalse(TEXT("but not one the lobby offers"), IsOffered(Range));
	TestTrue(TEXT("every other map is"), IsOffered(0) && IsOffered(DefaultMap()) && IsOffered(Proving));
	TestFalse(TEXT("nor is a map there isn't"), IsOffered(NumMaps()) || IsOffered(INDEX_NONE));
	const TArray<int32> Offered = MapsOffered();
	TestEqual(TEXT("the lobby offers every map but the range"), Offered.Num(), NumMaps() - 1);
	TestFalse(TEXT("not the range"), Offered.Contains(Range));
	TestTrue(TEXT("in the maps' order"), Offered.Num() > 1 && Offered[0] == 0 && Offered.Last() > Offered[0]);
	TestTrue(TEXT("a match with ?listen is hosted for others"), IsHosted(TEXT("?listen?Bots=0")) && IsHosted(CheeseStrikeRules::HostOptions() + TEXT("?listen")));
	TestFalse(TEXT("PRACTICE plays alone"), IsHosted(CheeseStrikeRange::PracticeOptions()));
	TestEqual(TEXT("a match hosted on the range plays the default map instead"), MapForMatch(Range, true), DefaultMap());
	TestEqual(TEXT("alone, the range is played"), MapForMatch(Range, false), Range);
	TestEqual(TEXT("a map offered is played as picked"), MapForMatch(0, true), 0);
	TestEqual(TEXT("a level not in the list is left alone when hosted"), MapForMatch(INDEX_NONE, true), INDEX_NONE);
	TestEqual(TEXT("a travelled match waits for so many players"), PlayersFrom(TEXT("?Players=3")), 3);
	TestEqual(TEXT("and a hosted one for none"), PlayersFrom(TEXT("")), 0);
	TestEqual(TEXT("the bots' names travel by their seed"), BotSeedFrom(TEXT("?BotSeed=1234"), 7), 1234);
	TestEqual(TEXT("or are drawn afresh"), BotSeedFrom(TEXT(""), 7), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuFollowsTheMatch, "CheeseStrike.Rules.TheMenuFollowsTheMatch", RulesTests::Flags)

bool FTheMenuFollowsTheMatch::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::PageFor;
	TestTrue(TEXT("alone before a match, the front end"), PageFor(false, false, false, false, EMenuPage::Main) == EMenuPage::Main);
	TestTrue(TEXT("or the page chosen in it"), PageFor(false, false, false, false, EMenuPage::Find) == EMenuPage::Find);
	TestTrue(TEXT("such as the credits"), PageFor(false, false, false, false, EMenuPage::Credits) == EMenuPage::Credits);
	TestTrue(TEXT("but not a page that isn't the front end's"), PageFor(false, false, false, false, EMenuPage::Pause) == EMenuPage::Main && PageFor(false, false, false, false, EMenuPage::Lobby) == EMenuPage::Main);
	TestTrue(TEXT("in a hosted or joined match that waits, the lobby"), PageFor(false, false, true, false, EMenuPage::Find) == EMenuPage::Lobby);
	TestTrue(TEXT("once the match is on, nothing"), PageFor(true, false, true, false, EMenuPage::Main) == EMenuPage::None);
	TestTrue(TEXT("unless paused"), PageFor(true, false, true, true, EMenuPage::Main) == EMenuPage::Pause);
	TestTrue(TEXT("and once it is over, nothing, whatever else"), PageFor(true, true, true, true, EMenuPage::Main) == EMenuPage::None);
	TestTrue(TEXT("while the match goes to another level, the lobby, however it stands"), PageFor(true, true, true, false, EMenuPage::Main, true) == EMenuPage::Lobby);
	TestTrue(TEXT("but alone, the front end"), PageFor(true, true, false, false, EMenuPage::Main, true) == EMenuPage::Main);
	TestTrue(TEXT("and back to the lobby after the results, the results held up, never a lobby still being made"), PageFor(true, true, true, false, EMenuPage::Main, true, true) == EMenuPage::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheLobbySaysWhereItStands, "CheeseStrike.Rules.TheLobbySaysWhereItStands", RulesTests::Flags)

bool FTheLobbySaysWhereItStands::RunTest(const FString& Parameters)
{
	using CheeseStrikeMenu::ELobbyStatus;
	using CheeseStrikeMenu::LobbyStatus;
	TestTrue(TEXT("a joiner without the host's match yet is joining, and guesses at nothing"), LobbyStatus(false, false, false, 0.0f, false) == ELobbyStatus::Joining);
	TestTrue(TEXT("with it, waits for the host"), LobbyStatus(true, false, false, 0.0f, false) == ELobbyStatus::Waiting);
	TestTrue(TEXT("whom the host can start"), LobbyStatus(true, false, false, 0.0f, true) == ELobbyStatus::CanStart);
	TestTrue(TEXT("counting down, the seconds"), LobbyStatus(true, false, true, 3.2f, false) == ELobbyStatus::Counting && LobbyStatus(true, false, true, 3.2f, true) == ELobbyStatus::Counting);
	TestTrue(TEXT("at 0, starting, not \"Starting in 0\" nor Start again"), LobbyStatus(true, false, true, 0.0f, false) == ELobbyStatus::Starting && LobbyStatus(true, false, true, 0.0f, true) == ELobbyStatus::Starting);
	TestTrue(TEXT("called off, back to waiting at once"), LobbyStatus(true, false, false, 0.0f, false) == ELobbyStatus::Waiting);
	TestTrue(TEXT("leaving the level, going there, whatever else"), LobbyStatus(true, true, true, 0.0f, true) == ELobbyStatus::Travelling && LobbyStatus(false, true, false, 0.0f, false) == ELobbyStatus::Travelling);
	using CheeseStrikeMenu::ShouldRedrawLobby;
	TestTrue(TEXT("the lobby is drawn again when what it shows changes"), ShouldRedrawLobby(true, false, false));
	TestFalse(TEXT("and only then"), ShouldRedrawLobby(false, false, false) || ShouldRedrawLobby(false, true, true));
	TestFalse(TEXT("not while the host has a dropdown open, which it would close"), ShouldRedrawLobby(true, false, true));
	TestTrue(TEXT("unless the lobby has moved on (starting, travelling)"), ShouldRedrawLobby(true, true, true));
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
