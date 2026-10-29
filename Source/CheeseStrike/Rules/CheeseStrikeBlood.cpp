// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBlood.h"
#include "CheeseStrikeExecution.h"

namespace CheeseStrikeBlood
{
	float DropStretch(float Speed)
	{
		return 1.0f + (MostStretch - 1.0f) * FMath::Clamp(Speed / 900.0f, 0.0f, 1.0f);
	}

	float DropLength(float Size, float Speed)
	{
		return DropShapeLength * Size * DropStretch(Speed);
	}

	FSpray ShotSpray(float Damage)
	{
		// A pistol's 12 throws a score of fine drops; a sniper's 100 twice as many, faster.
		const float Hard = FMath::Clamp(Damage / 100.0f, 0.0f, 1.0f);
		FSpray Spray;
		Spray.Drops = FMath::Clamp(FMath::RoundToInt32(20.0f + Damage * 0.2f), 20, 40);
		Spray.Speed = FMath::Lerp(350.0f, 650.0f, Hard);
		Spray.SpeedJitter = 0.4f;
		Spray.ConeDegrees = 22.0f;
		Spray.Size = FMath::Lerp(0.17f, 0.2f, Hard);
		Spray.Life = ShotDropLife;
		return Spray;
	}

	int32 DropsAllowed(int32 Wanted, int32 Alive)
	{
		return FMath::Clamp(FMath::Min(Wanted, MostDrops - Alive), 0, MostDrops);
	}

	FMist ShotMist(float Damage)
	{
		const float Hard = FMath::Clamp(Damage / 100.0f, 0.0f, 1.0f);
		FMist Mist;
		Mist.From = 8.0f + 6.0f * Hard;
		Mist.To = 35.0f + 20.0f * Hard;
		Mist.Life = 0.3f;
		Mist.Opacity = 0.8f;
		return Mist;
	}

	int32 ShotMistPuffs(float Damage)
	{
		return Damage >= 20.0f ? 2 : 1;
	}

	float MistRadiusAt(const FMist& Mist, float Since)
	{
		const float Through = Mist.Life > 0.0f ? FMath::Clamp(Since / Mist.Life, 0.0f, 1.0f) : 1.0f;
		return FMath::Lerp(Mist.From, Mist.To, 1.0f - FMath::Square(1.0f - Through));
	}

	float MistOpacityAt(const FMist& Mist, float Since)
	{
		const float Through = Mist.Life > 0.0f ? FMath::Clamp(Since / Mist.Life, 0.0f, 1.0f) : 1.0f;
		return Mist.Opacity * FMath::Pow(1.0f - Through, 1.3f);
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
			return 15.0f + 500.0f * StabPulse(Elapsed);
		}
		// Out: a gush, dying away.
		const float Since = Elapsed - StabOut;
		if (Since >= GushSeconds)
		{
			return 0.0f;
		}
		return 1200.0f * FMath::Square(1.0f - Since / GushSeconds);
	}

	float StabSpeed(float Elapsed)
	{
		using namespace CheeseStrikeExecution;
		if (Elapsed < StabOut)
		{
			return 220.0f + 200.0f * StabPulse(Elapsed);
		}
		return FMath::Lerp(520.0f, 280.0f, FMath::Clamp((Elapsed - StabOut) / GushSeconds, 0.0f, 1.0f));
	}

	float StabCone(float Elapsed)
	{
		return Elapsed < CheeseStrikeExecution::StabOut ? 25.0f : 40.0f;
	}

	float StabDropSize(float Elapsed)
	{
		return Elapsed < CheeseStrikeExecution::StabOut ? 0.3f : 0.35f;
	}

	FMist StabMist()
	{
		FMist Mist;
		Mist.From = 8.0f;
		Mist.To = 45.0f;
		Mist.Life = 0.45f;
		Mist.Opacity = 0.7f;
		return Mist;
	}

	bool StabMistDue(float From, float To)
	{
		using namespace CheeseStrikeExecution;
		if (To < StabLands || From >= StabOut + GushSeconds * 0.5f)
		{
			return false;
		}
		// At each spurt's beat while the knife is in, and as it comes out (and once more in the gush).
		const auto Beat = [](float Time)
		{
			if (Time < StabLands)
			{
				return -1;
			}
			if (Time < StabOut)
			{
				return FMath::FloorToInt32((Time - StabLands) * PulsesPerSecond);
			}
			return 1000 + FMath::FloorToInt32((Time - StabOut) / (GushSeconds * 0.25f));
		};
		return Beat(From) != Beat(To);
	}

	float StompRate(float Elapsed)
	{
		const float Since = Elapsed - CheeseStrikeExecution::StompLands;
		if (Since < 0.0f || Since >= HoseSeconds)
		{
			return 0.0f;
		}
		// Throbbing three times a second, and slackening over its last half second.
		const float Throb = 0.9f + 0.1f * FMath::Sin(Since * 3.0f * UE_TWO_PI);
		const float Slack = FMath::Clamp((HoseSeconds - Since) / 0.5f, 0.0f, 1.0f);
		return 520.0f * Throb * Slack;
	}

	float StompSpeed(float Elapsed)
	{
		const float Since = FMath::Clamp((Elapsed - CheeseStrikeExecution::StompLands) / HoseSeconds, 0.0f, 1.0f);
		return FMath::Lerp(1150.0f, 850.0f, Since);
	}

	float StompDropSize(float Elapsed)
	{
		const float Since = FMath::Clamp((Elapsed - CheeseStrikeExecution::StompLands) / HoseSeconds, 0.0f, 1.0f);
		return FMath::Lerp(0.45f, 0.35f, Since);
	}

	FMist HoseMist()
	{
		FMist Mist;
		Mist.From = 10.0f;
		Mist.To = 45.0f;
		Mist.Life = 0.4f;
		Mist.Opacity = 0.55f;
		return Mist;
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
		return FMath::Clamp(DropSize * 12.0f, 3.0f, 10.0f);
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
