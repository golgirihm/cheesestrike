// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSessions.h"
#include "Online/OnlineSessionNames.h"
#include "Rules/CheeseStrikeRules.h"

namespace CheeseStrikeSessions
{
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
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bAllowInvites = true;
		Settings.bIsLANMatch = bLan;
		// On Steam a match is a lobby, which is what the friends list can join; the two flags must agree.
		Settings.bUsesPresence = !bLan;
		Settings.bUseLobbiesIfAvailable = !bLan;
		Settings.bAllowJoinViaPresence = !bLan;
		Settings.bAllowJoinViaPresenceFriendsOnly = false;
		Settings.bIsDedicated = false;
		Settings.bUsesStats = false;
		Settings.bAntiCheatProtected = false;
		Settings.Set(GameKey, FString(GameValue), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(SETTING_MAPNAME, FString(Level), EOnlineDataAdvertisementType::ViaOnlineService);
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
		FString Line = FString::Printf(TEXT("%s's game, %d of %d in, first to %d kills"), *Host, In, Settings.NumPublicConnections, KillLimit);
		if (Result.PingInMs >= 0 && Result.PingInMs < MAX_QUERY_PING)
		{
			Line += FString::Printf(TEXT(", %d ms"), Result.PingInMs);
		}
		return Line;
	}
}
