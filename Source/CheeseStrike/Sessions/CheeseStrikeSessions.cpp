// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSessions.h"
#include "Online/OnlineSessionNames.h"
#include "Rules/CheeseStrikeRules.h"

namespace CheeseStrikeSessions
{
	EFarewell FarewellFor(ENetworkFailure::Type Failure)
	{
		switch (Failure)
		{
		case ENetworkFailure::PendingConnectionFailure:
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
		case ENetworkFailure::NetDriverCreateFailure:
		case ENetworkFailure::NetDriverListenFailure:
			return EFarewell::CouldNotJoin;
		case ENetworkFailure::NetDriverAlreadyExists:
			return EFarewell::None;
		default:
			return EFarewell::ConnectionLost;
		}
	}

	FString FarewellTitle(EFarewell Farewell)
	{
		switch (Farewell)
		{
		case EFarewell::HostClosedTheLobby: return TEXT("THE LOBBY WAS CLOSED");
		case EFarewell::HostLeftTheMatch: return TEXT("THE HOST LEFT");
		case EFarewell::ConnectionLost: return TEXT("DISCONNECTED");
		case EFarewell::CouldNotJoin: return TEXT("COULDN'T JOIN");
		case EFarewell::Kicked: return TEXT("YOU WERE REMOVED");
		default: return FString();
		}
	}

	FString FarewellText(EFarewell Farewell)
	{
		switch (Farewell)
		{
		case EFarewell::HostClosedTheLobby: return TEXT("The host closed the lobby.");
		case EFarewell::HostLeftTheMatch: return TEXT("The host left, and the match is over.");
		case EFarewell::ConnectionLost: return TEXT("The connection to the host was lost.");
		case EFarewell::CouldNotJoin: return TEXT("The host's match couldn't be reached.");
		case EFarewell::Kicked: return TEXT("The host removed you from the game.");
		default: return FString();
		}
	}

	const TCHAR* const Level = TEXT("/Game/Variant_Shooter/Lvl_Shooter");
	const FName GameKey(TEXT("CHEESESTRIKE"));
	const FName KillLimitKey(TEXT("KILLLIMIT"));
	const FName RespawnDelayKey(TEXT("RESPAWNDELAY"));
	const TCHAR* const GameValue = TEXT("GoudaOverload");

	FString NormalisedOptions(const FString& Options)
	{
		FString Trimmed = Options.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}
		return Trimmed.StartsWith(TEXT("?")) ? Trimmed : TEXT("?") + Trimmed;
	}

	FOnlineSessionSettings HostSettings(const FString& Options, bool bLan)
	{
		const FString Normalised = NormalisedOptions(Options);
		FOnlineSessionSettings Settings;
		Settings.NumPublicConnections = MaxPlayers;
		Settings.NumPrivateConnections = 0;
		// On the service, not advertised to everyone: a friend finds it (presence) or it is sent on an invite, never by a public
		// search. On the local network (the editor, the tests), found by a search there as it always was.
		Settings.bShouldAdvertise = bLan;
		Settings.bAllowJoinInProgress = true;
		Settings.bAllowInvites = true;
		Settings.bIsLANMatch = bLan;
		// On Steam a match is a lobby, which is what the friends list can join; the two flags must agree.
		Settings.bUsesPresence = !bLan;
		Settings.bUseLobbiesIfAvailable = !bLan;
		Settings.bAllowJoinViaPresence = !bLan;
		Settings.bAllowJoinViaPresenceFriendsOnly = true;
		Settings.bIsDedicated = false;
		Settings.bUsesStats = false;
		Settings.bAntiCheatProtected = false;
		Settings.Set(GameKey, FString(GameValue), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		// The map the match is to be played on, by name: the one picked in the lobby, or the level it is hosted on.
		const int32 Map = CheeseStrikeRules::MapFrom(Normalised, CheeseStrikeRules::MapIndexFor(Level));
		Settings.Set(SETTING_MAPNAME, Map >= 0 ? FString(CheeseStrikeRules::Maps[Map].Name) : FString(Level), EOnlineDataAdvertisementType::ViaOnlineService);
		Settings.Set(KillLimitKey, CheeseStrikeRules::KillLimitFrom(Normalised), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(RespawnDelayKey, CheeseStrikeRules::RespawnDelayFrom(Normalised), EOnlineDataAdvertisementType::ViaOnlineService);
		return Settings;
	}

	TSharedRef<FOnlineSessionSearch> Search(bool bLan)
	{
		TSharedRef<FOnlineSessionSearch> Found = MakeShared<FOnlineSessionSearch>();
		Found->MaxSearchResults = MaxResults;
		Found->bIsLanQuery = bLan;
		Found->QuerySettings.Set(GameKey, FString(GameValue), EOnlineComparisonOp::Equals);
		if (!bLan)
		{
			Found->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		}
		return Found;
	}

	FString TravelUrl(const FString& Options)
	{
		return FString(Level) + TEXT("?listen") + NormalisedOptions(Options);
	}

	bool IsOurs(const FOnlineSessionSearchResult& Result)
	{
		FString Game;
		return Result.Session.SessionSettings.Get(GameKey, Game) && Game == GameValue;
	}

	FString Describe(const FOnlineSessionSearchResult& Result)
	{
		const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;
		int32 KillLimit = CheeseStrikeRules::KillLimit;
		Settings.Get(KillLimitKey, KillLimit);
		const int32 In = Settings.NumPublicConnections - Result.Session.NumOpenPublicConnections;
		const FString Host = Result.Session.OwningUserName.IsEmpty() ? TEXT("someone") : Result.Session.OwningUserName;
		FString MapName;
		Settings.Get(SETTING_MAPNAME, MapName);
		FString Line = FString::Printf(TEXT("%s's game, %d of %d in, first to %d kills"), *Host, In, Settings.NumPublicConnections, KillLimit);
		if (!MapName.IsEmpty() && !MapName.StartsWith(TEXT("/")))
		{
			Line += FString::Printf(TEXT(" on %s"), *MapName);
		}
		if (Result.PingInMs >= 0 && Result.PingInMs < MAX_QUERY_PING)
		{
			Line += FString::Printf(TEXT(", %d ms"), Result.PingInMs);
		}
		return Line;
	}
}
