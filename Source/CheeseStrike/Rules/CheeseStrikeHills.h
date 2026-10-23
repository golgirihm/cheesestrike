// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * King of the Hill's rules, as plain functions over plain values, so that
 * they can be tested without a world: who holds the hill, the points that
 * holding earns, when the hill moves and to where, where players come back
 * (away from it), and what a bot does about it. ACheeseStrikeHill applies
 * them on the server and shows them on every machine.
 *
 * There is one hill at a time, a ring on the ground at one of the map's
 * spots (ACheeseStrikeHillSpot, placed by its script), which moves to the
 * next spot every HillTime seconds, with a warning MoveWarning seconds
 * before. In a team mode a side scores a point a second while only its
 * players stand in it; in the solo mode a player scores while they stand in
 * it alone. With anyone else in it too it is contested, and nobody scores.
 */
namespace CheeseStrikeHills
{
	/** Seconds a hill stands before it moves to the next spot, and the warning before it does. */
	constexpr float HillTime = 60.0f;
	constexpr float MoveWarning = 5.0f;
	/** A hill's usual radius (cm), unless its spot says otherwise; how far above its spot and below it someone stands in it. */
	constexpr float Radius = 400.0f;
	constexpr float Above = 300.0f;
	constexpr float Below = 120.0f;
	/** Points a second of holding earns. */
	constexpr int32 PointsPerSecond = 1;
	/** The points that win, unless the host says otherwise: more for a side than for one player. What a host may pick. */
	constexpr int32 TeamTarget = 150;
	constexpr int32 SoloTarget = 100;
	constexpr int32 TargetChoices[] = {50, 100, 150, 200, 250};
	/** Starts nearer the hill than this (cm) are passed over while any further ones are free. */
	constexpr float SpawnClear = 1800.0f;

	/** How a hill is held. */
	enum class EHold : uint8
	{
		/** Nobody in it. */
		Neutral,
		/** Only one side's players (in the solo mode, one player) in it. */
		Held,
		/** More than one side (or player) in it: nobody scores. */
		Contested,
	};

	struct FHold
	{
		EHold State = EHold::Neutral;
		/** Who holds it: the side in a team mode, the player (their id) in the solo mode; INDEX_NONE unless held. */
		int32 Holder = INDEX_NONE;

		bool operator==(const FHold& Other) const { return State == Other.State && Holder == Other.Holder; }
		bool operator!=(const FHold& Other) const { return !(*this == Other); }
	};

	/** Someone standing in the hill: their side, and who they are (a player id). */
	struct FInHill
	{
		int32 Team = 0;
		int32 Player = 0;
	};
	/** Who holds a hill with `Inside` standing in it: by side in a team mode (`bTeams`), by player in the solo mode. */
	FHold HoldOf(bool bTeams, TArrayView<const FInHill> Inside);

	/** Whether someone whose feet are at `Feet` stands in a hill of `HillRadius` at `Spot` (its middle, on the floor). */
	bool IsInside(const FVector& Feet, const FVector& Spot, float HillRadius);

	/**
	 * The points that holding earns, a whole second at a time: a second held
	 * by the same holder earns them PointsPerSecond, and any part of a
	 * second is kept toward the next while they go on holding it. Nobody
	 * holding, contested, or a new holder, starts again from nothing.
	 */
	struct FClock
	{
		FHold Last;
		float Held = 0.0f;
		/** `Seconds` more of the hill being as `Hold` says: the points they earn its holder (0 unless held). */
		int32 Advance(float Seconds, const FHold& Hold);
	};

	/** The spot after `Current` of `Count`: each in the map's order, then round again; 0 with none. */
	int32 NextSpot(int32 Current, int32 Count);
	/** Whether a hill that moves in `SecondsLeft` is warning that it will. */
	bool IsWarning(float SecondsLeft);

	/**
	 * Which of `Spots` (the starts) a player may come in at: those at least
	 * `Clear` from the hill at `Hill` (2D), and from `Next` where it is about to
	 * go (`bNext`), or all of them if none are.
	 */
	TArray<int32> AwayFromHill(TArrayView<const FVector> Spots, const FVector& Hill, float Clear, bool bNext = false, const FVector& Next = FVector::ZeroVector);

	/** What a bot does about the hill. */
	enum class EBotPlan : uint8
	{
		/** Fights as it would anywhere: someone is in its face. */
		Fight,
		/** Goes to the hill (fighting on the way at whoever it sees). */
		GoToHill,
		/** Stands in the hill: holds it, fighting from it rather than chasing anyone out of it. */
		HoldHill,
		/** Goes to where the hill is about to be. */
		GoToNext,
	};
	/** How near an enemy must be (cm) for a bot to turn from the hill to fight them. */
	constexpr float BotFightRange = 900.0f;
	/**
	 * What a bot does: in the hill, it holds it; out of it, it goes to it,
	 * unless an enemy it sees is within BotFightRange (then it fights them);
	 * while the hill warns that it is moving, a bot not in it goes on to the
	 * next spot instead.
	 */
	EBotPlan BotPlan(bool bInHill, bool bSeesEnemy, float EnemyDistance, bool bWarning);
}
