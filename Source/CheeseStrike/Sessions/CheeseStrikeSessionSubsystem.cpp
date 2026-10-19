// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	UE_LOG(LogCheeseStrike, Display, TEXT("Sessions: through %s"), *ServiceName());
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
	Say(Found.Num() == 0 ? TEXT("No matches found") : FString::Printf(TEXT("Found %d match%s; CheeseStrikeJoin <number> joins one"), Found.Num(), Found.Num() == 1 ? TEXT("") : TEXT("es")));
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
	if (bBusy)
	{
		Say(TEXT("Busy; try again in a moment"));
		return;
	}
	bBusy = true;
	Joining = Results[Index];
	if (Sessions()->GetNamedSession(NAME_GameSession) != nullptr)
	{
		Say(TEXT("Leaving the match to join another"));
		Sessions()->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCheeseStrikeSessionSubsystem::OnDestroyedToJoin));
		return;
	}
	JoinNow(Joining);
}

void UCheeseStrikeSessionSubsystem::OnDestroyedToJoin(FName SessionName, bool bWasSuccessful)
{
	JoinNow(Joining);
}

void UCheeseStrikeSessionSubsystem::JoinNow(const FOnlineSessionSearchResult& Result)
{
	Say(FString::Printf(TEXT("Joining %s"), *CheeseStrikeSessions::Describe(Result)));
	if (!Sessions()->JoinSession(0, NAME_GameSession, Result))
	{
		bBusy = false;
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
		Say(FString::Printf(TEXT("The match couldn't be joined (%s)"), LexToString(Result)));
		return;
	}
	Say(TEXT("Joined; travelling to the host"));
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

void UCheeseStrikeSessionSubsystem::OnDestroyedToLeave(FName SessionName, bool bWasSuccessful)
{
	bBusy = false;
	Say(TEXT("Left the match"));
	// Back to the level, alone.
	if (APlayerController* Player = GetGameInstance() != nullptr ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
	{
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
