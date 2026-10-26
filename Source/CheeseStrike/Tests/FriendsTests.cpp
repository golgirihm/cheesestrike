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
	TestEqual(TEXT("in our own lobby: our party, whatever else"), SectionFor(true, EStatus::InLobby, Shut, false, true), ESection::YourParty);
	TestEqual(TEXT("a lobby that lets friends in, with room: joinable"), SectionFor(false, EStatus::InLobby, Open, true, true), ESection::Joinable);
	TestEqual(TEXT("a match that lets friends in, with room: joinable"), SectionFor(false, EStatus::InMatch, Open, true, true), ESection::Joinable);
	TestEqual(TEXT("invite only: not joinable, but can be invited"), SectionFor(false, EStatus::InLobby, Shut, true, true), ESection::Invitable);
	TestEqual(TEXT("no room: not joinable"), SectionFor(false, EStatus::InLobby, Open, false, true), ESection::Invitable);
	TestEqual(TEXT("on the menu: invitable"), SectionFor(false, EStatus::MainMenu, Open, true, true), ESection::Invitable);
	TestEqual(TEXT("not running the game, not known to own it: others"), SectionFor(false, EStatus::NotInGame, Open, true, false), ESection::Others);
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
	TestTrue(TEXT("running our app: owns it"), OwnsTheGame(true, false));
	TestTrue(TEXT("publishing our status: owns it"), OwnsTheGame(false, true));
	TestFalse(TEXT("neither: not known to"), OwnsTheGame(false, false));
	TestTrue(TEXT("fifteen humans: room"), HasSpace(MaxHumans - 1));
	TestFalse(TEXT("sixteen humans: none"), HasSpace(MaxHumans));
	TestEqual(TEXT("the tooltip counts the players"), Tooltip(true, EStatus::InLobby, EPermission::InvitesAndFriends, true, 7),
		FString(TEXT("Owns the game: Yes\nStatus: In a lobby\nParty permission: Invites + friends\nSpace in party: Yes\nPlayers: 7/16")));
	TestTrue(TEXT("the tooltip says not in game"), Tooltip(false, EStatus::NotInGame, EPermission::InviteOnly, true, -1).Contains(TEXT("Status: Not in game")));
	return true;
}
