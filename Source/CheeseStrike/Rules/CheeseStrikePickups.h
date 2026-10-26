// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeModes.h"

/**
 * Weapon pickups, as plain functions over plain values (the user's,
 * October 2026). Everyone comes in with a weak primary, the Colby Carbine
 * (CheeseStrikeArsenal::DefaultPrimary), the Curd Pistol and the knife;
 * each map has four to eight stands at fixed spots, each holding one
 * primary floating over it, the better weapons at the riskier spots, both
 * sides reaching them alike. A pickup replaces the primary carried, which
 * drops to the floor with its rounds (the spawn primary too), where anyone
 * may take it as they would a stand's, for DroppedSeconds, at most
 * MostDropped lying about at once (the design's twenty seconds and
 * twelve); the sidearm and the knife stay. Taken, a stand is empty for
 * RespawnSeconds, showing a ghost of its weapon and the seconds left, then
 * its weapon is back; a weapon on the floor is gone. Gun Game has none.
 *
 * Nothing is ever taken by walking over it (the user's, October 2026):
 * a weapon is taken by pressing Interact (E, LB) near it, the screen
 * saying what it would be swapped for (HowToTake); pressed over one's own
 * weapon, its rounds top up those carried, if they aren't full.
 *
 * ACheeseStrikePickup is the stand, or the weapon on the floor, owned by
 * the server; the inventory swaps the primary for a press of Interact
 * (UCheeseStrikeInventory::PressForPickup, TakePickup); the HUD draws the
 * prompt, the seconds and the stands on the minimap; bots go for a better
 * weapon near them (BotWants) and press Interact there as players do.
 */
namespace CheeseStrikePickups
{
	/** Seconds a stand stays empty once taken. */
	constexpr float RespawnSeconds = 30.0f;
	/** How near (cm, across) a bot goes to a stand before pressing Interact for it. */
	constexpr float Reach = 65.0f;
	/** And how far above or below its floor (cm): 2 m up. */
	constexpr float ReachUp = 200.0f;
	/** How near to press Interact for it (cm, across). */
	constexpr float PressReach = 140.0f;
	/** The server's allowance on the player's machine's reach, for its lag (cm). */
	constexpr float ServerSlack = 60.0f;
	/** How high its weapon floats over its floor (cm), how far it bobs and how fast it turns (degrees a second). */
	constexpr float FloatHeight = 85.0f;
	constexpr float Bob = 5.0f;
	constexpr float TurnRate = 70.0f;
	/** How far a bot goes out of its way for a better weapon (cm), and how far off it sees the HUD's seconds over a stand. */
	constexpr float BotRange = 2500.0f;
	constexpr float SecondsShownWithin = 2500.0f;

	/** What pressing Interact over a weapon does, by what is carried: nothing, swaps the primary for it, or takes its rounds. */
	enum class ETake : uint8
	{
		None,
		Swap,
		TopUp,
	};

	/** Seconds a weapon dropped for another lies on the floor before it goes, and the most lying about at once (the oldest go first). */
	constexpr float DroppedSeconds = 20.0f;
	constexpr int32 MostDropped = 12;
	/** In its last DroppedBlink seconds a weapon on the floor blinks, BlinksPerSecond times a second. */
	constexpr float DroppedBlink = 3.0f;
	constexpr float BlinksPerSecond = 4.0f;
	/** Whether a weapon on the floor `Age` seconds is shown now: always, but for the off half of each blink near its end. */
	bool DroppedShown(float Age);
	/** How many of the `Lying` weapons on the floor go (the oldest) to make room for one more. */
	int32 ToClearForDrop(int32 Lying);
	/** Whether `Weapon` can be dropped for another: any primary, the spawn primary too. */
	bool CanBeDropped(FName Weapon);

	/** The primary everyone comes in with. */
	FName SpawnPrimary();
	/** Whether `Mode` has pickups: every mode but Gun Game, which gives its weapons by the ladder. */
	bool InMode(CheeseStrikeModes::EMode Mode);
	/** Whether `Weapon` can lie on a stand: a primary that isn't the spawn primary. */
	bool CanLieOnAStand(FName Weapon);

	/**
	 * What pressing Interact over `Offered` (on a stand, or on the floor,
	 * `bDropped`) does for someone carrying `Carried` as their primary (none
	 * for no primary), whose rounds for it are full or not: one's own
	 * weapon tops up its rounds unless they are full; any other primary is
	 * swapped for it. A stand never holds the spawn primary; the floor may.
	 */
	ETake HowToTake(FName Carried, FName Offered, bool bRoundsFull, bool bDropped = false);
	/** The rounds taken by topping up from a stand (its weapon's usual reserve, `StandReserve`) or from a weapon on the floor (all it has). */
	int32 TopUpRounds(bool bDropped, int32 StandReserve, int32 DroppedMagazine, int32 DroppedReserve);
	/** Whether someone may take anything now: alive, free to act (not held by a count), not down, and allowed their primary (Execution's last stand allows only the sidearm). */
	bool MayTake(bool bAlive, bool bCanAct, bool bDowned, bool bPrimaryAllowed);
	/** Whether feet at `Feet` are within `Across` (cm) of a stand whose floor is at `Stand`, and within ReachUp above or below it. */
	bool InReach(const FVector& Stand, const FVector& Feet, float Across);

	/** What a primary is worth, 0 the spawn primary to 3 the Roquefort: the better ones lie at the riskier spots, and bots want them more. */
	int32 Worth(FName Weapon);
	/** The colour of a weapon's stand pad, prompt and label: the same green (not the cheese yellow) for every weapon, whatever it is worth. */
	FLinearColor WorthColour(int32 Worth);
	/** Seconds until a stand taken until `BackAt` is back, at `Now` (both the server's); 0 if it is there. */
	float SecondsLeft(float Now, float BackAt);
	/** The seconds the HUD shows over a stand with `Left` to go: whole seconds, rounded up. */
	int32 ShownSeconds(float Left);

	/**
	 * How much a bot carrying `Carried` wants a stand holding `Offered`
	 * `Distance` cm away: 0 for not at all (no better than what it has,
	 * or too far), else more for a better weapon nearer.
	 */
	float BotWants(FName Carried, FName Offered, float Distance);

	/** A stand on a map, for checking how the stands are spread: where it is and what it holds. */
	struct FSpot
	{
		FVector At = FVector::ZeroVector;
		FName Weapon;
	};
	/** The fewest and most stands a map has. */
	constexpr int32 FewestSpots = 4;
	constexpr int32 MostSpots = 8;
	/**
	 * Whether `Spots` are fair to both sides, whose starts are `Gouda` and
	 * `Mozza`: every stand is about as near to one side's nearest start as
	 * to the other's (within `Tolerance` of the two together), or another
	 * holding the same weapon is as near to the other side as it is to the
	 * one. `Why` names a stand that isn't.
	 */
	bool AreFair(const TArray<FSpot>& Spots, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza, FString& Why, float Tolerance = 0.12f);
	/**
	 * How contested a stand at `At` is, 0 at a side's nearest start to 1
	 * as near to both sides: the nearer side's distance over the mean of
	 * the two. The riskier spots are the contested ones.
	 */
	float Contest(const FVector& At, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza);
	/** Whether the better weapons lie at the riskier spots: the stands of the best worth on the map are, on average, no less contested than those of the least. */
	bool BetterLieRiskier(const TArray<FSpot>& Spots, const TArray<FVector>& Gouda, const TArray<FVector>& Mozza, FString& Why);
}
