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
 * is gone (never dropped); the sidearm and the knife stay. Taken, a stand
 * is empty for RespawnSeconds, showing a ghost of its weapon and the
 * seconds left, then its weapon is back. Gun Game has none.
 *
 * How a weapon is taken depends on what is carried (HowToTake): walking
 * over it takes it from someone holding only the spawn primary, since
 * nothing is lost; anyone holding a better one presses Interact (E, LB)
 * near it, told on the screen what they would swap to, so that nobody
 * swaps by walking past; walking over one's own weapon tops up its
 * rounds, if they aren't full.
 *
 * ACheeseStrikePickup is the stand, owned by the server; the inventory
 * swaps the primary (UCheeseStrikeInventory::TakePickup); the HUD draws the
 * prompt, the seconds and the stands on the minimap; bots go for a better
 * weapon near them (BotWants).
 */
namespace CheeseStrikePickups
{
	/** Seconds a stand stays empty once taken. */
	constexpr float RespawnSeconds = 30.0f;
	/** How near (cm, across) the feet must come to walk over it: the design's reach, 1.3 m across. */
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

	/** How a weapon on a stand is taken, by what is carried: not at all, by walking over it, by pressing Interact, or its rounds by walking over it. */
	enum class ETake : uint8
	{
		None,
		WalkOver,
		Press,
		TopUp,
	};

	/** The primary everyone comes in with. */
	FName SpawnPrimary();
	/** Whether `Mode` has pickups: every mode but Gun Game, which gives its weapons by the ladder. */
	bool InMode(CheeseStrikeModes::EMode Mode);
	/** Whether `Weapon` can lie on a stand: a primary that isn't the spawn primary. */
	bool CanLieOnAStand(FName Weapon);

	/**
	 * How `Offered` is taken by someone carrying `Carried` as their primary
	 * (none for no primary), whose rounds for it are full or not: walking
	 * over it with only the spawn primary or none; walking over one's own
	 * weapon tops it up unless it is full; any other primary is swapped
	 * by pressing Interact.
	 */
	ETake HowToTake(FName Carried, FName Offered, bool bRoundsFull);
	/** Whether someone may take anything now: alive, free to act (not held by a count), not down, and allowed their primary (Execution's last stand allows only the sidearm). */
	bool MayTake(bool bAlive, bool bCanAct, bool bDowned, bool bPrimaryAllowed);
	/** Whether feet at `Feet` are within `Across` (cm) of a stand whose floor is at `Stand`, and within ReachUp above or below it. */
	bool InReach(const FVector& Stand, const FVector& Feet, float Across);

	/** What a primary is worth, 0 the spawn primary to 3 the Roquefort: the better ones lie at the riskier spots, and bots want them more. */
	int32 Worth(FName Weapon);
	/** The colour of a weapon worth `Worth`, on its stand's pad and the minimap: green, blue, then the cheese's gold for the best. */
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
