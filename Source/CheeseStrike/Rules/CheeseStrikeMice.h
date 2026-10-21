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
	constexpr int32 MaxMice = 8;
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
	constexpr float ReachUp = 250.0f;
	/** Eating: a bite of Bite every BiteEvery seconds; a block's 400 takes ten bites, five seconds of one mouse. */
	constexpr float Bite = 40.0f;
	constexpr float BiteEvery = 0.5f;
	/** The three stages a block is drawn in as it is eaten (0 whole, 3 nearly gone), by its health out of `Full`. */
	int32 Stage(float Health, float Full);

	/** Scared: hurt by FearAt (wearing off at FearFades a second), a mouse runs for FleeFor seconds and comes back. */
	constexpr float FearAt = 300.0f;
	constexpr float FearFades = 20.0f;
	constexpr float FleeFor = 6.0f;
	/** Fear after `Seconds` of fading from `Fear`. */
	float FearAfter(float Fear, float Seconds);

	/** A mouse that makes no progress on its cheese for GiveUpAfter seconds leaves it alone for GiveUpFor. */
	constexpr float GiveUpAfter = 8.0f;
	constexpr float GiveUpFor = 30.0f;

	/** Seconds a mouse takes to come over the edge of the map, from the top of its wall to the ground. */
	constexpr float EnterTime = 1.8f;
}
