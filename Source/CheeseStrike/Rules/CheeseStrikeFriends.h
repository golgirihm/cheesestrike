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
}
