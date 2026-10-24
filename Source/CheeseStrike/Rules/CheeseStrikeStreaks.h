// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Kill streaks, as plain functions over plain values, so that they can be
 * tested without a world. Kills in a row without dying earn a streak at
 * three, five and ten, which its player calls in when they choose (the
 * Streak key, 4):
 *
 *   - three: the Unmanned Aerial Cheese, a cheese drone over the map whose
 *     sweeps show the earner's side where every enemy is, for 25 s;
 *   - five: the Cheese Strike, gouda wheels dropped over an area the
 *     player chooses on the minimap, after a falling whistle;
 *   - ten: the Gouda Overload, a siren and a warning to everyone, then
 *     wheels raining on every enemy for eight seconds; cover overhead saves
 *     them, and so does keeping on the move.
 *
 * What a streak's wheels kill counts to the earner, but not toward their
 * next streak: only their own weapon's kills make a run. Earned streaks are
 * kept, in the order they came, until they are called in, through deaths
 * and rounds. ACheeseStrikePlayerState keeps a player's count;
 * ACheeseStrikeGameMode calls them in; ACheeseStrikeBombs drops the wheels.
 */
namespace CheeseStrikeStreaks
{
	enum class EStreak : uint8
	{
		None,
		AerialCheese,
		CheeseStrike,
		GoudaOverload,
	};

	struct FStreakInfo
	{
		EStreak Kind;
		/** Kills in a row that earn it. */
		int32 Kills;
		const TCHAR* Name;
		/** How it is spoken of: "an Unmanned Aerial Cheese", "a Cheese Strike", "the Gouda Overload". */
		const TCHAR* Article;
	};
	constexpr int32 NumStreaks = 3;
	extern const FStreakInfo Streaks[NumStreaks];
	/** Its line in Streaks; for None, a line with no kills and no name. */
	const FStreakInfo& Info(EStreak Kind);
	EStreak StreakAt(int32 Index);

	/** The streak a kill earns that makes `KillsInARow`, or None. */
	EStreak EarnedAt(int32 KillsInARow);
	/** The next streak a run of `KillsInARow` is on its way to, or None past the last. */
	EStreak NextAfter(int32 KillsInARow);

	/** Whether `Name` is a streak's (a kill's How that names a streak doesn't run on the next). */
	bool IsStreakName(const FString& Name);

	/** What everyone is told: "Camem Bert called in a Cheese Strike". */
	FString CalledIn(const FString& Who, EStreak Kind);

	/** A player's run: kills in a row, and the streaks earned and not yet called in, oldest first. */
	struct FRun
	{
		int32 Kills = 0;
		TArray<EStreak> Ready;

		/** A kill: one more in the run if it `bCounts` (a weapon's, not a streak's); the streak it earns, if any, joins Ready and is returned. */
		EStreak AddKill(bool bCounts);
		/** Killed: the run starts again; what was earned is kept. */
		void Died() { Kills = 0; }
		/** The streak the key calls in next: the oldest earned, or None. */
		EStreak Next() const { return Ready.Num() > 0 ? Ready[0] : EStreak::None; }
		/** Calls in `Kind` if it is the next; false if it isn't. */
		bool Take(EStreak Kind);
	};

	/** Unmanned Aerial Cheese: how long the earner's side sees, and how often its sweep shows where the enemies are. */
	constexpr float RevealTime = 25.0f;
	constexpr float SweepPeriod = 2.0f;
	/** A side's reveal: from when (the server's time) its sweeps began, until when they show the enemy. */
	struct FReveal
	{
		float From = 0.0f;
		float Until = 0.0f;
	};
	/** Another Unmanned Aerial Cheese called in at `Now`: one still up runs `Seconds` longer, its sweeps going on; else a new one from now. */
	FReveal AddReveal(const FReveal& Running, float Now, float Seconds = RevealTime);
	/** The drone circles this high over the starts, this far out, once in so many seconds. */
	constexpr float DroneAltitude = 2500.0f;
	constexpr float DroneCircle = 24.0f;
	/**
	 * Under a roof (a map whose starts are mostly under one: a cellar, a
	 * cave, a hall, a kitchen), the drone flies under it, where it can be
	 * seen: its middle DroneUnderRoof below the lowest of the roof over its
	 * circle, shrunk to DroneIndoorScale where that is less than DroneLowRoof
	 * over the lowest start (the floor), and never lower than
	 * DroneAboveStarts over the highest. The roof over a circle is the
	 * middle one of the roofs over its points (DroneRoof), so that a deck or
	 * a tunnel's roof under part of it doesn't bring it down everywhere; a
	 * point whose roof is lower than the drone counts as a stretch that runs
	 * into something. Of the circles it could fly (FDroneWay: its radius, the
	 * roof over it, or 0 for none, and how many stretches of it run into
	 * something), it takes the one that runs into least, the first of those.
	 */
	constexpr float DroneUnderRoof = 250.0f;
	constexpr float DroneIndoorScale = 0.5f;
	constexpr float DroneLowRoof = 900.0f;
	constexpr float DroneAboveStarts = 250.0f;
	struct FDroneWay
	{
		float Radius = 0.0f;
		float Roof = 0.0f;
		int32 Blocked = 0;
	};
	/** How high the drone's middle flies, and how big it is, under `Roof` (0 for open sky) with the starts from `LowestStart` to `HighestStart`. */
	float DroneHeight(float Roof, float LowestStart, float HighestStart, float& OutScale);
	int32 ChooseDroneWay(TConstArrayView<FDroneWay> Ways);
	/** The roof over a circle, from the roofs found over its points (those with none left out); 0 for none. */
	float DroneRoof(TArray<float> Roofs);

	/**
	 * Cheese Strike: how many wheels, the radius of the area they fall on
	 * (cm; the caller chooses it on the minimap, ACheeseStrikeHUD's strike
	 * map), how far out from its middle the ring of wheels round the middle
	 * one is (a share of the radius: with StrikeBlast's reach every point of
	 * the area is in a burst), and how far a spot may be marked from the
	 * caller (cm).
	 */
	constexpr int32 StrikeWheels = 7;
	constexpr float StrikeRadius = 750.0f;
	constexpr float StrikeRing = 0.6f;
	constexpr float StrikeRange = 15000.0f;
	/** Seconds from the call to the first wheel landing (the whistle), and between wheels. */
	constexpr float StrikeWarning = 2.2f;
	constexpr float StrikeInterval = 0.12f;
	/** Where the wheels land, on the ground, in the order they fall: the first on `Spot`, the rest round it StrikeRing of StrikeRadius out, from the way `Yaw` (degrees) faces, clockwise seen from above. */
	TArray<FVector> StrikeArea(const FVector& Spot, float Yaw);
	/** How far the strike's mark on the minimap moves for a degree the view would have turned (cm), the mouse's and the stick's alike. */
	constexpr float StrikeCursorPerDegree = 35.0f;

	/** Gouda Overload: the warning before the rain, how long it rains, how often a wheel falls on each enemy, and how far from them it may land. */
	constexpr float OverloadWarning = 4.0f;
	constexpr float OverloadRain = 8.0f;
	constexpr float OverloadInterval = 0.7f;
	constexpr float OverloadScatter = 200.0f;

	/** What a wheel does where it lands: harm out to a radius, from so much at the middle down to so much at the edge, on whoever the burst can see. */
	struct FBlast
	{
		float Radius;
		float Centre;
		float Edge;
	};
	constexpr FBlast StrikeBlast{500.0f, 150.0f, 40.0f};
	constexpr FBlast OverloadBlast{450.0f, 70.0f, 10.0f};
	/** The harm `Blast` does `Distance` from where it lands: falling off in a straight line to its edge, none past it. */
	float BlastDamage(const FBlast& Blast, float Distance);

	/** A wheel falls from this high above where it lands, starting at this speed, under this gravity (cm, cm/s, cm/s²): faster than a thing let go, as a bomb is. */
	constexpr float DropHeight = 3000.0f;
	constexpr float DropSpeed = 1500.0f;
	constexpr float DropGravity = 2400.0f;
	/**
	 * Indoors, a Cheese Strike comes through the roof: a wheel whose place
	 * on the line has a roof over it within DropHeight lands on the floor
	 * under it, at about the height of the mark, bursting through the roof
	 * (a puff, and a scorch left on its underside) RoofGap over its middle
	 * and falling the rest from IndoorDropSpeed, slower than from the sky,
	 * so that it is seen falling.
	 */
	constexpr float RoofGap = 40.0f;
	constexpr float IndoorDropSpeed = 300.0f;
	/** Seconds a wheel takes to fall from `Height`, starting at `Speed`. */
	float FallTime(float Height = DropHeight, float Speed = DropSpeed);
	/** How far above where it lands a wheel falling from `Height` at `Speed` is, `Left` seconds before it lands. */
	float HeightBefore(float Left, float Height = DropHeight, float Speed = DropSpeed);
}
