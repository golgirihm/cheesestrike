// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// How a match is offered, searched for and described through the online
// service, without one.

#include "Misc/AutomationTest.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "Rules/CheeseStrikeRules.h"
#include "Sessions/CheeseStrikeSessions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SessionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAMatchIsOfferedAsALobby, "CheeseStrike.Sessions.AMatchIsOfferedAsALobby", SessionTests::Flags)

bool FAMatchIsOfferedAsALobby::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSessions;
	const FOnlineSessionSettings Steam = HostSettings(TEXT("?KillLimit=5?RespawnDelay=1"), false);
	TestEqual(TEXT("room for the design's players"), Steam.NumPublicConnections, MaxPlayers);
	TestFalse(TEXT("not advertised to a public search: a friend finds it, or it is sent on an invite"), Steam.bShouldAdvertise);
	TestTrue(TEXT("joinable while on"), Steam.bAllowJoinInProgress);
	TestTrue(TEXT("with invitations"), Steam.bAllowInvites);
	TestTrue(TEXT("a lobby, which the friends list can join"), Steam.bUseLobbiesIfAvailable && Steam.bUsesPresence && Steam.bAllowJoinViaPresence);
	TestTrue(TEXT("joinable by friends only, and on invites"), Steam.bAllowJoinViaPresenceFriendsOnly);
	TestFalse(TEXT("not on the local network"), Steam.bIsLANMatch);
	TestFalse(TEXT("not dedicated"), Steam.bIsDedicated);
	FString Game;
	TestTrue(TEXT("marked as this game's"), Steam.Get(GameKey, Game) && Game == GameValue);
	int32 KillLimit = 0;
	TestTrue(TEXT("carrying the kill limit"), Steam.Get(KillLimitKey, KillLimit) && KillLimit == 5);
	float RespawnDelay = 0.0f;
	TestTrue(TEXT("and the respawn delay"), Steam.Get(RespawnDelayKey, RespawnDelay) && RespawnDelay == 1.0f);
	FString Map;
	TestTrue(FString::Printf(TEXT("and the map, by name (%s)"), *Map), Steam.Get(SETTING_MAPNAME, Map) && Map == CheeseStrikeRules::Maps[CheeseStrikeRules::MapIndexFor(Level)].Name);

	const FOnlineSessionSettings Lan = HostSettings(TEXT(""), true);
	TestTrue(TEXT("on the local network, a LAN match"), Lan.bIsLANMatch);
	TestFalse(TEXT("without a lobby"), Lan.bUseLobbiesIfAvailable || Lan.bUsesPresence);
	TestTrue(TEXT("with the usual kill limit"), Lan.Get(KillLimitKey, KillLimit) && KillLimit == CheeseStrikeRules::KillLimit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASearchLooksForThisGame, "CheeseStrike.Sessions.ASearchLooksForThisGame", SessionTests::Flags)

bool FASearchLooksForThisGame::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSessions;
	const TSharedRef<FOnlineSessionSearch> Steam = Search(false);
	TestEqual(TEXT("plenty of results"), Steam->MaxSearchResults, MaxResults);
	TestFalse(TEXT("over the service"), Steam->bIsLanQuery);
	FString Game;
	TestTrue(TEXT("for this game only"), Steam->QuerySettings.Get(GameKey, Game) && Game == GameValue);
	bool bLobbies = false;
	TestTrue(TEXT("among lobbies"), Steam->QuerySettings.Get(SEARCH_LOBBIES, bLobbies) && bLobbies);
	const TSharedRef<FOnlineSessionSearch> Lan = Search(true);
	TestTrue(TEXT("or over the local network"), Lan->bIsLanQuery);
	TestFalse(TEXT("where there are no lobbies"), Lan->QuerySettings.Get(SEARCH_LOBBIES, bLobbies));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheHostTravelsToTheLevelListening, "CheeseStrike.Sessions.TheHostTravelsToTheLevelListening", SessionTests::Flags)

bool FTheHostTravelsToTheLevelListening::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSessions;
	TestEqual(TEXT("with options"), TravelUrl(TEXT("?KillLimit=5")), FString(Level) + TEXT("?listen?KillLimit=5"));
	TestEqual(TEXT("without"), TravelUrl(TEXT("")), FString(Level) + TEXT("?listen"));
	TestEqual(TEXT("options get their question mark"), NormalisedOptions(TEXT("KillLimit=5")), FString(TEXT("?KillLimit=5")));
	TestEqual(TEXT("and keep it"), NormalisedOptions(TEXT(" ?KillLimit=5 ")), FString(TEXT("?KillLimit=5")));
	TestEqual(TEXT("or stay empty"), NormalisedOptions(TEXT("  ")), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFoundMatchIsDescribed, "CheeseStrike.Sessions.AFoundMatchIsDescribed", SessionTests::Flags)

bool FAFoundMatchIsDescribed::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSessions;
	FOnlineSessionSearchResult Result;
	Result.Session.OwningUserName = TEXT("Ash");
	Result.Session.SessionSettings = HostSettings(TEXT("?KillLimit=10?Map=0"), false);
	Result.Session.NumOpenPublicConnections = MaxPlayers - 3;
	Result.PingInMs = 42;
	TestTrue(TEXT("it is ours"), IsOurs(Result));
	TestEqual(TEXT("and reads"), Describe(Result), FString::Printf(TEXT("Ash's game, 3 of 16 in, first to 10 kills on %s, 42 ms"), CheeseStrikeRules::Maps[0].Name));
	Result.Session.SessionSettings = HostSettings(TEXT("?KillLimit=10"), false);
	TestEqual(TEXT("without a map picked, the level it is hosted on"), Describe(Result), FString::Printf(TEXT("Ash's game, 3 of 16 in, first to 10 kills on %s, 42 ms"), CheeseStrikeRules::Maps[CheeseStrikeRules::MapIndexFor(Level)].Name));

	FOnlineSessionSearchResult Other;
	Other.Session.SessionSettings.NumPublicConnections = 4;
	Other.Session.NumOpenPublicConnections = 4;
	TestFalse(TEXT("another project's match on the test app id isn't"), IsOurs(Other));
	TestEqual(TEXT("and an unknown host is still described"), Describe(Other), FString::Printf(TEXT("someone's game, 0 of 4 in, first to %d kills"), CheeseStrikeRules::KillLimit));
	return true;
}

#endif
