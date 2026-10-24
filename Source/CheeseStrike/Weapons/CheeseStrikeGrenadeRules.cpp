// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenadeRules.h"

namespace CheeseStrikeGrenades
{
	const TCHAR* const Name = TEXT("Cheese Grenade");
	const TCHAR* const OwnWords = TEXT("blew themself up");

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

	EHeld HeldThrow(bool bKeyHeld, bool bCanThrow, bool bChangedWeapon)
	{
		if (!bCanThrow || bChangedWeapon)
		{
			return EHeld::PutAway;
		}
		return bKeyHeld ? EHeld::Holding : EHeld::Throw;
	}

	TArray<FVector> PreviewPoints(TConstArrayView<FVector> Path, float Spacing, float Skip, int32 MostDots)
	{
		TArray<FVector> Points;
		if (Path.Num() < 2 || Spacing <= 0.0f || MostDots <= 0)
		{
			return Points;
		}
		// Walking along the path, a dot each time another `Spacing` has gone by.
		float Along = 0.0f;
		float Next = Skip;
		for (int32 Index = 1; Index < Path.Num() && Points.Num() < MostDots; ++Index)
		{
			const FVector From = Path[Index - 1];
			const float Length = FVector::Dist(From, Path[Index]);
			while (Length > 0.0f && Next <= Along + Length && Points.Num() < MostDots)
			{
				Points.Add(FMath::Lerp(From, Path[Index], (Next - Along) / Length));
				Next += Spacing;
			}
			Along += Length;
		}
		return Points;
	}

	float BotLandingShort(float Distance)
	{
		// The faster it comes down, the further it goes on: a quarter of the way, at most four metres, and never so short
		// that it comes down within reach of the bot itself, which its own grenade hurts.
		return FMath::Clamp(FMath::Min(Distance * 0.25f, 400.0f), 0.0f, FMath::Max(0.0f, Distance - BotOwnMargin));
	}

	bool BotGetsAway(float Distance, bool bCanHurt, bool bBurst)
	{
		return bCanHurt && !bBurst && Distance < BotKeepsAway;
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
