// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The cheese the players bleed: yellow drops of molten cheese that spray
 * from a hit along the shot, spurt from a stab, gush from a stomped body
 * and pool on the floor. Only for show, on every machine that draws (never a
 * dedicated server), by ACheeseStrikeCheeseBlood (Effects/). Its numbers, as
 * plain arithmetic tested without a world (CheeseStrike.Blood.*): how many
 * drops a hit throws and how fast, how hard an execution's wound runs at each
 * moment, how a pool spreads and fades, and the caps that keep it cheap.
 *
 * Lengths are centimetres, speeds cm/s, times seconds.
 */
namespace CheeseStrikeBlood
{
	/** The most drops in the air at once, splats on the floors and walls, and pools, on a machine. */
	constexpr int32 MostDrops = 700;
	constexpr int32 MostSplats = 48;
	constexpr int32 MostPools = 4;
	/** A drop falls by the world's gravity, and is gone after DropLife if it lands on nothing. */
	constexpr float Gravity = 980.0f;
	constexpr float DropLife = 2.5f;

	/** One burst of drops: how many, how fast (and how much faster or slower each may be), how wide a cone, how big (a radius). */
	struct FSpray
	{
		int32 Drops = 0;
		float Speed = 0.0f;
		float SpeedJitter = 0.0f;
		float ConeDegrees = 0.0f;
		float Size = 0.0f;
	};

	/** A hit for `Damage`: a short spray along the shot, more, bigger and faster drops the harder it hit. */
	FSpray ShotSpray(float Damage);
	/** How many of `Wanted` drops may be made with `Alive` already in the air (never past MostDrops). */
	int32 DropsAllowed(int32 Wanted, int32 Alive);

	/**
	 * The stab's wound, `Elapsed` seconds into the execution (CheeseStrikeExecution's times): nothing until the knife is
	 * in; then spurts, a few a second, while it is held there; and as it comes out a gush, which dies away over
	 * GushSeconds. Drops a second, and how fast they leave.
	 */
	constexpr float PulsesPerSecond = 2.6f;
	constexpr float GushSeconds = 0.7f;
	float StabRate(float Elapsed);
	float StabSpeed(float Elapsed);
	/** Whether the stab's spurts are at a beat (a pulse at its strongest): for the tests, and the gush's start. */
	float StabPulse(float Elapsed);

	/**
	 * The stomp: as the boot lands, a firehose of cheese from the body for HoseSeconds, throbbing and slackening toward its
	 * end. Drops a second, how fast, and how big.
	 */
	constexpr float HoseSeconds = 2.0f;
	float StompRate(float Elapsed);
	float StompSpeed(float Elapsed);
	float StompDropSize(float Elapsed);

	/**
	 * A pool where the stomped body lies: spreads out from PoolStart to PoolRadius over PoolGrowSeconds (quick at first,
	 * then slowing, as a liquid does), stays for PoolStays, then fades over PoolFades.
	 */
	constexpr float PoolStart = 15.0f;
	constexpr float PoolRadius = 130.0f;
	constexpr float PoolGrowSeconds = 3.0f;
	constexpr float PoolStays = 7.0f;
	constexpr float PoolFades = 2.5f;
	float PoolRadiusAt(float Since);
	float PoolOpacityAt(float Since);
	bool IsPoolGone(float Since);

	/** A drop that lands leaves a splat of about this radius for its size, which lasts SplatSeconds and fades over SplatFades. */
	constexpr float SplatSeconds = 6.0f;
	constexpr float SplatFades = 1.5f;
	float SplatRadius(float DropSize);
	float SplatOpacityAt(float Since);
	/** Whether a drop of `Index` (any running count) leaves a splat: one in SplatEvery. */
	constexpr int32 SplatEvery = 3;
	bool LeavesSplat(int32 Index);
}
