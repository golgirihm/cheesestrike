// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"

/**
 * How a match is offered and found through the online service (Steam, or
 * the Null service on a local network), as plain functions over the
 * engine's session settings, so that they can be tested without a service.
 * UCheeseStrikeSessionSubsystem applies them.
 */
namespace CheeseStrikeSessions
{
	/** Players a match holds (docs/unreal/design.md, "Modes"). */
	constexpr int32 MaxPlayers = 16;
	/** Games a search brings back at most. */
	constexpr int32 MaxResults = 50;
	/** The level a match is played on, until there are arenas. */
	extern const TCHAR* const Level;
	/** Keys the host's settings travel under: which game this is (the test app id is shared with every Unreal project), and the match's rules. */
	extern const FName GameKey;
	extern const FName KillLimitKey;
	extern const FName RespawnDelayKey;
	extern const TCHAR* const GameValue;

	/** `Options` as a map URL carries them: with the leading `?`, or empty. */
	FString NormalisedOptions(const FString& Options);

	/** A match to be hosted with the URL `Options` (`?KillLimit=10?RespawnDelay=1`), over the local network or the service. */
	FOnlineSessionSettings HostSettings(const FString& Options, bool bLan);

	/** A search for matches of this game, over the local network or the service. */
	TSharedRef<FOnlineSessionSearch> Search(bool bLan);

	/** The URL the host travels to, to play the match it offered: the level, listening, with `Options`. */
	FString TravelUrl(const FString& Options);

	/** Whether a found match is one of this game's, rather than another project's on the shared test app id. */
	bool IsOurs(const FOnlineSessionSearchResult& Result);

	/** A found match in a line, for the player to pick: who hosts, how many are in, the rules, the ping. */
	FString Describe(const FOnlineSessionSearchResult& Result);

	/** Why a joiner is no longer in the host's match, as the front end tells them once they are back on it. */
	enum class EFarewell : uint8
	{
		None,
		/** The host left the lobby, or quit the game from it. */
		HostClosedTheLobby,
		/** The host left the match, or quit the game during it. */
		HostLeftTheMatch,
		/** The connection to the host was lost: it timed out or closed with no word from the host. */
		ConnectionLost,
		/** The host removed them from the game (the friends panel's Kick). */
		Kicked,
		/** The host couldn't be reached on joining. */
		CouldNotJoin,
	};
	/** What a joiner's network failure means for them; None for a failure that doesn't end their match. */
	EFarewell FarewellFor(ENetworkFailure::Type Failure);
	/** The title and the line the front end shows for `Farewell` (nothing for None). */
	FString FarewellTitle(EFarewell Farewell);
	FString FarewellText(EFarewell Farewell);
}
