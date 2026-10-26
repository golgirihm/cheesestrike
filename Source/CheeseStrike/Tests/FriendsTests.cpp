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
