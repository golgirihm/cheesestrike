// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenadeRules.h"

namespace CheeseStrikeGrenades
{
	const TCHAR* const Name = TEXT("Cheese Grenade");

	float DamageAt(float Distance)
	{
		if (Distance < 0.0f || Distance > Radius)
		{
			return Distance < 0.0f ? Most : 0.0f;
		}
		return FMath::Lerp(Most, Least, Distance / Radius);
	}

	FVector LaunchVelocity(const FVector& Aim, const FVector& CarrierVelocity)
	{
		const FVector Along = Aim.GetSafeNormal();
		// Lifted toward the vertical by ThrowLift degrees, never past it.
		const FRotator Rotation = Along.Rotation();
		const float Pitch = FMath::Min(Rotation.Pitch + ThrowLift, 89.0f);
		const FVector Lifted = FRotator(Pitch, Rotation.Yaw, 0.0f).Vector();
		return Lifted * ThrowSpeed + CarrierVelocity * CarriedSpeedShare;
	}

	bool PitchToReach(float Across, float Rise, float Speed, float Gravity, float& OutPitch)
	{
		if (Across <= 1.0f || Speed <= 0.0f || Gravity <= 0.0f)
		{
			return false;
		}
		// tan(a) = (v² - sqrt(v⁴ - g(g x² + 2 y v²))) / (g x): the lower arc.
		const double V2 = static_cast<double>(Speed) * Speed;
		const double Inside = V2 * V2 - Gravity * (static_cast<double>(Gravity) * Across * Across + 2.0 * Rise * V2);
		if (Inside < 0.0)
		{
			return false;
		}
		OutPitch = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan((V2 - FMath::Sqrt(Inside)) / (Gravity * Across))));
		return true;
	}

	float BotLandingShort(float Distance)
	{
		// The faster it comes down, the further it goes on: a quarter of the way, at most four metres.
		return FMath::Min(Distance * 0.25f, 400.0f);
	}

	bool BotShouldThrow(const FBotThrow& Thought)
	{
		if (Thought.Carried <= 0 || Thought.SinceLastThrow < BotInterval || Thought.SinceSeen > BotMemory)
		{
			return false;
		}
		if (Thought.Distance < BotNearest || Thought.Distance > BotFarthest)
		{
			return false;
		}
		return Thought.Roll < (Thought.bSeesThem ? BotChanceInSight : BotChanceHidden);
	}
}
