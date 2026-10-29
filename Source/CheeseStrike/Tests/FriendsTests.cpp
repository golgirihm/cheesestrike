// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The friends panel's rules, as plain functions: which friends' games can be
// joined from here, who may invite and who may kick.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Sessions/CheeseStrikeSessions.h"

namespace FriendsTests
{
	using namespace CheeseStrikeFriends;
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFriendsPanelSaysWhatCanBeJoined, "CheeseStrike.Friends.WhatCanBeJoined", FriendsTests::Flags)

bool FTheFriendsPanelSaysWhatCanBeJoined::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	TestEqual(TEXT("not in a game: nothing to join"), Classify(false, 0, true, false), EState::NotPlaying);
	TestEqual(TEXT("in a game with friends+invite and room: join"), Classify(true, 3, true, false), EState::Join);
	TestEqual(TEXT("in a game with room and invite only: no, unless invited"), Classify(true, 3, false, false), EState::InviteOnly);
	TestEqual(TEXT("invited: join even when invite only"), Classify(true, 3, false, true), EState::Join);
	TestEqual(TEXT("full with friends+invite: can't join"), Classify(true, MaxHumans, true, false), EState::Full);
	TestEqual(TEXT("full, invite only: can't join"), Classify(true, MaxHumans, false, false), EState::Full);
	TestEqual(TEXT("full, even invited"), Classify(true, MaxHumans, true, true), EState::Full);
	TestEqual(TEXT("fifteen humans and bots is still room"), Classify(true, MaxHumans - 1, true, false), EState::Join);
	TestEqual(TEXT("sixteen humans is full"), Classify(true, MaxHumans, true, false), EState::Full);
	TestEqual(TEXT("with its count, as the panel shows it"), StateLine(EState::Join, 5), FString(TEXT("Join (5 of 16)")));
	TestEqual(TEXT("not in a game"), StateLine(EState::NotPlaying, 0), FString(TEXT("Not in a game")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnyoneInAGameInvitesAndOnlyTheHostKicks, "CheeseStrike.Friends.InvitesAndKicks", FriendsTests::Flags)

bool FAnyoneInAGameInvitesAndOnlyTheHostKicks::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	TestTrue(TEXT("a joiner in a lobby invites a friend"), CanInvite(true, false));
	TestFalse(TEXT("not to one who is in it already"), CanInvite(true, true));
	TestFalse(TEXT("not from outside a game"), CanInvite(false, false));
	TestTrue(TEXT("the host kicks a joiner"), CanKick(true, false));
	TestFalse(TEXT("a joiner kicks nobody"), CanKick(false, false));
	TestFalse(TEXT("the host doesn't kick themself"), CanKick(true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFriendsMayJoinIsTheHostsChoice, "CheeseStrike.Friends.TheHostsChoice", FriendsTests::Flags)

bool FFriendsMayJoinIsTheHostsChoice::RunTest(const FString& Parameters)
{
	const FOnlineSessionSettings Settings = CheeseStrikeSessions::HostSettings(TEXT(""), false);
	TestTrue(TEXT("friends may join a new game"), CheeseStrikeSessions::FriendsMayJoin(Settings));
	const FOnlineSessionSettings Invite = CheeseStrikeSessions::WithFriendsMayJoin(Settings, false);
	TestFalse(TEXT("the host can shut them out: invitations only"), CheeseStrikeSessions::FriendsMayJoin(Invite));
	TestFalse(TEXT("so the game is not joinable through a friend's presence"), Invite.bAllowJoinViaPresence);
	TestTrue(TEXT("and the host can let them back in"), CheeseStrikeSessions::FriendsMayJoin(CheeseStrikeSessions::WithFriendsMayJoin(Invite, true)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFriendsAreListedInSections, "CheeseStrike.Friends.Sections", FriendsTests::Flags)

bool FFriendsAreListedInSections::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	const EPermission Open = EPermission::InvitesAndFriends;
	const EPermission Shut = EPermission::InviteOnly;
	TestEqual(TEXT("three sections"), NumSections, 3);
	TestEqual(TEXT("in our own lobby: our party, whatever else"), SectionFor(true, EStatus::InLobby, Shut, false), ESection::YourParty);
	TestEqual(TEXT("a lobby that lets friends in, with room: joinable"), SectionFor(false, EStatus::InLobby, Open, true), ESection::Joinable);
	TestEqual(TEXT("a match that lets friends in, with room: joinable"), SectionFor(false, EStatus::InMatch, Open, true), ESection::Joinable);
	TestEqual(TEXT("invite only: others"), SectionFor(false, EStatus::InLobby, Shut, true), ESection::Others);
	TestEqual(TEXT("no room: others"), SectionFor(false, EStatus::InLobby, Open, false), ESection::Others);
	TestEqual(TEXT("on the menu: others"), SectionFor(false, EStatus::MainMenu, Open, true), ESection::Others);
	TestEqual(TEXT("not running the game: others"), SectionFor(false, EStatus::NotInGame, Open, true), ESection::Others);
	TestEqual(TEXT("the sections' names"), SectionTitle(ESection::YourParty) + SectionTitle(ESection::Joinable) + SectionTitle(ESection::Others), FString(TEXT("Your partyJoinableOthers")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheOthersAreInOrder, "CheeseStrike.Friends.OthersOrder", FriendsTests::Flags)

bool FTheOthersAreInOrder::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	// Online and playing CheeseStrike, then online in another game, then online, then away (or busy), then offline.
	const int32 Ours = OthersRank(EOnline::Online, EPlaying::CheeseStrike);
	const int32 Another = OthersRank(EOnline::Online, EPlaying::AnotherGame);
	const int32 Idle = OthersRank(EOnline::Online, EPlaying::Nothing);
	const int32 Away = OthersRank(EOnline::Away, EPlaying::Nothing);
	const int32 Offline = OthersRank(EOnline::Offline, EPlaying::Nothing);
	TestTrue(TEXT("playing CheeseStrike before another game"), Ours < Another);
	TestTrue(TEXT("another game before online"), Another < Idle);
	TestTrue(TEXT("online before away"), Idle < Away);
	TestTrue(TEXT("away before offline"), Away < Offline);
	TestEqual(TEXT("busy is as away"), OthersRank(EOnline::Busy, EPlaying::Nothing), Away);
	TestEqual(TEXT("offline is last whatever Steam last said they played"), OthersRank(EOnline::Offline, EPlaying::CheeseStrike), Offline);
	TestTrue(TEXT("away in CheeseStrike is still playing it"), OthersRank(EOnline::Away, EPlaying::CheeseStrike) < Another);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFriendsStatusAndPermissionRoundTrip, "CheeseStrike.Friends.StatusAndPermission", FriendsTests::Flags)

bool FAFriendsStatusAndPermissionRoundTrip::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	for (const EStatus Each : { EStatus::MainMenu, EStatus::InLobby, EStatus::InMatch })
	{
		TestEqual(TEXT("a published status reads back"), StatusFromValue(StatusValue(Each), true), Each);
	}
	TestEqual(TEXT("running the game with nothing published: the menu"), StatusFromValue(FString(), true), EStatus::MainMenu);
	TestEqual(TEXT("a stale value from a friend not running the game: not in game"), StatusFromValue(TEXT("Lobby"), false), EStatus::NotInGame);
	TestEqual(TEXT("invite only reads back"), PermissionFromValue(PermissionValue(EPermission::InviteOnly)), EPermission::InviteOnly);
	TestEqual(TEXT("invites + friends reads back"), PermissionFromValue(PermissionValue(EPermission::InvitesAndFriends)), EPermission::InvitesAndFriends);
	TestEqual(TEXT("no value: friends may join, as a new game"), PermissionFromValue(FString()), EPermission::InvitesAndFriends);
	TestEqual(TEXT("invite only says what it means"), PermissionDescription(EPermission::InviteOnly), FString(TEXT("Only invitations let players in")));
	TestEqual(TEXT("and so do invites + friends"), PermissionDescription(EPermission::InvitesAndFriends), FString(TEXT("Friends can join")));
	TestFalse(TEXT("a host that picked invite only shows it, whatever the session still says"), FriendsMayJoinShown(true, TOptional<bool>(false), true));
	TestTrue(TEXT("a host that picked friends shows it"), FriendsMayJoinShown(true, TOptional<bool>(true), false));
	TestFalse(TEXT("a host that hasn't picked shows the session's"), FriendsMayJoinShown(true, TOptional<bool>(), false));
	TestFalse(TEXT("a joiner shows the session's, not a stale pick"), FriendsMayJoinShown(false, TOptional<bool>(true), false));
	TestTrue(TEXT("fifteen humans: room"), HasSpace(MaxHumans - 1));
	TestFalse(TEXT("sixteen humans: none"), HasSpace(MaxHumans));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFriendsTooltipSaysWhatSteamSays, "CheeseStrike.Friends.Tooltip", FriendsTests::Flags)

bool FAFriendsTooltipSaysWhatSteamSays::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	TestEqual(TEXT("in a lobby: everything"), Tooltip(EOnline::Online, EPlaying::CheeseStrike, EStatus::InLobby, EPermission::InvitesAndFriends, true, 7),
		FString(TEXT("Steam: Online\nPlaying: CheeseStrike\nStatus: In a lobby\nParty permission: Invites + friends\nSpace in party: Yes\nPlayers: 7/16")));
	TestEqual(TEXT("in another game: N/A for the party"), Tooltip(EOnline::Away, EPlaying::AnotherGame, EStatus::NotInGame, EPermission::InviteOnly, true, -1),
		FString(TEXT("Steam: Away\nPlaying: another game\nStatus: Not playing CheeseStrike\nParty permission: N/A\nSpace in party: N/A\nPlayers: N/A")));
	const FString Offline = Tooltip(EOnline::Offline, EPlaying::AnotherGame, EStatus::NotInGame, EPermission::InvitesAndFriends, true, -1);
	TestTrue(TEXT("offline, alone: Steam gives no time last online"), Offline.StartsWith(TEXT("Steam: Offline\n")));
	TestTrue(TEXT("offline plays nothing"), Offline.Contains(TEXT("Playing: nothing")));
	TestTrue(TEXT("on the menu: no party"), Tooltip(EOnline::Busy, EPlaying::CheeseStrike, EStatus::MainMenu, EPermission::InvitesAndFriends, true, -1).Contains(TEXT("Steam: Busy\nPlaying: CheeseStrike\nStatus: Main menu\nParty permission: N/A")));
	TestFalse(TEXT("nothing about playing before"), Offline.Contains(TEXT("Played before")));
	TestTrue(TEXT("a full match"), Tooltip(EOnline::Online, EPlaying::CheeseStrike, EStatus::InMatch, EPermission::InviteOnly, false, MaxHumans).EndsWith(TEXT("Party permission: Invite only\nSpace in party: No\nPlayers: 16/16")));
	// INVITE: only from a game of ours, and only to someone online.
	TestTrue(TEXT("invite an online friend from a lobby"), WhyNoInvite(true, EOnline::Online).IsEmpty());
	TestTrue(TEXT("or one away"), WhyNoInvite(true, EOnline::Away).IsEmpty());
	TestFalse(TEXT("not one offline"), WhyNoInvite(true, EOnline::Offline).IsEmpty());
	TestFalse(TEXT("not from the front end"), WhyNoInvite(false, EOnline::Online).IsEmpty());
	TestNotEqual(TEXT("each says its own why"), WhyNoInvite(true, EOnline::Offline), WhyNoInvite(false, EOnline::Online));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFriendsRowSaysWhereTheyAre, "CheeseStrike.Friends.RowSaysWhereTheyAre", FriendsTests::Flags)

bool FAFriendsRowSaysWhereTheyAre::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	const EOnline On = EOnline::Online;
	const EPlaying Ours = EPlaying::CheeseStrike;
	TestEqual(TEXT("a friend in a lobby that can be joined: how full it is"), RowStatus(EState::Join, EStatus::InLobby, 3, On, Ours), FString(TEXT("Join (3 of 16)")));
	TestEqual(TEXT("in a full match: full"), RowStatus(EState::Full, EStatus::InMatch, MaxHumans, On, Ours), FString(TEXT("Full (16 of 16)")));
	TestEqual(TEXT("in an invite-only match: where they are"), RowStatus(EState::InviteOnly, EStatus::InMatch, 4, On, Ours), FString(TEXT("In a match")));
	TestEqual(TEXT("on the menu"), RowStatus(EState::NotPlaying, EStatus::MainMenu, -1, EOnline::Away, Ours), FString(TEXT("Main menu")));
	TestEqual(TEXT("in another game"), RowStatus(EState::NotPlaying, EStatus::NotInGame, -1, On, EPlaying::AnotherGame), FString(TEXT("Playing another game")));
	TestEqual(TEXT("online, playing nothing"), RowStatus(EState::NotPlaying, EStatus::NotInGame, -1, On, EPlaying::Nothing), FString(TEXT("Online")));
	TestEqual(TEXT("away"), RowStatus(EState::NotPlaying, EStatus::NotInGame, -1, EOnline::Away, EPlaying::Nothing), FString(TEXT("Away")));
	TestEqual(TEXT("offline"), RowStatus(EState::NotPlaying, EStatus::NotInGame, -1, EOnline::Offline, EPlaying::Nothing), FString(TEXT("Offline")));
	// Each status its own dot, and offline the dimmest.
	const TArray<FLinearColor> All = { StatusDotColour(EStatus::InMatch, On), StatusDotColour(EStatus::InLobby, On), StatusDotColour(EStatus::MainMenu, On),
		StatusDotColour(EStatus::NotInGame, On), StatusDotColour(EStatus::NotInGame, EOnline::Away), StatusDotColour(EStatus::NotInGame, EOnline::Offline) };
	for (int32 One = 0; One < All.Num(); ++One)
	{
		for (int32 Other = One + 1; Other < All.Num(); ++Other)
		{
			TestFalse(TEXT("every status has a dot of its own colour"), All[One].Equals(All[Other], 0.05f));
		}
		if (One != All.Num() - 1)
		{
			TestTrue(TEXT("offline is the dimmest"), All[One].GetLuminance() > All.Last().GetLuminance());
		}
	}
	TestTrue(TEXT("busy's dot is away's"), StatusDotColour(EStatus::NotInGame, EOnline::Busy).Equals(StatusDotColour(EStatus::NotInGame, EOnline::Away)));
	TestFalse(TEXT("an empty section says so"), EmptySectionLine().IsEmpty());
	TestEqual(TEXT("initials of two words"), Initials(TEXT("gorgon zola")), FString(TEXT("GZ")));
	TestEqual(TEXT("of one"), Initials(TEXT("Brie")), FString(TEXT("B")));
	TestEqual(TEXT("never more than two"), Initials(TEXT("The Big Cheese")), FString(TEXT("TB")));
	TestEqual(TEXT("none for no name"), Initials(TEXT("  ")), FString(TEXT("?")));
	return true;
}
