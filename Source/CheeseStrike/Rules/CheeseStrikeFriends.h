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
	/** The words for a status: "Not in game", "Main menu", "In a lobby", "In a match". */
	FString StatusLabel(EStatus Status);

	/** The value published for a permission ("Invite" or "Friends"); anything but "Invite" is Invites + friends (the default). */
	FString PermissionValue(EPermission Permission);
	EPermission PermissionFromValue(const FString& Value);
	/** "Invite only" or "Invites + friends". */
	FString PermissionLabel(EPermission Permission);

	/**
	 * Whether a friend owns the game. Steam can't report that for a friend, so this is a guess: running our app or publishing our
	 * presence says yes; anyone else is unknown, and treated as not owning it.
	 */
	bool OwnsTheGame(bool bRunningThisGame, bool bPublishesStatus);

	/** Whether a party with `Humans` players (-1 when not known) has room for one more. */
	bool HasSpace(int32 Humans);

	/** The collapsible sections of the friends panel, in the order shown. */
	enum class ESection : uint8
	{
		/** In the lobby or match we are in. */
		YourParty,
		/** In a lobby or match that lets friends in and has room: a click joins. */
		Joinable,
		/** Owns the game, and can be invited. */
		Invitable,
		/** Everyone else. */
		Others,
	};
	constexpr int32 NumSections = 4;

	/** The section a friend is listed in; the first that fits, in the order of ESection. */
	ESection SectionFor(bool bInMyParty, EStatus Status, EPermission Permission, bool bHasSpace, bool bOwns);
	/** The header of a section. */
	FString SectionTitle(ESection Section);

	/** What the hover tooltip of a friend's row says. `Humans` is -1 when not known. */
	FString Tooltip(bool bOwns, EStatus Status, EPermission Permission, bool bHasSpace, int32 Humans);
}
