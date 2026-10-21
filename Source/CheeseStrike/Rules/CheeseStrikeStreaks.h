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
 *   - five: the Cheese Strike, a run of gouda wheels dropped along a line
 *     through a spot the player marks, after a falling whistle;
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
	/** The drone circles this high over the starts, this far out, once in so many seconds. */
	constexpr float DroneAltitude = 2500.0f;
	constexpr float DroneCircle = 24.0f;

	/** Cheese Strike: how many wheels, how far apart along the line, how far a spot may be marked from the caller (cm). */
	constexpr int32 StrikeWheels = 7;
	constexpr float StrikeSpacing = 300.0f;
	constexpr float StrikeRange = 15000.0f;
	/** Seconds from the call to the first wheel landing (the whistle), and between wheels. */
	constexpr float StrikeWarning = 2.2f;
	constexpr float StrikeInterval = 0.12f;
	/** Where the wheels land, on the ground: centred on `Spot`, along the way `Yaw` (degrees) faces. */
	TArray<FVector> StrikeLine(const FVector& Spot, float Yaw);

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
	/** Seconds a wheel takes to fall from `Height`. */
	float FallTime(float Height = DropHeight);
	/** How far above where it lands a wheel is, `Left` seconds before it lands. */
	float HeightBefore(float Left);
}
