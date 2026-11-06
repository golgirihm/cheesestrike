// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHeroTurn.h"

namespace CheeseStrikeHeroTurn
{
	float AutoShare(double SinceTurned)
	{
		if (SinceTurned < 0.0)
		{
			return 1.0f;
		}
		const float Resumed = static_cast<float>((SinceTurned - IdleSeconds) / ResumeSeconds);
		return Resumed <= 0.0f ? 0.0f : FMath::SmoothStep(0.0f, 1.0f, FMath::Min(Resumed, 1.0f));
	}

	float StickTurn(float Axis, float DeltaSeconds)
	{
		const float Size = FMath::Abs(Axis);
		if (Size <= StickDeadZone)
		{
			return 0.0f;
		}
		const float Past = FMath::Min(1.0f, (Size - StickDeadZone) / (1.0f - StickDeadZone));
		return FMath::Sign(Axis) * Past * StickDegreesPerSecond * DeltaSeconds;
	}
}
