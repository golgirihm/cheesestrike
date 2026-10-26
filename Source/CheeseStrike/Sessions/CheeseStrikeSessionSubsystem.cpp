// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSessionSubsystem.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "UI/CheeseStrikeAvatars.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameDelegates.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "CheeseStrikePlayerController.h"
#include "CheeseStrikeSeatMemory.h"
#include "UObject/UObjectGlobals.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "CheeseStrike.h"
#include "CheeseStrikeSessions.h"

void UCheeseStrikeSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IOnlineSessionPtr Interface = Sessions())
	{
		CreatedHandle = Interface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnCreated));
		FoundHandle = Interface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnFound));
		JoinedHandle = Interface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnJoined));
		InviteHandle = Interface->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnInviteAccepted));
	}
	LoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCheeseStrikeSessionSubsystem::OnLevelLoaded);
	if (GEngine != nullptr)
	{
		TravelFailedHandle = GEngine->OnTravelFailure().AddUObject(this, &UCheeseStrikeSessionSubsystem::OnTravelFailed);
		NetworkFailedHandle = GEngine->OnNetworkFailure().AddUObject(this, &UCheeseStrikeSessionSubsystem::OnNetworkFailed);
	}
	ExitHandle = FGameDelegates::Get().GetExitCommandDelegate().AddUObject(this, &UCheeseStrikeSessionSubsystem::OnExit);
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: through %s"), *ServiceName());
	// The game's traffic goes through Steam's net driver only when Steam is
	// the service; without it (the editor, -NOSTEAM, no Steam client) that
	// driver can't connect even to an address, so the plain IP driver takes over.
	if (ServiceName() != TEXT("STEAM") && GEngine != nullptr)
	{
		for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
		{
			if (Definition.DefName == NAME_GameNetDriver)
			{
				Definition.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
			}
		}
	}
}

void UCheeseStrikeSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Interface = Sessions())
	{
		Interface->ClearOnCreateSessionCompleteDelegate_Handle(CreatedHandle);
		Interface->ClearOnFindSessionsCompleteDelegate_Handle(FoundHandle);
		Interface->ClearOnJoinSessionCompleteDelegate_Handle(JoinedHandle);
		Interface->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadedHandle);
	FGameDelegates::Get().GetExitCommandDelegate().Remove(ExitHandle);
	if (GEngine != nullptr)
	{
		GEngine->OnTravelFailure().Remove(TravelFailedHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailedHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr UCheeseStrikeSessionSubsystem::Sessions() const
{
	const IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
	return Service != nullptr ? Service->GetSessionInterface() : nullptr;
}

FString UCheeseStrikeSessionSubsystem::ServiceName() const
{
	const IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
	return Service != nullptr ? Service->GetSubsystemName().ToString() : TEXT("none");
}

bool UCheeseStrikeSessionSubsystem::IsInAMatch() const
{
	const IOnlineSessionPtr Interface = Sessions();
	return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession) != nullptr;
}

void UCheeseStrikeSessionSubsystem::Say(const FString& What)
{
	Status = What;
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: %s"), *What);
}

void UCheeseStrikeSessionSubsystem::SetPending(ESessionPending Now)
{
	Pending = Now;
}

void UCheeseStrikeSessionSubsystem::OnLevelLoaded(UWorld* World)
{
	// Another game's level (a second player in the same editor) is none of this one's business.
	if (World == nullptr || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// Joined: what was found, and the word that it was being joined, are over.
	if (Pending == ESessionPending::Joining && World->GetNetMode() == NM_Client)
	{
		Found.Empty();
		Results.Empty();
		Status.Reset();
		UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: in the host's match"));
	}
	SetPending(ESessionPending::None);
}

void UCheeseStrikeSessionSubsystem::OnTravelFailed(UWorld* World, ETravelFailure::Type Failure, const FString& Error)
{
	if (World == nullptr || World->GetGameInstance() == GetGameInstance())
	{
		SetPending(ESessionPending::None);
	}
}

void UCheeseStrikeSessionSubsystem::OnNetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error)
{
	if (World != nullptr && World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	SetPending(ESessionPending::None);
	// A joiner's connection to the host is gone: the engine takes them back to the level alone, where the front end says
	// why. The match joined is over for them, so it is left too, or it would be in the way of the next.
	if (Driver != nullptr && Driver->GetNetMode() == NM_Client)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: out of the host's match (%s: %s)"), ENetworkFailure::ToString(Failure), *Error);
		SayFarewell(CheeseStrikeSessions::FarewellFor(Failure));
		if (IOnlineSessionPtr Interface = Sessions(); Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession) != nullptr)
		{
			Interface->DestroySession(NAME_GameSession);
		}
		bBusy = false;
	}
}

void UCheeseStrikeSessionSubsystem::SayFarewell(CheeseStrikeSessions::EFarewell Why)
{
	using CheeseStrikeSessions::EFarewell;
	if (Why == EFarewell::None)
	{
		return;
	}
	// The host's own word stands over the lost connection that follows it.
	const bool bHostSaid = Farewell == EFarewell::HostClosedTheLobby || Farewell == EFarewell::HostLeftTheMatch || Farewell == EFarewell::Kicked;
	if (bHostSaid && Why == EFarewell::ConnectionLost && FPlatformTime::Seconds() - FarewellAt < 30.0)
	{
		return;
	}
	Farewell = Why;
	FarewellAt = FPlatformTime::Seconds();
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: %s"), *CheeseStrikeSessions::FarewellText(Why));
}

void UCheeseStrikeSessionSubsystem::SayGoodbye(UWorld* World)
{
	UNetDriver* Driver = World != nullptr ? World->GetNetDriver() : nullptr;
	if (Driver == nullptr || World->GetNetMode() != NM_ListenServer)
	{
		return;
	}
	const AGameStateBase* State = World->GetGameState();
	const bool bInMatch = State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded();
	int32 Told = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(It->Get());
		if (Player != nullptr && !Player->IsLocalController() && Player->GetNetConnection() != nullptr)
		{
			Player->ClientHostLeft(bInMatch);
			++Told;
		}
	}
	// Sent now: the connections close as the host goes, before the next tick would have sent it.
	for (UNetConnection* Connection : Driver->ClientConnections)
	{
		if (Connection != nullptr)
		{
			Connection->FlushNet(true);
		}
	}
	if (Told > 0)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: told %d joiner(s) the host is leaving"), Told);
	}
}

void UCheeseStrikeSessionSubsystem::OnExit()
{
	if (GetGameInstance() != nullptr)
	{
		SayGoodbye(GetGameInstance()->GetWorld());
	}
}

void UCheeseStrikeSessionSubsystem::Host(const FString& Options, bool bLan)
{
	IOnlineSessionPtr Interface = Sessions();
	if (!Interface.IsValid())
	{
		Say(TEXT("No online service to host through"));
		return;
	}
	if (bBusy)
	{
		Say(TEXT("Busy; try again in a moment"));
		return;
	}
	bBusy = true;
	SetPending(ESessionPending::Hosting);
	DismissFarewell();
	// A match of its own: nobody has sat in it yet.
	if (UCheeseStrikeSeatMemory* Memory = GetGameInstance()->GetSubsystem<UCheeseStrikeSeatMemory>())
	{
		Memory->Clear();
	}
	HostingOptions = CheeseStrikeSessions::NormalisedOptions(Options);
	bHostingLan = bLan;
	// A match already offered or joined is left first.
	if (Interface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Say(TEXT("Leaving the match to host a new one"));
		Interface->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnDestroyedToHost));
		return;
	}
	CreateNow();
}

void UCheeseStrikeSessionSubsystem::OnDestroyedToHost(FName SessionName, bool bWasSuccessful)
{
	CreateNow();
}

void UCheeseStrikeSessionSubsystem::CreateNow()
{
	Say(FString::Printf(TEXT("Offering a match %s"), bHostingLan ? TEXT("on the local network") : *FString::Printf(TEXT("through %s"), *ServiceName())));
	if (!Sessions()->CreateSession(0, NAME_GameSession, CheeseStrikeSessions::HostSettings(HostingOptions, bHostingLan)))
	{
		bBusy = false;
		SetPending(ESessionPending::None);
		Say(TEXT("The match couldn't be offered"));
	}
}

void UCheeseStrikeSessionSubsystem::OnCreated(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}
	bBusy = false;
	if (!bWasSuccessful)
	{
		SetPending(ESessionPending::None);
		Say(TEXT("The match couldn't be offered"));
		return;
	}
	Say(FString::Printf(TEXT("Hosting %s"), bHostingLan ? TEXT("on the local network") : *FString::Printf(TEXT("through %s"), *ServiceName())));
	Travel(CheeseStrikeSessions::TravelUrl(HostingOptions), true);
}

void UCheeseStrikeSessionSubsystem::Find(bool bLan)
{
	IOnlineSessionPtr Interface = Sessions();
	if (!Interface.IsValid())
	{
		Say(TEXT("No online service to look through"));
		return;
	}
	if (bBusy)
	{
		Say(TEXT("Busy; try again in a moment"));
		return;
	}
	bBusy = true;
	Found.Empty();
	Results.Empty();
	Search = CheeseStrikeSessions::Search(bLan);
	Say(FString::Printf(TEXT("Looking for matches %s"), bLan ? TEXT("on the local network") : *FString::Printf(TEXT("through %s"), *ServiceName())));
	if (!Interface->FindSessions(0, Search.ToSharedRef()))
	{
		bBusy = false;
		Say(TEXT("The search couldn't start"));
	}
}

void UCheeseStrikeSessionSubsystem::OnFound(bool bWasSuccessful)
{
	bBusy = false;
	Results.Empty();
	Found.Empty();
	if (!bWasSuccessful || !Search.IsValid())
	{
		Say(TEXT("The search failed"));
		return;
	}
	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		if (Result.IsValid() && CheeseStrikeSessions::IsOurs(Result))
		{
			Results.Add(Result);
			Found.Add(FString::Printf(TEXT("%d. %s"), Found.Num() + 1, *CheeseStrikeSessions::Describe(Result)));
		}
	}
	Say(Found.Num() == 0 ? TEXT("No matches found") : FString::Printf(TEXT("Found %d match%s"), Found.Num(), Found.Num() == 1 ? TEXT("") : TEXT("es")));
	for (const FString& Line : Found)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Sessions:   %s"), *Line);
	}
}

void UCheeseStrikeSessionSubsystem::Join(int32 Index)
{
	if (!Results.IsValidIndex(Index))
	{
		Say(FString::Printf(TEXT("There is no match %d to join; CheeseStrikeFind looks for them"), Index + 1));
		return;
	}
	StartJoin(Results[Index]);
}

void UCheeseStrikeSessionSubsystem::StartJoin(const FOnlineSessionSearchResult& Result)
{
	if (bBusy)
	{
		Say(TEXT("Busy; try again in a moment"));
		return;
	}
	bBusy = true;
	SetPending(ESessionPending::Joining);
	DismissFarewell();
	Joining = Result;
	if (Sessions()->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Say(TEXT("Leaving the match to join another"));
		Sessions()->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnDestroyedToJoin));
		return;
	}
	JoinNow(Joining);
}

void UCheeseStrikeSessionSubsystem::RefreshFriends()
{
	IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
	IOnlineFriendsPtr FriendsInterface = Service != nullptr ? Service->GetFriendsInterface() : nullptr;
	if (!FriendsInterface.IsValid())
	{
		Say(TEXT("No friends to read: no online service"));
		return;
	}
	// The list is read from the service; then each joinable friend's game is looked up, one at a time.
	FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::Default), FOnReadFriendsListComplete::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnFriendsRead));
}

void UCheeseStrikeSessionSubsystem::OnFriendsRead(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	Friends.Empty();
	IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
	IOnlineFriendsPtr FriendsInterface = Service != nullptr ? Service->GetFriendsInterface() : nullptr;
	if (!bWasSuccessful || !FriendsInterface.IsValid())
	{
		Say(FString::Printf(TEXT("The friends couldn't be read%s"), ErrorStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(": %s"), *ErrorStr)));
		return;
	}
	TArray<TSharedRef<FOnlineFriend>> Listed;
	FriendsInterface->GetFriendsList(LocalUserNum, ListName, Listed);
	for (const TSharedRef<FOnlineFriend>& Each : Listed)
	{
		FCheeseStrikeFriend Friend;
		Friend.Name = Each->GetDisplayName();
		Friend.Id = Each->GetUserId();
		// Joinable: their lobby or match, which Steam lets friends into by presence (and on invites).
		const FOnlineUserPresence& Presence = Each->GetPresence();
		Friend.bInGame = Presence.bIsJoinable;
		Friend.State = CheeseStrikeFriends::Classify(Friend.bInGame, 0, true, false);
		// What they publish: where they are, their party's permission and how many are in it. Steam can't say whether a friend
		// owns the game; running our app, or publishing our status, is the best sign there is.
		const FVariantData* Published = Presence.Status.Properties.Find(CheeseStrikeFriends::PresenceKey);
		FString Value;
		if (Published != nullptr)
		{
			Published->GetValue(Value);
		}
		Friend.bRunningThisGame = Presence.bIsPlayingThisGame;
		Friend.bPublishesStatus = Published != nullptr;
		Friend.Status = CheeseStrikeFriends::StatusFromValue(Value, Friend.bRunningThisGame || Friend.bPublishesStatus);
		FString Access;
		if (const FVariantData* Shut = Presence.Status.Properties.Find(CheeseStrikeFriends::PermissionKey))
		{
			Shut->GetValue(Access);
		}
		Friend.Permission = CheeseStrikeFriends::PermissionFromValue(Access);
		FString Count;
		if (const FVariantData* Counted = Presence.Status.Properties.Find(CheeseStrikeFriends::HumansKey))
		{
			Counted->GetValue(Count);
			Friend.Humans = Count.IsNumeric() ? FCString::Atoi(*Count) : -1;
		}
		// In the lobby we are in: Steam gives both our session ids as the lobby's.
		const IOnlineSessionPtr Interface = Sessions();
		const FNamedOnlineSession* Ours = Interface.IsValid() ? Interface->GetNamedSession(NAME_GameSession) : nullptr;
		Friend.bInMyParty = Ours != nullptr && Ours->SessionInfo.IsValid() && Presence.SessionId.IsValid()
			&& Presence.SessionId->ToString() == Ours->SessionInfo->GetSessionId().ToString();
		Friends.Add(Friend);
	}
	NextFriendLookup = 0;
	LookUpNextFriend();
}

void UCheeseStrikeSessionSubsystem::LookUpNextFriend()
{
	while (Friends.IsValidIndex(NextFriendLookup) && !Friends[NextFriendLookup].bInGame)
	{
		++NextFriendLookup;
	}
	IOnlineSessionPtr Interface = Sessions();
	if (!Friends.IsValidIndex(NextFriendLookup) || !Interface.IsValid())
	{
		return;
	}
	FriendSessionHandle = Interface->AddOnFindFriendSessionCompleteDelegate_Handle(0, FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnFriendSession));
	if (!Interface->FindFriendSession(0, *Friends[NextFriendLookup].Id))
	{
		Interface->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FriendSessionHandle);
		++NextFriendLookup;
		LookUpNextFriend();
	}
}

void UCheeseStrikeSessionSubsystem::OnFriendSession(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& Looked)
{
	if (IOnlineSessionPtr Interface = Sessions(); Interface.IsValid())
	{
		Interface->ClearOnFindFriendSessionCompleteDelegate_Handle(0, FriendSessionHandle);
	}
	if (Friends.IsValidIndex(NextFriendLookup))
	{
		FCheeseStrikeFriend& Friend = Friends[NextFriendLookup];
		if (bWasSuccessful && Looked.Num() > 0 && Looked[0].IsValid())
		{
			Friend.Session = Looked[0];
			// Bots don't count: the players in the game against its room.
			Friend.Humans = Looked[0].Session.SessionSettings.NumPublicConnections - Looked[0].Session.NumOpenPublicConnections;
			Friend.State = CheeseStrikeFriends::Classify(true, Friend.Humans, CheeseStrikeSessions::FriendsMayJoin(Looked[0].Session.SessionSettings), false);
			// Their party's permission, as its session says.
			Friend.Permission = CheeseStrikeSessions::FriendsMayJoin(Looked[0].Session.SessionSettings) ? CheeseStrikeFriends::EPermission::InvitesAndFriends : CheeseStrikeFriends::EPermission::InviteOnly;
		}
		else
		{
			// No game to be found now: they are not playing.
			Friend.bInGame = false;
			Friend.State = CheeseStrikeFriends::EState::NotPlaying;
		}
	}
	++NextFriendLookup;
	LookUpNextFriend();
}

UTexture2D* UCheeseStrikeSessionSubsystem::AvatarOf(int32 Index)
{
	if (!Friends.IsValidIndex(Index) || !Friends[Index].Id.IsValid())
	{
		return nullptr;
	}
	if (Avatars == nullptr)
	{
		Avatars = NewObject<UCheeseStrikeAvatars>(this);
	}
	return Avatars->ForUniqueId(Friends[Index].Id->GetType().ToString(), Friends[Index].Id->ToString(), GetWorld());
}

bool UCheeseStrikeSessionSubsystem::FriendsMayJoinHere() const
{
	const IOnlineSessionPtr Interface = Sessions();
	const FNamedOnlineSession* Named = Interface.IsValid() ? Interface->GetNamedSession(NAME_GameSession) : nullptr;
	return Named == nullptr || CheeseStrikeSessions::FriendsMayJoin(Named->SessionSettings);
}

void UCheeseStrikeSessionSubsystem::SetFriendsMayJoin(bool bFriends)
{
	IOnlineSessionPtr Interface = Sessions();
	FNamedOnlineSession* Named = Interface.IsValid() ? Interface->GetNamedSession(NAME_GameSession) : nullptr;
	if (Named == nullptr || !Named->bHosting)
	{
		Say(TEXT("Only the host decides who can join"));
		return;
	}
	FOnlineSessionSettings Settings = CheeseStrikeSessions::WithFriendsMayJoin(Named->SessionSettings, bFriends);
	Interface->UpdateSession(NAME_GameSession, Settings, true);
	// The presence says the permission too: send it again.
	PublishedPresence.Reset();
	Say(bFriends ? TEXT("Friends can join") : TEXT("Only invitations let players in"));
}

void UCheeseStrikeSessionSubsystem::PublishPresence(CheeseStrikeFriends::EStatus Now, int32 Humans)
{
	IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
	const IOnlinePresencePtr Presence = Service != nullptr ? Service->GetPresenceInterface() : nullptr;
	const IOnlineIdentityPtr Identity = Service != nullptr ? Service->GetIdentityInterface() : nullptr;
	const FUniqueNetIdPtr Me = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (!Presence.IsValid() || !Me.IsValid())
	{
		return;
	}
	const CheeseStrikeFriends::EPermission Permission = FriendsMayJoinHere() ? CheeseStrikeFriends::EPermission::InvitesAndFriends : CheeseStrikeFriends::EPermission::InviteOnly;
	const bool bParty = IsInAMatch();
	const FString Key = FString::Printf(TEXT("%d|%d|%d|%d"), static_cast<int32>(Now), static_cast<int32>(Permission), bParty ? Humans : -1, bParty ? 1 : 0);
	if (Key == PublishedPresence)
	{
		return;
	}
	PublishedPresence = Key;
	FOnlineUserPresenceStatus Published;
	Published.State = EOnlinePresenceState::Online;
	Published.StatusStr = CheeseStrikeFriends::StatusLabel(Now);
	Published.Properties.Add(CheeseStrikeFriends::PresenceKey, FVariantData(CheeseStrikeFriends::StatusValue(Now)));
	if (bParty)
	{
		Published.Properties.Add(CheeseStrikeFriends::PermissionKey, FVariantData(CheeseStrikeFriends::PermissionValue(Permission)));
		Published.Properties.Add(CheeseStrikeFriends::HumansKey, FVariantData(FString::FromInt(Humans)));
	}
	Presence->SetPresence(*Me, Published);
}

void UCheeseStrikeSessionSubsystem::InviteFriend(int32 Index)
{
	if (!Friends.IsValidIndex(Index) || !CheeseStrikeFriends::CanInvite(IsInAMatch(), false))
	{
		Say(TEXT("Only a friend can be invited, to a lobby or match of ours"));
		return;
	}
	if (!Sessions()->SendSessionInviteToFriend(0, NAME_GameSession, *Friends[Index].Id))
	{
		Say(FString::Printf(TEXT("The invitation to %s couldn't be sent"), *Friends[Index].Name));
		return;
	}
	Say(FString::Printf(TEXT("Invited %s"), *Friends[Index].Name));
}

void UCheeseStrikeSessionSubsystem::JoinFriend(int32 Index)
{
	if (!Friends.IsValidIndex(Index) || Friends[Index].State != CheeseStrikeFriends::EState::Join || !Friends[Index].Session.IsValid())
	{
		Say(FString::Printf(TEXT("%s's game can't be joined"), Friends.IsValidIndex(Index) ? *Friends[Index].Name : TEXT("That friend")));
		return;
	}
	StartJoin(Friends[Index].Session);
}

void UCheeseStrikeSessionSubsystem::OnDestroyedToJoin(FName SessionName, bool bWasSuccessful)
{
	JoinNow(Joining);
}

void UCheeseStrikeSessionSubsystem::JoinNow(const FOnlineSessionSearchResult& Result)
{
	Say(TEXT("Joining the lobby"));
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: joining %s"), *CheeseStrikeSessions::Describe(Result));
	if (!Sessions()->JoinSession(0, NAME_GameSession, Result))
	{
		bBusy = false;
		SetPending(ESessionPending::None);
		Say(TEXT("The match couldn't be joined"));
	}
}

void UCheeseStrikeSessionSubsystem::OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}
	bBusy = false;
	FString Url;
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions()->GetResolvedConnectString(NAME_GameSession, Url))
	{
		SetPending(ESessionPending::None);
		Say(FString::Printf(TEXT("The match couldn't be joined (%s)"), LexToString(Result)));
		return;
	}
	// Still joining, to the player: the lobby is theirs once the host's level has loaded.
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: joined; travelling to the host"));
	Travel(Url, false);
}

void UCheeseStrikeSessionSubsystem::OnInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Invite)
{
	if (!bWasSuccessful || !Invite.IsValid())
	{
		Say(TEXT("The invitation couldn't be taken up"));
		return;
	}
	Say(TEXT("Taking up an invitation"));
	bBusy = true;
	SetPending(ESessionPending::Joining);
	Joining = Invite;
	if (Sessions()->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Sessions()->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnDestroyedToJoin));
		return;
	}
	JoinNow(Joining);
}

void UCheeseStrikeSessionSubsystem::Leave()
{
	IOnlineSessionPtr Interface = Sessions();
	if (!Interface.IsValid() || Interface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		Say(TEXT("Not in a match"));
		return;
	}
	bBusy = true;
	Say(TEXT("Leaving the match"));
	Interface->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnDestroyedToLeave));
}

void UCheeseStrikeSessionSubsystem::UpdateRules(const FString& Options)
{
	IOnlineSessionPtr Interface = Sessions();
	if (!Interface.IsValid() || Interface->GetNamedSession(NAME_GameSession) == nullptr)
	{
		return;
	}
	HostingOptions = CheeseStrikeSessions::NormalisedOptions(Options);
	FOnlineSessionSettings Settings = CheeseStrikeSessions::HostSettings(HostingOptions, bHostingLan);
	Interface->UpdateSession(NAME_GameSession, Settings, true);
}

void UCheeseStrikeSessionSubsystem::OnDestroyedToLeave(FName SessionName, bool bWasSuccessful)
{
	bBusy = false;
	Say(TEXT("Left the match"));
	// Back to the level, alone. Not if the player is there already, alone (the match was left some other way meanwhile, by the
	// host's goodbye, say): a second trip to the same place would show the loading cover again and end where it began.
	if (APlayerController* Player = GetGameInstance() != nullptr ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
	{
		const UWorld* Here = Player->GetWorld();
		const bool bAlreadyThere = Here != nullptr && Here->GetNetMode() == NM_Standalone && Here->GetOutermost()->GetName().EndsWith(TEXT("Lvl_Shooter"));
		if (bAlreadyThere)
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: already back at the front end, not travelling there again"));
			return;
		}
		Player->ClientTravel(CheeseStrikeSessions::Level, TRAVEL_Absolute);
	}
}

void UCheeseStrikeSessionSubsystem::Travel(const FString& Url, bool bAsHost)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (bAsHost)
	{
		World->ServerTravel(Url);
	}
	else if (APlayerController* Player = GetGameInstance()->GetFirstLocalPlayerController())
	{
		Player->ClientTravel(Url, TRAVEL_Absolute);
	}
}
