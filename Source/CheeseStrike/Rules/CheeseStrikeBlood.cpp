// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBlood.h"
#include "CheeseStrikeExecution.h"

namespace CheeseStrikeBlood
{
	FSpray ShotSpray(float Damage)
	{
		// A pistol's 25 throws a score of small drops; a sniper's 100 twice as many, bigger.
		const float Hard = FMath::Clamp(Damage / 100.0f, 0.0f, 1.0f);
		FSpray Spray;
		Spray.Drops = FMath::Clamp(FMath::RoundToInt32(10.0f + Damage * 0.35f), 10, 45);
		Spray.Speed = FMath::Lerp(240.0f, 480.0f, Hard);
		Spray.SpeedJitter = 0.45f;
		Spray.ConeDegrees = 26.0f;
		Spray.Size = FMath::Lerp(0.6f, 1.2f, Hard);
		return Spray;
	}

	int32 DropsAllowed(int32 Wanted, int32 Alive)
	{
		return FMath::Clamp(FMath::Min(Wanted, MostDrops - Alive), 0, MostDrops);
	}

	float StabPulse(float Elapsed)
	{
		using namespace CheeseStrikeExecution;
		if (Elapsed < StabLands || Elapsed >= StabOut)
		{
			return 0.0f;
		}
		// A beat at the knife going in, then on, PulsesPerSecond: each a sharp rise and quick fall.
		const float Phase = FMath::Frac((Elapsed - StabLands) * PulsesPerSecond);
		return FMath::Pow(1.0f - Phase, 3.0f);
	}

	float StabRate(float Elapsed)
	{
		using namespace CheeseStrikeExecution;
		if (Elapsed < StabLands)
		{
			return 0.0f;
		}
		if (Elapsed < StabOut)
		{
			return 6.0f + 110.0f * StabPulse(Elapsed);
		}
		// Out: a gush, dying away.
		const float Since = Elapsed - StabOut;
		if (Since >= GushSeconds)
		{
			return 0.0f;
		}
		return 260.0f * FMath::Square(1.0f - Since / GushSeconds);
	}

	float StabSpeed(float Elapsed)
	{
		using namespace CheeseStrikeExecution;
		if (Elapsed < StabOut)
		{
			return 170.0f + 140.0f * StabPulse(Elapsed);
		}
		return FMath::Lerp(420.0f, 220.0f, FMath::Clamp((Elapsed - StabOut) / GushSeconds, 0.0f, 1.0f));
	}

	float StompRate(float Elapsed)
	{
		const float Since = Elapsed - CheeseStrikeExecution::StompLands;
		if (Since < 0.0f || Since >= HoseSeconds)
		{
			return 0.0f;
		}
		// Throbbing three times a second, and slackening over its last half second.
		const float Throb = 0.78f + 0.22f * FMath::Sin(Since * 3.0f * UE_TWO_PI);
		const float Slack = FMath::Clamp((HoseSeconds - Since) / 0.5f, 0.0f, 1.0f);
		return 360.0f * Throb * Slack;
	}

	float StompSpeed(float Elapsed)
	{
		const float Since = FMath::Clamp((Elapsed - CheeseStrikeExecution::StompLands) / HoseSeconds, 0.0f, 1.0f);
		return FMath::Lerp(720.0f, 380.0f, Since);
	}

	float StompDropSize(float Elapsed)
	{
		const float Since = FMath::Clamp((Elapsed - CheeseStrikeExecution::StompLands) / HoseSeconds, 0.0f, 1.0f);
		return FMath::Lerp(2.0f, 1.4f, Since);
	}

	float PoolRadiusAt(float Since)
	{
		if (Since <= 0.0f)
		{
			return PoolStart;
		}
		// Eased out: quick at first, then slowing to its full size.
		const float Spread = 1.0f - FMath::Square(1.0f - FMath::Clamp(Since / PoolGrowSeconds, 0.0f, 1.0f));
		return FMath::Lerp(PoolStart, PoolRadius, Spread);
	}

	float PoolOpacityAt(float Since)
	{
		const float FadeFrom = PoolGrowSeconds + PoolStays;
		if (Since < FadeFrom)
		{
			return 1.0f;
		}
		return FMath::Clamp(1.0f - (Since - FadeFrom) / PoolFades, 0.0f, 1.0f);
	}

	bool IsPoolGone(float Since)
	{
		return Since >= PoolGrowSeconds + PoolStays + PoolFades;
	}

	float SplatRadius(float DropSize)
	{
		return FMath::Clamp(DropSize * 4.5f, 4.0f, 16.0f);
	}

	float SplatOpacityAt(float Since)
	{
		if (Since < SplatSeconds)
		{
			return 1.0f;
		}
		return FMath::Clamp(1.0f - (Since - SplatSeconds) / SplatFades, 0.0f, 1.0f);
	}

	bool LeavesSplat(int32 Index)
	{
		return Index % SplatEvery == 0;
	}
}
