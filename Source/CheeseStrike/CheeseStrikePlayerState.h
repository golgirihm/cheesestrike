// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "Rules/CheeseStrikeStreaks.h"
#include "Rules/CheeseStrikeBadges.h"
#include "CheeseStrikePlayerState.generated.h"

class ACheeseStrikeWatchCamera;

/**
 * What a match knows about a player wherever their character is: their
 * kills and deaths, counted by the server and seen by everyone; their side;
 * their run of kills and the streaks it has earned (CheeseStrikeStreaks);
 * and, in Elimination, whether they are out until the next round.
 *
 * It is also where the player calls in a streak, being theirs on their own
 * machine: the Streak key calls the next one in, or, for a Cheese Strike,
 * starts choosing where it goes on the HUD's strike map, which the next
 * shot calls in (or the key or Escape calls off). An out player watches a teammate
 * (ACheeseStrikeWatchCamera).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	int32 GetKills() const { return Kills; }
	int32 GetDeaths() const { return Deaths; }
	/** Kills they helped with: hurt the one killed lately. */
	int32 GetAssists() const { return Assists; }
	/** Players they put down (Execution). */
	int32 GetDowns() const { return Downs; }
	/** Server only. */
	void AddAssist() { ++Assists; }
	/** Server only. */
	void AddDown() { ++Downs; }
	/** The side this player is on (CheeseStrikeRules::TeamNames); in free-for-all every player has one too, which nothing uses. */
	int32 GetTeam() const { return Team; }
	/** Server only. A kill, by this player's own weapon (`bCountsForStreak`) or a streak's; returns the streak it earns, if any. */
	CheeseStrikeStreaks::EStreak AddKill(bool bCountsForStreak = true);
	/** Server only: killed, which ends the run of kills. */
	void AddDeath();
	/** How many kills of theirs have earned `Badge` (CheeseStrikeBadges), on every machine: for the achievements. */
	int32 GetBadgeCount(CheeseStrikeBadges::EBadge Badge) const;
	/** Server only: a kill of theirs earned `Badges` (as bits). */
	void AddBadges(uint8 Badges);
	/** Server only: the player id of who last killed them and hasn't been paid back for it since (-1 for nobody), for REVENGE. */
	int32 GetLastKilledBy() const { return LastKilledBy; }
	void SetLastKilledBy(int32 KillerId) { LastKilledBy = KillerId; }
	void SetTeam(int32 InTeam) { Team = InTeam; }
	/** When this player first joined the match, against the others (0 the first; kept across its levels): the lobby seats them in this order. */
	int32 GetJoinOrder() const { return JoinOrder; }
	/** Server only. */
	void SetJoinOrder(int32 Order) { JoinOrder = Order; }

	/** In Gun Game, the step of the ladder this player is on (CheeseStrikeArsenal::GunGameLadder), 0 the first. */
	int32 GetGunLevel() const { return GunLevel; }
	/** Server only. */
	void SetGunLevel(int32 Level) { GunLevel = Level; }

	/** King of the Hill: the points this player has scored by holding the hill alone (the solo mode's score), and the whole seconds they have stood in it, held or contested. */
	int32 GetHillPoints() const { return HillPoints; }
	int32 GetHillSeconds() const { return HillSeconds; }
	/** Server only. */
	void AddHillPoints(int32 Points) { HillPoints += Points; }
	void AddHillTime(float Seconds);

	/** This player's line on the scoreboard. */
	CheeseStrikeRules::FStanding Standing() const;

	/** Kills in a row, now. */
	int32 GetStreak() const { return StreakKills; }
	/** The streaks earned and not yet called in, oldest first; the key calls in the first. */
	TArray<CheeseStrikeStreaks::EStreak> GetReadyStreaks() const;
	CheeseStrikeStreaks::EStreak NextStreak() const;
	/** The streak earned last, and the server's time it was (for the word that it is ready); None and 0 before any. */
	CheeseStrikeStreaks::EStreak GetLastEarned() const { return static_cast<CheeseStrikeStreaks::EStreak>(LastEarned); }
	float GetLastEarnedAt() const { return LastEarnedAt; }
	/** Server only: gives `Kind` as if earned (for testing, and the CheeseStrikeGiveStreak command). */
	void GiveStreak(CheeseStrikeStreaks::EStreak Kind);
	/** Server only: takes `Kind` off the list as it is called in; false if it isn't the next. */
	bool TakeStreak(CheeseStrikeStreaks::EStreak Kind);

	/** In Elimination: whether this player has been killed this round, or came too late into it, and waits for the next. */
	bool IsOut() const { return bOut; }
	/** Server only. */
	void SetOut(bool bInOut);

	/** In free-for-all, until when (the server's time) this player's own Unmanned Aerial Cheese shows them everyone; in team modes the game state keeps it for the side. */
	float GetRevealUntil() const { return RevealUntil; }
	/** And since when its sweeps have run (a second drone adds its time to the first's). */
	float GetRevealFrom() const { return RevealFrom; }
	void SetReveal(float From, float Until) { RevealFrom = From; RevealUntil = Until; }

	/** The Streak key, on this player's machine: calls in the next streak, or starts or stops marking where a Cheese Strike goes. */
	void StreakPressed();
	/**
	 * Whether this player is choosing where their Cheese Strike goes, on the
	 * HUD's strike map (the minimap, big in the middle): the look (the mouse
	 * or the stick) moves the mark instead of the view, and a shot calls it
	 * in instead of firing.
	 */
	bool IsMarkingStrike() const { return bMarkingStrike; }
	/** Where this player's view meets the world, and its yaw; false if it meets nothing near enough. Where the mark starts. */
	bool StrikeAim(FVector& OutSpot, float& OutYaw) const;
	/** While marking: the middle of the strike's area (world X and Y), and the view's yaw when marking began, which the strike map is turned by. */
	FVector2D GetStrikeTarget() const { return StrikeTarget; }
	float GetStrikeYaw() const { return StrikeYaw; }
	void SetStrikeTarget(const FVector2D& Target);
	/** Moves the mark by what the look would have turned the view (degrees: right, and up), on the map as it is turned (CheeseStrikeStreaks::StrikeCursorPerDegree), held within reach of the caller. */
	void MoveStrikeTarget(const FVector2D& Degrees);
	/** The spot the strike is marked on: the ground under the mark, the floor nearest the caller's feet (ACheeseStrikeBombs::GroundNear). */
	bool StrikeSpot(FVector& OutSpot) const;
	/** Calls the Cheese Strike in over the area at the mark (StrikeSpot). */
	void MarkStrike();
	/** Stops marking; the streak stays ready. */
	void CancelStrike();

	/** For the HUD: where the enemies were at the Unmanned Aerial Cheese's last sweep, as this player's machine saw them, and which sweep that was. */
	TArray<FVector> SweepMarks;
	int32 SweepIndex = -1;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Destroyed() override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerCallStreak(uint8 Kind, FVector_NetQuantize Spot, float Yaw);
	UFUNCTION()
	void OnRep_Out();
	/** On this player's own machine: out, they watch a teammate; back in, their own eyes. */
	void ShowOut();
	/** Copies the server's record of the run to what replicates. */
	void PublishRun();

	UPROPERTY(Replicated)
	int32 Kills = 0;
	UPROPERTY(Replicated)
	int32 Deaths = 0;
	UPROPERTY(Replicated)
	int32 Assists = 0;
	UPROPERTY(Replicated)
	int32 Downs = 0;
	UPROPERTY(Replicated)
	int32 Team = 0;
	UPROPERTY(Replicated)
	int32 JoinOrder = 0;
	UPROPERTY(Replicated)
	int32 GunLevel = 0;
	UPROPERTY(Replicated)
	int32 StreakKills = 0;
	/** The streaks waiting (CheeseStrikeStreaks::EStreak), oldest first. */
	UPROPERTY(Replicated)
	TArray<uint8> ReadyStreaks;
	UPROPERTY(Replicated)
	uint8 LastEarned = 0;
	UPROPERTY(Replicated)
	float LastEarnedAt = 0.0f;
	UPROPERTY(ReplicatedUsing=OnRep_Out)
	bool bOut = false;
	UPROPERTY(Replicated)
	float RevealUntil = 0.0f;
	UPROPERTY(Replicated)
	float RevealFrom = 0.0f;
	UPROPERTY(Replicated)
	int32 HillPoints = 0;
	UPROPERTY(Replicated)
	int32 HillSeconds = 0;
	/** Kills that earned each badge (CheeseStrikeBadges::EBadge's order). */
	UPROPERTY(Replicated)
	TArray<int32> BadgeCounts;
	int32 LastKilledBy = -1;
	/** The server's count of the time in the hill, to the part of a second. */
	float HillTime = 0.0f;

	/** The server's record of the run. */
	CheeseStrikeStreaks::FRun Run;
	bool bMarkingStrike = false;
	FVector2D StrikeTarget = FVector2D::ZeroVector;
	float StrikeYaw = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<ACheeseStrikeWatchCamera> Watching;
};
