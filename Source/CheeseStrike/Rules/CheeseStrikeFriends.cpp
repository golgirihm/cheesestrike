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

	const TCHAR* const PresenceKey = TEXT("cs_status");
	const TCHAR* const PermissionKey = TEXT("cs_access");
	const TCHAR* const HumansKey = TEXT("cs_humans");

	FString StatusValue(EStatus Status)
	{
		switch (Status)
		{
		case EStatus::MainMenu: return TEXT("Menu");
		case EStatus::InLobby: return TEXT("Lobby");
		case EStatus::InMatch: return TEXT("Match");
		default: return FString();
		}
	}

	EStatus StatusFromValue(const FString& Value, bool bRunningThisGame)
	{
		if (!bRunningThisGame)
		{
			return EStatus::NotInGame;
		}
		if (Value == TEXT("Lobby"))
		{
			return EStatus::InLobby;
		}
		if (Value == TEXT("Match"))
		{
			return EStatus::InMatch;
		}
		return EStatus::MainMenu;
	}

	FString StatusLabel(EStatus Status)
	{
		switch (Status)
		{
		case EStatus::MainMenu: return TEXT("Main menu");
		case EStatus::InLobby: return TEXT("In a lobby");
		case EStatus::InMatch: return TEXT("In a match");
		default: return TEXT("Not in game");
		}
	}

	FString RowStatus(EState State, EStatus Status, int32 Humans)
	{
		const bool bInAParty = Status == EStatus::InLobby || Status == EStatus::InMatch;
		return bInAParty && (State == EState::Join || State == EState::Full) ? StateLine(State, Humans) : StatusLabel(Status);
	}

	FLinearColor StatusDotColour(EStatus Status)
	{
		switch (Status)
		{
		case EStatus::InMatch: return FLinearColor(0.3f, 0.82f, 0.36f);
		case EStatus::InLobby: return FLinearColor(0.28f, 0.6f, 1.0f);
		case EStatus::MainMenu: return FLinearColor(0.96f, 0.72f, 0.2f);
		default: return FLinearColor(0.38f, 0.37f, 0.36f);
		}
	}

	FString EmptySectionLine()
	{
		return TEXT("No one here");
	}

	FString Initials(const FString& Name)
	{
		TArray<FString> Words;
		Name.TrimStartAndEnd().ParseIntoArrayWS(Words);
		FString Letters;
		for (int32 Index = 0; Index < Words.Num() && Letters.Len() < 2; ++Index)
		{
			Letters.AppendChar(FChar::ToUpper(Words[Index][0]));
		}
		return Letters.IsEmpty() ? FString(TEXT("?")) : Letters;
	}

	FString PermissionValue(EPermission Permission)
	{
		return Permission == EPermission::InviteOnly ? TEXT("Invite") : TEXT("Friends");
	}

	EPermission PermissionFromValue(const FString& Value)
	{
		return Value == TEXT("Invite") ? EPermission::InviteOnly : EPermission::InvitesAndFriends;
	}

	bool FriendsMayJoinShown(bool bHosting, const TOptional<bool>& HostChoice, bool bSession)
	{
		return bHosting && HostChoice.IsSet() ? HostChoice.GetValue() : bSession;
	}

	FString PermissionLabel(EPermission Permission)
	{
		return Permission == EPermission::InviteOnly ? TEXT("Invite only") : TEXT("Invites + friends");
	}

	FString PermissionDescription(EPermission Permission)
	{
		return Permission == EPermission::InviteOnly ? TEXT("Only invitations let players in") : TEXT("Friends can join");
	}

	bool PlayedBefore(bool bRunningThisGame, bool bPublishesStatus)
	{
		return bRunningThisGame || bPublishesStatus;
	}

	bool HasSpace(int32 Humans)
	{
		return Humans < MaxHumans;
	}

	ESection SectionFor(bool bInMyParty, EStatus Status, EPermission Permission, bool bHasSpace, bool bOwns)
	{
		if (bInMyParty)
		{
			return ESection::YourParty;
		}
		const bool bInAParty = Status == EStatus::InLobby || Status == EStatus::InMatch;
		if (bInAParty && Permission == EPermission::InvitesAndFriends && bHasSpace)
		{
			return ESection::Joinable;
		}
		return bOwns ? ESection::Invitable : ESection::Others;
	}

	FString SectionTitle(ESection Section)
	{
		switch (Section)
		{
		case ESection::YourParty: return TEXT("Your party");
		case ESection::Joinable: return TEXT("Joinable");
		case ESection::Invitable: return TEXT("Invitable");
		default: return TEXT("Others");
		}
	}

	FString Tooltip(bool bOwns, EStatus Status, EPermission Permission, bool bHasSpace, int32 Humans)
	{
		const bool bInAParty = Status == EStatus::InLobby || Status == EStatus::InMatch;
		const FString Players = bInAParty && Humans >= 0 ? FString::Printf(TEXT("%d/%d"), Humans, MaxHumans) : FString(TEXT("-"));
		return FString::Printf(TEXT("Played before: %s\nStatus: %s\nParty permission: %s\nSpace in party: %s\nPlayers: %s"),
			bOwns ? TEXT("Yes") : TEXT("No"),
			*StatusLabel(Status),
			bInAParty ? *PermissionLabel(Permission) : TEXT("None"),
			bInAParty && bHasSpace ? TEXT("Yes") : TEXT("No"),
			*Players);
	}
}
