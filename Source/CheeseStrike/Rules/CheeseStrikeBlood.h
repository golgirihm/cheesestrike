// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The cheese the players bleed, read as a liquid: fine drops of molten
 * cheese, each drawn out along its flight the faster it goes, and soft
 * puffs of mist where it leaves the body. A hit throws a brief misty spray
 * along the shot, a stab a big fan of spray and spurts and a pool, a stomp
 * a fire-hydrant jet from the head and a pool. Only for show, on every
 * machine that draws (never a dedicated server), by ACheeseStrikeCheeseBlood
 * (Effects/). Its numbers, as plain arithmetic tested without a world
 * (CheeseStrike.Blood.*): how many drops a hit throws, how fast and how
 * big, how they stretch, how the mist grows and fades, how hard an
 * execution's wound runs at each moment, how a pool spreads and fades, and
 * the caps that keep it cheap.
 *
 * Lengths are centimetres, speeds cm/s, times seconds.
 */
namespace CheeseStrikeBlood
{
	/** The most drops in the air at once (the stomp's jet keeps a thousand up), splats on the floors and walls, pools, and puffs of mist, on a machine. */
	constexpr int32 MostDrops = 2000;
	constexpr int32 MostSplats = 48;
	constexpr int32 MostPools = 4;
	constexpr int32 MostMist = 64;
	/** A drop falls by the world's gravity, and is gone after its life (DropLife unless its spray says) if it lands on nothing. */
	constexpr float Gravity = 980.0f;
	constexpr float DropLife = 2.5f;

	/**
	 * One burst of drops: how many, how fast (and how much faster or slower each may be), how wide a cone, how big (the
	 * drop's radius: its length is DropLength of it), and how long each lives.
	 */
	struct FSpray
	{
		int32 Drops = 0;
		float Speed = 0.0f;
		float SpeedJitter = 0.0f;
		float ConeDegrees = 0.0f;
		float Size = 0.0f;
		float Life = DropLife;
	};

	/** Each drop's size is its spray's times somewhere from 1 - SizeJitter to 1 + SizeJitter. */
	constexpr float SizeJitter = 0.25f;
	/** The drop's shape: a teardrop DropShapeLength radii long, its round head ahead (SM_CheeseDrop). */
	constexpr float DropShapeLength = 3.0f;
	/**
	 * How much longer than its shape a drop is drawn at `Speed`: not at all at rest, up to MostStretch times as it flies
	 * fast, so that a spray reads as streaks of liquid, not balls.
	 */
	constexpr float MostStretch = 2.5f;
	float DropStretch(float Speed);
	/** How long a drop of radius `Size` is drawn flying at `Speed`, nose to tail. */
	float DropLength(float Size, float Speed);

	/**
	 * A hit for `Damage`: a brief misty spray along the shot: 20 to 40 tiny drops (none longer than 2 cm), gone in
	 * ShotDropLife, more and faster the harder it hit.
	 */
	constexpr float ShotDropLife = 0.4f;
	/** How far through the body from where a shot went in it comes out (cm): the spray and its mist leave there. */
	constexpr float ExitDepth = 25.0f;
	FSpray ShotSpray(float Damage);
	/** How many of `Wanted` drops may be made with `Alive` already in the air (never past MostDrops). */
	int32 DropsAllowed(int32 Wanted, int32 Alive);

	/** A puff of mist: grows from a radius of From to To over Life, fading out as it goes, from Opacity. */
	struct FMist
	{
		float From = 0.0f;
		float To = 0.0f;
		float Life = 0.0f;
		float Opacity = 0.0f;
	};
	/** A hit's: a soft puff where it went in, and for a harder hit (ShotMistPuffs) one where it came out, a third of a second. */
	FMist ShotMist(float Damage);
	int32 ShotMistPuffs(float Damage);
	/** How big a puff is, and how thick, `Since` it was made: growing quickly then slowly, thinning to nothing at its end. */
	float MistRadiusAt(const FMist& Mist, float Since);
	float MistOpacityAt(const FMist& Mist, float Since);

	/**
	 * The stab's wound, `Elapsed` seconds into the execution (CheeseStrikeExecution's times): nothing until the knife is
	 * in; then spurts, a few a second, while it is held there; and as it comes out a gush, which dies away over
	 * GushSeconds: hundreds of fine drops in a fan. Drops a second, how fast they leave, how wide a fan, and how big.
	 */
	constexpr float PulsesPerSecond = 2.6f;
	constexpr float GushSeconds = 0.7f;
	float StabRate(float Elapsed);
	float StabSpeed(float Elapsed);
	float StabCone(float Elapsed);
	float StabDropSize(float Elapsed);
	/** Whether the stab's spurts are at a beat (a pulse at its strongest): for the tests, and the gush's start. */
	float StabPulse(float Elapsed);
	/** The stab's mist: a puff at each spurt and the gush (StabMistDue says when one is due between two moments). */
	FMist StabMist();
	bool StabMistDue(float From, float To);

	/**
	 * The stomp: as the boot lands, a fire-hydrant jet from where the head was for HoseSeconds: a dense stream of fine
	 * drops, never fewer than 400 a second until it slackens at its end, at 8 to 12 m/s in a narrow cone (HoseCone),
	 * falling as they fly, with a sheath of mist at its source. Drops a second, how fast, and how big.
	 */
	constexpr float HoseSeconds = 2.0f;
	constexpr float HoseCone = 7.0f;
	float StompRate(float Elapsed);
	float StompSpeed(float Elapsed);
	float StompDropSize(float Elapsed);
	/** The jet's sheath of mist: a puff every HoseMistEvery seconds while it runs. */
	constexpr float HoseMistEvery = 0.08f;
	FMist HoseMist();

	/**
	 * A pool where the stomped or stabbed body lies: spreads out from PoolStart to PoolRadius over PoolGrowSeconds (quick at
	 * first, then slowing, as a liquid does), stays for PoolStays, then fades over PoolFades.
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
	/** Whether a drop of `Index` (any running count) leaves a splat: one in SplatEvery (the drops are many and fine). */
	constexpr int32 SplatEvery = 6;
	bool LeavesSplat(int32 Index);
}
