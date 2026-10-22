// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The mice, as plain functions over plain values, tested without a world
 * (docs/unreal/design.md, "The mice"; its numbers are Godot metres, three
 * times life size, divided by three here): when they come and how many,
 * which cheese a mouse goes for, how it eats, and how it is scared off.
 * ACheeseStrikeMouse plays by them, ACheeseStrikeCheese is eaten by them, and
 * ACheeseStrikeGameMode brings them in.
 */
namespace CheeseStrikeMice
{
	/** Seconds into a match the mice come, and the seconds before that a red countdown shows. */
	constexpr float ArriveAfter = 60.0f;
	constexpr float Warning = 10.0f;
	/** One mouse for every BlocksPerMouse blocks of cheese, from MinMice to MaxMice; none on a map without cheese. */
	constexpr int32 BlocksPerMouse = 60;
	constexpr int32 MinMice = 2;
	constexpr int32 MaxMice = 10;
	int32 MiceFor(int32 Blocks);
	/** Whether a match with these URL `Options` has mice (`?Mice=0` for none), and when they come (`?MiceAt=` seconds into it, else ArriveAfter). */
	bool HasMice(const FString& Options);
	float ArrivalFrom(const FString& Options);

	/** How fast a mouse walks, and runs when scared (cm/s). */
	constexpr float WalkSpeed = 267.0f;
	constexpr float FleeSpeed = 600.0f;
	/** A mouse is this many times as long as the rat it is made from (Content/Fab/Nestaeric/BlackRat): car-sized. */
	constexpr float Scale = 15.0f;

	/** Which cheese a mouse goes for: the cheapest, where height up costs HeightCost times its distance, and cheese another mouse has costs Claimed (cm) more. */
	constexpr float HeightCost = 4.0f;
	constexpr float Claimed = 1333.0f;
	float CheeseCost(float Distance, float Height, bool bClaimedByAnother);

	/** How far a mouse reaches to eat: across (cm, from its middle to the block) and up (from its feet to the block's bottom). */
	constexpr float ReachAcross = 260.0f;
	constexpr float ReachUp = 400.0f;
	/** Eating: a bite of Bite every BiteEvery seconds, at an even pace; a block's 400 takes ten bites, five seconds of one mouse. */
	constexpr float Bite = 40.0f;
	constexpr float BiteEvery = 0.5f;
	/**
	 * The pace: every map is eaten bare about EatenWithin seconds after the
	 * mice come, steadily, whatever its size. A steady eating would leave
	 * PlannedLeft of the cheese (by health) at each moment, none at
	 * PlannedWithin (a little before EatenWithin, for the walks to the last
	 * few blocks). A mouse's bite is Bite times its Pace: the cube of the
	 * share of the cheese left over the share planned, so that behind (a big
	 * map, few mice, long walks) they bite bigger, up to MaxPace, and ahead
	 * smaller, down to MinPace.
	 */
	constexpr float EatenWithin = 300.0f;
	constexpr float PlannedWithin = 220.0f;
	constexpr float MinPace = 0.25f;
	constexpr float MaxPace = 10.0f;
	float PlannedLeft(float SecondsIn);
	float Pace(float ShareLeft, float SecondsIn);
	/** A bite, `SecondsIn` after the mice came, with `ShareLeft` (0 to 1) of the edible cheese left. */
	float BiteFor(float ShareLeft, float SecondsIn);

	/** The three stages a block is drawn in as it is eaten (0 whole, 3 nearly gone), by its health out of `Full`. */
	int32 Stage(float Health, float Full);

	/** Scared: hurt by FearAt (wearing off at FearFades a second), a mouse runs for FleeFor seconds and comes back. */
	constexpr float FearAt = 300.0f;
	constexpr float FearFades = 20.0f;
	constexpr float FleeFor = 6.0f;
	/** Fear after `Seconds` of fading from `Fear`. */
	float FearAfter(float Fear, float Seconds);

	/**
	 * After a scare, a mouse goes off to eat somewhere else, the far side of
	 * the map if it can: for AvoidFor seconds, or until it has eaten
	 * AvoidUntilEaten blocks, a block costs it ScareCost more: AvoidSide (cm)
	 * if it is on the side of the map's middle that the scare was on, and up
	 * to AvoidNear the nearer it is to where it was scared. It runs off away
	 * from the shooter and from where it was eating.
	 */
	constexpr float AvoidFor = 60.0f;
	constexpr int32 AvoidUntilEaten = 4;
	constexpr float AvoidSide = 5000.0f;
	constexpr float AvoidNear = 2500.0f;
	bool StillAvoids(float SecondsSinceScare, int32 EatenSinceScare);
	float ScareCost(const FVector2D& Block, const FVector2D& ScaredAt, const FVector2D& Middle);

	/** A mouse that makes no progress on its cheese for GiveUpAfter seconds leaves it alone for GiveUpFor. */
	constexpr float GiveUpAfter = 8.0f;
	constexpr float GiveUpFor = 30.0f;

	/**
	 * Cheese a mouse can't walk to (an island of floor, a block standing
	 * alone, an upper floor no stairs lead to): it walks as near as it can and
	 * leaps the rest, across a gap or up as far as it reaches, onto a block
	 * standing alone to eat it from on top. From BoundAfter seconds after the
	 * mice came, a mouse with nothing it can walk or leap to bounds straight
	 * to the nearest such cheese, so that every map is eaten bare.
	 */
	constexpr float BoundAfter = 180.0f;

	/** Seconds a mouse takes to come over the edge of the map, from the top of its wall to the ground. */
	constexpr float EnterTime = 1.8f;

	/**
	 * Whether a mouse may go for a block: one still there and not falling, and
	 * not one the map marks to be left (ACheeseStrikeCheese::IsEdible): the
	 * walkable way through a map, and what its starts stand on, are never
	 * eaten (docs/unreal/maps/principles.md).
	 */
	bool MayEat(bool bEdible, float Health, bool bFalling);

	/**
	 * Whether what a block rests on still holds it up, when something under it
	 * is eaten or falls: `Footprint` is the block seen from above, `Contacts`
	 * the parts of it that still rest on something (its overlap with each
	 * thing under it). It is held if its middle is inside them all taken
	 * together (their convex hull), by at least HeldMargin of its narrower
	 * side: on a block under its middle, or on walls either side of it (a
	 * lintel over a door, a roof over a room, a plank over a canal), but not on
	 * one wall at its edge, nor half on a block beside its middle. What it
	 * touches over less than HeldThinnest (cm) either way, a door's jamb or a
	 * rail, doesn't hold it.
	 */
	constexpr float HeldMargin = 0.1f;
	constexpr float HeldThinnest = 15.0f;
	bool IsHeld(const FBox2D& Footprint, TConstArrayView<FBox2D> Contacts);
}
