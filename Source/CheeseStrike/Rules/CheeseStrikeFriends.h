// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The friends panel's rules, as plain functions (no world, no service):
 * whether a friend's game can be joined from here, what the panel says
 * about each friend, who may invite whom and who may kick. There is no
 * public search: a match is joined only through a friend, or on an invite.
 */
namespace CheeseStrikeFriends
{
	/** A match with this many human players in it is full (bots don't count): the design's sixteen. */
	constexpr int32 MaxHumans = 16;

	/** What a friend's game is, from here, as the panel says it. */
	enum class EState : uint8
	{
		/** Not in a game of this one: nothing to join. */
		NotPlaying,
		/** In a game with no room left: nothing to join. */
		Full,
		/** In a game with room, but it can be joined only on an invite, and we have none. */
		InviteOnly,
		/** In a game with room that can be joined: the friend's lobby or match. */
		Join,
	};

	/**
	 * `bInGame`: the friend is in a lobby or match of this game. `Humans`:
	 * the players in it, bots not counted. `bFriendsMayJoin`: the friend's
	 * game lets their friends in (else only invitees). `bInvited`: this player
	 * has an invite to it.
	 */
	EState Classify(bool bInGame, int32 Humans, bool bFriendsMayJoin, bool bInvited);

	/** Whether a player in a lobby or a match may invite a friend to it: anyone in the game, not just its host. */
	bool CanInvite(bool bInGame, bool bFriendAlreadyIn);

	/** Whether a player may kick another from the game: only the host, and never themself. */
	bool CanKick(bool bHost, bool bSelf);

	/** The line the panel shows under a friend's name for `State`, with `Humans` in the game. */
	FString StateLine(EState State, int32 Humans);

	/** What a friend is doing, as the friend publishes it (Steam rich presence, key PresenceKey). */
	enum class EStatus : uint8
	{
		/** Not running the game. */
		NotInGame,
		/** On the front end. */
		MainMenu,
		/** In a lobby. */
		InLobby,
		/** In a match. */
		InMatch,
	};

	/** Who may join a party: set by its host, and the same for every player in it. */
	enum class EPermission : uint8
	{
		InviteOnly,
		InvitesAndFriends,
	};

	/** The rich presence keys a player publishes: where they are, their party's permission, and the humans in it. */
	extern const TCHAR* const PresenceKey;
	extern const TCHAR* const PermissionKey;
	extern const TCHAR* const HumansKey;

	/** The value published for `Status` ("Menu", "Lobby" or "Match"; empty for NotInGame). */
	FString StatusValue(EStatus Status);
	/**
	 * What a friend's published value means. A friend running this game with no value yet is on the menu; one not running it
	 * is not in the game whatever stale value is left.
	 */
	EStatus StatusFromValue(const FString& Value, bool bRunningThisGame);
	/** The words for a status: "Not playing CheeseStrike", "Main menu", "In a lobby", "In a match". */
	FString StatusLabel(EStatus Status);

	/**
	 * Whether the permission shown says friends may join: a host shows its own choice (`HostChoice`, kept once it picks, since the
	 * session's settings are changed by an update that is not done yet, or are not kept at all with no session), else the session's
	 * (`bSession`, which is true with no session, the default).
	 */
	bool FriendsMayJoinShown(bool bHosting, const TOptional<bool>& HostChoice, bool bSession);
	/** The value published for a permission ("Invite" or "Friends"); anything but "Invite" is Invites + friends (the default). */
	FString PermissionValue(EPermission Permission);
	EPermission PermissionFromValue(const FString& Value);
	/** "Invite only" or "Invites + friends". */
	FString PermissionLabel(EPermission Permission);
	/** What a permission means, under the party's dropdown while it is chosen: "Only invitations let players in" or "Friends can join". */
	FString PermissionDescription(EPermission Permission);

	/** Where a friend is on Steam (its persona state: Snooze counts as Away, Looking to play/trade as Online). */
	enum class EOnline : uint8
	{
		Online,
		Away,
		Busy,
		Offline,
	};
	/** "Online", "Away", "Busy" or "Offline". */
	FString OnlineLabel(EOnline Online);

	/** What a friend is playing, as Steam says (the app id of their game: ours, another, or none). */
	enum class EPlaying : uint8
	{
		Nothing,
		CheeseStrike,
		/** Another game: Steam no longer gives other apps' names, so it is only "another game". */
		AnotherGame,
	};
	/** "CheeseStrike", "another game" or "nothing". */
	FString PlayingLabel(EPlaying Playing);

	/** Whether a party with `Humans` players (-1 when not known) has room for one more. */
	bool HasSpace(int32 Humans);

	/** The collapsible sections of the friends panel, in the order shown, all open to begin with. */
	enum class ESection : uint8
	{
		/** Ourself first, then everyone in the lobby or match we are in. */
		YourParty,
		/** In a lobby or match that lets friends in and has room: JOIN. */
		Joinable,
		/** Everyone else, in the order of OthersRank. */
		Others,
	};
	constexpr int32 NumSections = 3;

	/** The section a friend is listed in; the first that fits, in the order of ESection. */
	ESection SectionFor(bool bInMyParty, EStatus Status, EPermission Permission, bool bHasSpace);
	/** The header of a section. */
	FString SectionTitle(ESection Section);
	/**
	 * Where a friend comes in Others, the lower first: online and playing CheeseStrike (0), online in another game (1), online
	 * (2), away or busy (3), offline (4). Friends of the same rank are listed by name.
	 */
	int32 OthersRank(EOnline Online, EPlaying Playing);

	/**
	 * What the hover tooltip of a friend's row says, a line each: "Steam: Online" (Steam gives no time last online for a friend,
	 * so offline is "Offline" alone), "Playing: CheeseStrike / another game / nothing", "Status: ..." (StatusLabel), and the party's
	 * permission, space and players, "N/A" while they are not in a lobby or match. `Humans` is -1 when not known.
	 */
	FString Tooltip(EOnline Online, EPlaying Playing, EStatus Status, EPermission Permission, bool bHasSpace, int32 Humans);
	/**
	 * Why INVITE can't be pressed for a friend, for its tooltip; empty when it can. We must be in a lobby or match
	 * (`bWeAreInAGame`) and they online.
	 */
	FString WhyNoInvite(bool bWeAreInAGame, EOnline Them);

	/**
	 * The line under a friend's name, in a softer colour: their game's state while they are in a lobby or match that can be
	 * joined or is full ("Join (3 of 16)", "Full (16 of 16)"), what they are doing in CheeseStrike ("In a lobby", "Main menu"),
	 * and otherwise where they are on Steam ("Playing another game", "Online", "Away", "Offline").
	 */
	FString RowStatus(EState State, EStatus Status, int32 Humans, EOnline Online, EPlaying Playing);
	/**
	 * The colour of the dot before that line: in a match green, in a lobby blue, on the menu cheese; not in the game, a pale grey
	 * online, a dull amber away or busy, and dark grey offline.
	 */
	FLinearColor StatusDotColour(EStatus Status, EOnline Online);
	/** What a section with nobody in it says, faintly, so that it still looks meant. */
	FString EmptySectionLine();
	/** The letters shown for a friend with no picture: the first of their name's first two words, in capitals ("?" for none). */
	FString Initials(const FString& Name);
}
