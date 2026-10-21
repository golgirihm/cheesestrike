// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikeSessionSubsystem.generated.h"

class UNetDriver;
class UWorld;

/** What the player has set going that ends in a level change: the front end shows it, and takes no more clicks until it is over. */
enum class ESessionPending : uint8
{
	None,
	/** Offering a match, then reloading the level into its lobby. */
	Hosting,
	/** Joining a match found (or an invitation), then travelling to its host. */
	Joining,
};

/**
 * Hosts, finds, joins and leaves matches through the online service: Steam
 * when the Steam client is running, or the Null service on the local
 * network. A host offers a match (a Steam lobby) and then listens on the
 * level; a joiner finds the matches of this game, picks one and travels to
 * its host; accepting an invite, or joining from the Steam friends list,
 * does the same. What is going on, and what was found, is kept as text for
 * the HUD (CheeseStrikeHUD) and the log.
 *
 * From the console: CheeseStrikeHost [options], CheeseStrikeFind,
 * CheeseStrikeJoin <number>, CheeseStrikeLeave (CheeseStrikePlayerController),
 * and the Lan forms for the local network.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Offers a match with the URL `Options` and, once offered, listens on the level for it. */
	void Host(const FString& Options, bool bLan);
	/** Looks for this game's matches; what it finds is in GetFound(). */
	void Find(bool bLan);
	/** Joins the `Index`th match found. */
	void Join(int32 Index);
	/** Leaves the match hosted or joined, back to the level alone. */
	void Leave();
	/** The host's match, as offered, now says these URL `Options` (its rules), for those looking for one. */
	void UpdateRules(const FString& Options);

	/** The matches the last search found, described. */
	const TArray<FString>& GetFound() const { return Found; }
	/** What is going on, in a line. */
	const FString& GetStatus() const { return Status; }
	/** Whether something is under way that the next command would cut across. */
	bool IsBusy() const { return bBusy; }
	/**
	 * Hosting or joining, from the moment it is asked for until the level
	 * it leads to has loaded, or it fails: the menu shows a spinner, and its
	 * buttons can't be pressed.
	 */
	ESessionPending GetPending() const { return Pending; }
	/** The service in use: "Steam" or "Null". */
	FString ServiceName() const;
	/** Whether this game instance hosts or has joined a match. */
	bool IsInAMatch() const;

protected:
	IOnlineSessionPtr Sessions() const;
	void Say(const FString& What);
	void OnCreated(FName SessionName, bool bWasSuccessful);
	void OnFound(bool bWasSuccessful);
	void OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Invite);
	void OnDestroyedToHost(FName SessionName, bool bWasSuccessful);
	void OnDestroyedToLeave(FName SessionName, bool bWasSuccessful);
	void OnDestroyedToJoin(FName SessionName, bool bWasSuccessful);
	void CreateNow();
	void JoinNow(const FOnlineSessionSearchResult& Result);
	void Travel(const FString& Url, bool bAsHost);
	/** Hosting or joining is over: a level of this game's has loaded, or the travel failed. */
	void OnLevelLoaded(UWorld* World);
	void OnTravelFailed(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
	void OnNetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
	void SetPending(ESessionPending Now);

	/** The last search, and the matches of ours it found. */
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	TArray<FString> Found;
	FString Status;
	bool bBusy = false;
	ESessionPending Pending = ESessionPending::None;
	/** What the next step needs: the options of the match being hosted, or the match being joined. */
	FString HostingOptions;
	bool bHostingLan = false;
	FOnlineSessionSearchResult Joining;

	FDelegateHandle CreatedHandle;
	FDelegateHandle FoundHandle;
	FDelegateHandle JoinedHandle;
	FDelegateHandle InviteHandle;
	FDelegateHandle LoadedHandle;
	FDelegateHandle TravelFailedHandle;
	FDelegateHandle NetworkFailedHandle;
};
