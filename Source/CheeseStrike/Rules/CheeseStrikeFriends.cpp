// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeFriends.h"

namespace CheeseStrikeFriends
{
	EState Classify(bool bInGame, int32 Humans, bool bFriendsMayJoin, bool bInvited)
	{
		if (!bInGame)
		{
			return EState::NotPlaying;
		}
		if (Humans >= MaxHumans)
		{
			return EState::Full;
		}
		return bFriendsMayJoin || bInvited ? EState::Join : EState::InviteOnly;
	}

	bool CanInvite(bool bInGame, bool bFriendAlreadyIn)
	{
		return bInGame && !bFriendAlreadyIn;
	}

	bool CanKick(bool bHost, bool bSelf)
	{
		return bHost && !bSelf;
	}

	FString StateLine(EState State, int32 Humans)
	{
		switch (State)
		{
		case EState::NotPlaying:
			return TEXT("Not in a game");
		case EState::Full:
			return FString::Printf(TEXT("Full (%d of %d)"), Humans, MaxHumans);
		case EState::InviteOnly:
			return TEXT("Invite only");
		case EState::Join:
		default:
			return FString::Printf(TEXT("Join (%d of %d)"), Humans, MaxHumans);
		}
	}
}
