// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Rules/CheeseStrikeFriends.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CheeseStrikeSessions.h"
#include "CheeseStrikeSessionSubsystem.generated.h"

class UNetDriver;
class UWorld;
class UCheeseStrikeAvatars;

/** What the player has set going that ends in a level change: the front end shows it, and takes no more clicks until it is over. */
enum class ESessionPending : uint8
{
	None,
	/** Offering a match, then reloading the level into its lobby. */
	Hosting,
	/** Joining a match found (or an invitation), then travelling to its host. */
	Joining,
};

/** A friend, as the friends panel lists them: their name, whether their game can be joined from here, and how full it is. */
struct FCheeseStrikeFriend
{
	FString Name;
	FUniqueNetIdPtr Id;
	/** Whether Steam says their game (a lobby or match of this one) is joinable. */
	bool bInGame = false;
	/** The players in it, once its session has been looked up (-1 until then). */
	int32 Humans = -1;
	/** Their game's session, as Steam found it: what joining takes. */
	FOnlineSessionSearchResult Session;
	CheeseStrikeFriends::EState State = CheeseStrikeFriends::EState::NotPlaying;
	/** Whether Steam says they are running this game (the best Steam gives for "owns it"). */
	bool bRunningThisGame = false;
	/** Whether they publish our status (cs_status): they have the game. */
	bool bPublishesStatus = false;
	/** What they are doing, from their published status. */
	CheeseStrikeFriends::EStatus Status = CheeseStrikeFriends::EStatus::NotInGame;
	/** Their party's permission, from their game's session or what they publish. */
	CheeseStrikeFriends::EPermission Permission = CheeseStrikeFriends::EPermission::InvitesAndFriends;
	/** Whether Steam puts them in the lobby we are in. */
	bool bInMyParty = false;
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

	/** The friends the last refresh found, with their games. */
	const TArray<FCheeseStrikeFriend>& GetFriends() const { return Friends; }
	/** Reads the friends list again, and looks up each joinable friend's game. */
	void RefreshFriends();
	/** Invites the friend `Index` (of GetFriends()) to the lobby or match we are in. */
	void InviteFriend(int32 Index);
	/** Joins the game of the friend `Index`. */
	void JoinFriend(int32 Index);
	/** The picture of the friend `Index`, if Steam has one yet (asked for, else). */
	UTexture2D* AvatarOf(int32 Index);
	/** Whether friends may join this game (the host's choice; true with no game). */
	bool FriendsMayJoinHere() const;
	/** The host lets friends join by their game, or only on an invitation. */
	void SetFriendsMayJoin(bool bFriends);
	/**
	 * Tells Steam (rich presence) what this player is doing, their party's permission and the humans in it, for their friends'
	 * panels. Does nothing until one of them has changed; the controller calls it each tick.
	 */
	void PublishPresence(CheeseStrikeFriends::EStatus Now, int32 Humans);

	/** The matches the last search found, described. */
	const TArray<FString>& GetFound() const { return Found; }
	/** What is going on, in a line. */
	/**
	 * What the sessions last said ("Left the match", "The search failed"), for a few seconds after it was said, or while a
	 * search or a join is still going: an old note must not greet the player on a page they open much later.
	 */
	FString GetStatus() const { return (bBusy || FPlatformTime::Seconds() - StatusAt < StatusSeconds) ? Status : FString(); }
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

	/**
	 * Why this player, a joiner, was last put out of a match (the host left
	 * or quit, or the connection was lost), for the front end to say until
	 * they dismiss it; None if nothing has.
	 */
	CheeseStrikeSessions::EFarewell GetFarewell() const { return Farewell; }
	void DismissFarewell() { Farewell = CheeseStrikeSessions::EFarewell::None; }
	/** A joiner is out of the match for `Why`; the host's word (left, closed) isn't overwritten by the lost connection that follows it. */
	void SayFarewell(CheeseStrikeSessions::EFarewell Why);
	/**
	 * The host is leaving `World`'s match (the lobby's or a match's), or
	 * quitting the game: every joiner is told why at once, before the
	 * connection to them closes (ACheeseStrikePlayerController::ClientHostLeft).
	 * Nothing for a joiner, or alone.
	 */
	void SayGoodbye(UWorld* World);

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
	/** Joins `Result`'s match, leaving the one we are in first. */
	void StartJoin(const FOnlineSessionSearchResult& Result);
	void OnFriendsRead(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);
	/** Looks up the game of the next joinable friend (the Steam calls go one at a time). */
	void LookUpNextFriend();
	void OnFriendSession(int32 LocalUserNum, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& Looked);
	void Travel(const FString& Url, bool bAsHost);
	/** Hosting or joining is over: a level of this game's has loaded, or the travel failed. */
	void OnLevelLoaded(UWorld* World);
	void OnTravelFailed(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
	void OnNetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Failure, const FString& Error);
	void SetPending(ESessionPending Now);
	/** The game is closing (its window, Alt+F4, Quit): a host says goodbye first. */
	void OnExit();

	/** The last search, and the matches of ours it found. */
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	TArray<FString> Found;
	FString Status;
	double StatusAt = 0.0;
	static constexpr double StatusSeconds = 8.0;
	bool bBusy = false;
	ESessionPending Pending = ESessionPending::None;
	/** What the next step needs: the options of the match being hosted, or the match being joined. */
	FString HostingOptions;
	bool bHostingLan = false;
	FOnlineSessionSearchResult Joining;
	/** The friends as last read, and the next one whose game is to be looked up. */
	TArray<FCheeseStrikeFriend> Friends;
	int32 NextFriendLookup = 0;
	FDelegateHandle FriendSessionHandle;
	/** The presence last published, so that it is sent again only when it changes. */
	FString PublishedPresence;
	/** What the host picked for who can join, shown at once (the session's settings are updated later, or with no session not at all). Cleared when a session is made or left. */
	TOptional<bool> FriendsChoice;
	/** Pictures of friends, by Steam id (created when first asked for). */
	UPROPERTY()
	TObjectPtr<UCheeseStrikeAvatars> Avatars;

	FDelegateHandle CreatedHandle;
	FDelegateHandle FoundHandle;
	FDelegateHandle JoinedHandle;
	FDelegateHandle InviteHandle;
	FDelegateHandle LoadedHandle;
	FDelegateHandle TravelFailedHandle;
	FDelegateHandle NetworkFailedHandle;
	FDelegateHandle ExitHandle;
	CheeseStrikeSessions::EFarewell Farewell = CheeseStrikeSessions::EFarewell::None;
	/** When the host's word came (FPlatformTime), so that the failure it leads to keeps it. */
	double FarewellAt = 0.0;
};
