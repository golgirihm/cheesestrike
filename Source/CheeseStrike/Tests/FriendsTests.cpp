// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The friends panel's rules, as plain functions: which friends' games can be
// joined from here, who may invite and who may kick.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Sessions/CheeseStrikeSessions.h"
#include "UI/CheeseStrikeMenu.h"

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
	const FLinearColor OnlineDot = StatusDotColour(EStatus::NotInGame, On);
	TestTrue(TEXT("an online friend's dot is green"), OnlineDot.G > 0.7f && OnlineDot.G > OnlineDot.R + 0.2f && OnlineDot.G > OnlineDot.B + 0.4f);
	TestTrue(TEXT("busy's dot is away's"), StatusDotColour(EStatus::NotInGame, EOnline::Busy).Equals(StatusDotColour(EStatus::NotInGame, EOnline::Away)));
	TestFalse(TEXT("an empty section says so"), EmptySectionLine().IsEmpty());
	TestEqual(TEXT("initials of two words"), Initials(TEXT("gorgon zola")), FString(TEXT("GZ")));
	TestEqual(TEXT("of one"), Initials(TEXT("Brie")), FString(TEXT("B")));
	TestEqual(TEXT("never more than two"), Initials(TEXT("The Big Cheese")), FString(TEXT("TB")));
	TestEqual(TEXT("none for no name"), Initials(TEXT("  ")), FString(TEXT("?")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFriendsPanelOpensOnAClick, "CheeseStrike.Friends.PanelOpensOnAClick", FriendsTests::Flags)

bool FTheFriendsPanelOpensOnAClick::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	// Only a click opens or shuts it: the tile opens it, the open panel's chevron or its word FRIENDS shut it; hovering does nothing.
	TestTrue(TEXT("clicked open, it is open"), PanelOpen(false, true));
	TestFalse(TEXT("not clicked open (or clicked shut), it is shut, the mouse over it or not"), PanelOpen(false, false));
	// The find page is about the friends' games: open there whatever else.
	TestTrue(TEXT("always open on the find page"), PanelOpen(true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFriendsListRefreshesWithACooldown, "CheeseStrike.Friends.TheListRefreshesWithACooldown", FriendsTests::Flags)

bool FTheFriendsListRefreshesWithACooldown::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	TestEqual(TEXT("five seconds' rest after a refresh"), RefreshCooldown, 5.0f);
	TestEqual(TEXT("every thirty seconds by itself while open"), AutoRefreshSeconds, 30.0f);
	TestFalse(TEXT("just refreshed: the button rests"), CanRefresh(100.0, 100.0));
	TestEqual(TEXT("with all its cooldown left"), RefreshCooldownLeft(100.0, 100.0), RefreshCooldown);
	TestEqual(TEXT("two seconds on, three left"), RefreshCooldownLeft(102.0, 100.0), 3.0f);
	TestFalse(TEXT("nor a moment before the end"), CanRefresh(104.9, 100.0));
	TestTrue(TEXT("after the cooldown it can be pressed"), CanRefresh(105.0, 100.0));
	TestEqual(TEXT("with nothing left"), RefreshCooldownLeft(200.0, 100.0), 0.0f);
	TestTrue(TEXT("never refreshed: it can be"), CanRefresh(0.0, -1000.0));
	// Opened by a click: a refresh at once (unless one was just made), which starts the cooldown.
	TestTrue(TEXT("opened by a click: a refresh"), ShouldAutoRefresh(true, true, 50.0, 10.0));
	TestFalse(TEXT("opened by a click within the cooldown: none"), ShouldAutoRefresh(true, true, 12.0, 10.0));
	// Open: again every thirty seconds; shut: never by itself.
	TestFalse(TEXT("open, ten seconds on: none"), ShouldAutoRefresh(true, false, 20.0, 10.0));
	TestTrue(TEXT("open, thirty seconds on: one"), ShouldAutoRefresh(true, false, 40.0, 10.0));
	TestFalse(TEXT("shut: none, however long"), ShouldAutoRefresh(false, false, 1000.0, 10.0));
	// The glyph turns once, eased out, and is still after.
	TestEqual(TEXT("not turned at the click"), RefreshSpinAt(0.0f), 0.0f);
	TestTrue(TEXT("partway round"), RefreshSpinAt(RefreshSpinSeconds * 0.5f) > 180.0f && RefreshSpinAt(RefreshSpinSeconds * 0.5f) < 360.0f);
	TestEqual(TEXT("once round"), RefreshSpinAt(RefreshSpinSeconds), 360.0f);
	TestEqual(TEXT("and still after"), RefreshSpinAt(4.0f), 360.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFriendsPanelSlidesOpenAndFades, "CheeseStrike.Friends.PanelSlidesOpenAndFades", FriendsTests::Flags)

bool FTheFriendsPanelSlidesOpenAndFades::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	TestEqual(TEXT("a quarter of a second to open or shut"), PanelSeconds, 0.25f);
	// The width follows the curve from the tab's to the open panel's, never in a step.
	TestEqual(TEXT("shut: the tab's width"), PanelWidthAt(0.0f, 120.0f, 496.0f), 120.0f);
	TestEqual(TEXT("open: the panel's"), PanelWidthAt(1.0f, 120.0f, 496.0f), 496.0f);
	TestEqual(TEXT("halfway: halfway"), PanelWidthAt(0.5f, 120.0f, 496.0f), 308.0f);
	float Last = PanelWidthAt(0.0f, 120.0f, 496.0f);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		const float Width = PanelWidthAt(Step / 20.0f, 120.0f, 496.0f);
		TestTrue(TEXT("each step a little wider, by less than a tenth of the way"), Width > Last && Width - Last < (496.0f - 120.0f) * 0.1f);
		Last = Width;
	}
	TestEqual(TEXT("past the end it is open, not wider"), PanelWidthAt(1.5f, 120.0f, 496.0f), 496.0f);
	// The height and the top move along the same curve: the tile grows into the whole height and up to the top, and back.
	TestEqual(TEXT("shut: the tile's height"), PanelHeightAt(0.0f, 156.0f, 900.0f), 156.0f);
	TestEqual(TEXT("open: the screen's"), PanelHeightAt(1.0f, 156.0f, 900.0f), 900.0f);
	TestEqual(TEXT("halfway: halfway"), PanelHeightAt(0.5f, 156.0f, 900.0f), 528.0f);
	TestEqual(TEXT("shut: the tile's top"), PanelTopAt(0.0f, 40.0f), 40.0f);
	TestEqual(TEXT("open: the screen's top"), PanelTopAt(1.0f, 40.0f), 0.0f);
	TestEqual(TEXT("halfway up"), PanelTopAt(0.5f, 40.0f), 20.0f);
	TestEqual(TEXT("a screen not yet measured never makes it shorter than the tile"), PanelHeightAt(1.0f, 156.0f, 0.0f), 156.0f);
	float LastHeight = PanelHeightAt(0.0f, 156.0f, 900.0f);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		const float Height = PanelHeightAt(Step / 20.0f, 156.0f, 900.0f);
		TestTrue(TEXT("each step a little taller, by less than a tenth of the way"), Height > LastHeight && Height - LastHeight < (900.0f - 156.0f) * 0.1f);
		LastHeight = Height;
	}
	// The content is clear over the first half of an expand and fades in over the second; played backwards it fades out
	// over the first half of a collapse.
	TestEqual(TEXT("clear at the start"), ContentOpacityAt(0.0f), 0.0f);
	TestEqual(TEXT("still clear halfway"), ContentOpacityAt(0.5f), 0.0f);
	TestEqual(TEXT("half seen three quarters of the way"), ContentOpacityAt(0.75f), 0.5f);
	TestEqual(TEXT("whole at the end"), ContentOpacityAt(1.0f), 1.0f);
	TestTrue(TEXT("fading in all the way through the second half"), ContentOpacityAt(0.6f) > ContentOpacityAt(0.55f) && ContentOpacityAt(0.9f) > ContentOpacityAt(0.8f));
	// Collapsing is the expanding played backwards: the tall content stays while the width shrinks, and only shut does
	// the panel fold to its tab.
	TestTrue(TEXT("opening: the content"), ShowsContent(true, 0.0f));
	TestTrue(TEXT("open: the content"), ShowsContent(true, 1.0f));
	TestTrue(TEXT("shutting, still wide: the content"), ShowsContent(false, 0.7f));
	TestTrue(TEXT("shutting, nearly shut: still the content"), ShowsContent(false, 0.05f));
	TestFalse(TEXT("shut: the tab"), ShowsContent(false, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATooltipMovesFromRowToRowInPlace, "CheeseStrike.Friends.TooltipMovesFromRowToRowInPlace", FriendsTests::Flags)

bool FATooltipMovesFromRowToRowInPlace::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeFriends;
	constexpr float Delay = CheeseStrikeMenu::TooltipDelay;
	// The first comes after a rest on a row, fading in.
	TestTrue(TEXT("nothing at once"), TooltipStep(false, true, false, 0.05, 100.0, Delay) == ETooltipStep::None);
	TestTrue(TEXT("after the rest it fades in"), TooltipStep(false, true, false, Delay + 0.01, 100.0, Delay) == ETooltipStep::FadeIn);
	TestTrue(TEXT("it stays while its row is hovered"), TooltipStep(true, true, true, 2.0, 100.0, Delay) == ETooltipStep::Keep);
	// From row A to row B: the card is replaced in place, with no fade and no frame without it.
	TestTrue(TEXT("straight onto the next row: swapped in place"), TooltipStep(true, true, false, 0.0, 100.0, Delay) == ETooltipStep::Swap);
	TestTrue(TEXT("over the gap between the rows it stays"), TooltipStep(true, false, false, 0.0, 100.0, Delay) == ETooltipStep::Keep);
	TestTrue(TEXT("for a moment"), TooltipStep(true, false, false, TooltipLinger * 0.9, 100.0, Delay) == ETooltipStep::Keep);
	TestTrue(TEXT("the moment long enough for a quick move across a gap (a few frames at 60)"), TooltipLinger >= 0.1f && TooltipLinger < Delay);
	TestTrue(TEXT("then goes, the mouse having left the rows"), TooltipStep(true, false, false, TooltipLinger + 0.01, 100.0, Delay) == ETooltipStep::Hide);
	// Just gone (a slower move), the next comes at once and without a fade, as it would have in place.
	TestTrue(TEXT("just after one went, the next at once, not faded"), TooltipStep(false, true, false, 0.0, 0.1, Delay) == ETooltipStep::Swap);
	TestTrue(TEXT("nothing hovered and nothing up: nothing"), TooltipStep(false, false, false, 5.0, 0.1, Delay) == ETooltipStep::None);
	// A whole sweep down a list, a frame at 60 at a time, rows 40 units high with gaps of 6, at 1500 units a second: the
	// card never goes and never fades in again once up.
	bool bShown = false;
	bool bEverGone = false;
	int32 FadeIns = 0;
	int32 HoveredRow = INDEX_NONE;
	int32 ShownRow = INDEX_NONE;
	double ChangedAt = 0.0;
	double HiddenAt = -100.0;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		const double Now = Frame / 60.0;
		// Resting on the first row for the delay, then sweeping down.
		const double Y = Now < 0.5 ? 20.0 : 20.0 + (Now - 0.5) * 1500.0;
		const int32 Row = FMath::FloorToInt32(Y / 46.0);
		const bool bOnRow = FMath::Fmod(Y, 46.0) < 40.0 && Row < 20;
		const int32 Hovered = bOnRow ? Row : INDEX_NONE;
		if (Hovered != HoveredRow)
		{
			HoveredRow = Hovered;
			ChangedAt = Now;
		}
		const ETooltipStep Step = TooltipStep(bShown, Hovered != INDEX_NONE, Hovered == ShownRow, Now - ChangedAt, Now - HiddenAt, Delay);
		if (Step == ETooltipStep::FadeIn || Step == ETooltipStep::Swap)
		{
			FadeIns += Step == ETooltipStep::FadeIn ? 1 : 0;
			bShown = true;
			ShownRow = Hovered;
		}
		else if (Step == ETooltipStep::Hide)
		{
			bEverGone = bEverGone || Row < 20;
			bShown = false;
			ShownRow = INDEX_NONE;
			HiddenAt = Now;
		}
	}
	TestEqual(TEXT("one fade in, the first"), FadeIns, 1);
	TestFalse(TEXT("the card never goes while the mouse sweeps over the rows"), bEverGone);

	// The list's bar stands outside it in the panel's margin (20), so that the list never narrows for it.
	TestTrue(TEXT("the bar fits in the panel's margin"), ScrollBarGap + ScrollBarThickness <= 20.0f && ScrollBarThickness >= 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryClickOnThePanelIsThePanels, "CheeseStrike.Friends.EveryClickOnThePanelIsThePanels", FriendsTests::Flags)

bool FEveryClickOnThePanelIsThePanels::RunTest(const FString& Parameters)
{
	// The box under the whole panel takes a press, a release, a double click and the wheel that nothing in it took, so that
	// none goes on to what is under the panel (whether it would is a matter of Slate's hit test: a manual test, W17-FRI-03).
	const TSharedRef<SCheeseStrikeClickCatcher> Catcher = SNew(SCheeseStrikeClickCatcher) [ SNullWidget::NullWidget ];
	const FGeometry Geometry;
	const FPointerEvent Press(0, 0, FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), TSet<FKey>({EKeys::LeftMouseButton}), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
	const FPointerEvent Release(0, 0, FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
	const FPointerEvent Wheel(0, 0, FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), TSet<FKey>(), EKeys::MouseWheelAxis, -1.0f, FModifierKeysState());
	TestTrue(TEXT("a press is taken"), Catcher->OnMouseButtonDown(Geometry, Press).IsEventHandled());
	TestTrue(TEXT("a release is taken"), Catcher->OnMouseButtonUp(Geometry, Release).IsEventHandled());
	TestTrue(TEXT("a double click is taken"), Catcher->OnMouseButtonDoubleClick(Geometry, Press).IsEventHandled());
	TestTrue(TEXT("the wheel is taken"), Catcher->OnMouseWheel(Geometry, Wheel).IsEventHandled());
	TestTrue(TEXT("and it is hit by Slate's hit test (not hit-test invisible)"), Catcher->GetVisibility().IsHitTestVisible());
	TestTrue(TEXT("a right click too"), Catcher->OnMouseButtonDown(Geometry, FPointerEvent(0, 0, FVector2D::ZeroVector, FVector2D::ZeroVector, TSet<FKey>({EKeys::RightMouseButton}), EKeys::RightMouseButton, 0.0f, FModifierKeysState())).IsEventHandled());
	return true;
}

