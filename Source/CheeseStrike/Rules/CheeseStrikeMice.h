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
	/**
	 * Seconds into a match the mice come, and the seconds before that a red
	 * countdown shows: nothing for the first two and a half minutes, then
	 * "VERMIN INVASION IN 0:30" counting down (the user's, 24 Oct 2026; it
	 * showed the whole minute before; they came a minute in till 26 Oct
	 * 2026, two minutes in till 27 Oct 2026).
	 */
	constexpr float ArriveAfter = 180.0f;
	constexpr float Warning = 30.0f;
	/** Seconds into a match the countdown shows, for the mice coming `Arrival` seconds in: at once if that is no more than Warning. */
	float CountdownFrom(float Arrival);
	/** One mouse for every BlocksPerMouse blocks of cheese, from MinMice to MaxMice; none on a map without cheese. */
	constexpr int32 BlocksPerMouse = 25;
	constexpr int32 MinMice = 3;
	constexpr int32 MaxMice = 24;
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
	/**
	 * So that they spread over the map rather than eat in a crowd: a block
	 * costs up to Crowded (cm) more for each other mouse near it, and as much
	 * again for each other mouse's cheese near it, less the further off, none
	 * past CrowdWithin. `Others` are the other mice and their cheese, from
	 * above. The block a mouse is already going for costs it KeepGoing less,
	 * so that it doesn't turn this way and that as the others move.
	 */
	constexpr float Crowded = 1500.0f;
	constexpr float CrowdWithin = 2000.0f;
	constexpr float KeepGoing = 600.0f;
	float CrowdCost(const FVector2D& Block, TConstArrayView<FVector2D> Others);

	/** How far a mouse reaches to eat: across (cm, from its middle to the block) and up (from its feet to the block's bottom). */
	constexpr float ReachAcross = 260.0f;
	constexpr float ReachUp = 400.0f;
	/** Eating: a bite of Bite every BiteEvery seconds, at an even pace; a block's 400 takes four bites, four seconds of one mouse; how big a bite is (Pace) is what sets how long a map takes. */
	constexpr float Bite = 100.0f;
	constexpr float BiteEvery = 1.0f;
	/**
	 * The pace: every map is eaten bare about EatenWithin seconds after the
	 * mice come, steadily, whatever its size. A steady eating would leave
	 * PlannedLeft of the cheese (by health) at each moment, none at
	 * PlannedWithin (a little before EatenWithin, for the walks to the last
	 * few blocks). A mouse's bite is Bite times its Pace: the cube of the
	 * share of the cheese left over the share planned, so that behind (a big
	 * map, few mice, long walks) they bite bigger, up to MaxPace (a block at
	 * a bite), and ahead smaller, down to MinPace.
	 */
	// Half as long again as they were (328 and 248, 164 and 124, 82 and 62 before): the mice eat 50% slower (27 Oct 2026).
	// MinPace is lower with it: the slowest bite of a mouse with a map of its own to eat has to be able to take this long.
	constexpr float EatenWithin = 492.0f;
	constexpr float PlannedWithin = 372.0f;
	constexpr float MinPace = 0.15f;
	constexpr float MaxPace = 4.0f;
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
	constexpr float BoundAfter = 40.0f;

	/** Seconds a mouse takes to come over the edge of the map, from the top of its wall to the ground. */
	constexpr float EnterTime = 1.8f;

	/**
	 * Once no edible cheese is left on the map, the mice leave (the user's,
	 * 24 Oct 2026): each scurries at LeaveSpeed to the way in it can walk to
	 * soonest, climbs up onto the wall in ClimbOutTime and drops out of sight
	 * behind it in DropOutTime, then is gone. One that finds no way out
	 * within LeaveWithin seconds leaps for the nearest way out from where it
	 * is.
	 */
	constexpr float LeaveSpeed = 450.0f;
	constexpr float ClimbOutTime = 1.8f;
	constexpr float DropOutTime = 0.9f;
	constexpr float LeaveWithin = 25.0f;
	/** How far past the wall's top, outward, and below it a mouse drops out of sight (cm). */
	constexpr float DropOutward = 450.0f;
	constexpr float DropBelow = 700.0f;
	/** Whether the mice leave, with `EdibleLeft` blocks of cheese on the map they may eat. */
	bool ShouldLeave(int32 EdibleLeft);
	/**
	 * Where a leaving mouse's feet are `Seconds` after it set off over the
	 * edge from `From` (its feet, inside), to the wall's top at `Top`, then
	 * out along `Outward` (flat, away from the map) and down out of sight;
	 * unset once it is gone.
	 */
	TOptional<FVector> OverTheEdge(const FVector& From, const FVector& Top, const FVector& Outward, float Seconds);

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
