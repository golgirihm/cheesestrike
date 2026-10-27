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

	float SwitchStep(float Switch, bool bBusy, float DeltaSeconds)
	{
		const float Rate = 1.0f / (Switch < 1.0f ? PutDownSeconds : TakeUpSeconds);
		const float Wanted = bBusy ? 2.0f : 0.0f;
		// Never past the middle in one step, so that each half takes its own time.
		const float Step = FMath::FInterpConstantTo(Switch, Wanted, DeltaSeconds, Rate);
		return (Switch < 1.0f) != (Step < 1.0f) && Switch != 1.0f ? 1.0f : Step;
	}

	float WeaponDown(float Switch)
	{
		return FMath::Clamp(Switch, 0.0f, 1.0f);
	}

	float GrenadeUp(float Switch)
	{
		return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Switch - 1.0f, 0.0f, 1.0f));
	}

	bool IsWeaponAway(float Switch, bool bThrowing)
	{
		return bThrowing || Switch >= 1.0f;
	}

	namespace
	{
		/** One pose of the throw: when (its phase), and the hand then. */
		struct FThrowKey
		{
			float Phase;
			FVector At;
			FVector Fingers;
			FVector Palm;
		};
		// Held; wound back by the ear, the palm forward; let go ahead of the eyes, the fingers forward; across and down,
		// out of sight.
		const FThrowKey ThrowKeys[] = {
			{0.0f, FVector(36.0f, 17.0f, -13.0f), FVector(0.45f, -0.35f, 0.8f), FVector(-0.3f, -0.9f, 0.1f)},
			{0.28f, FVector(0.0f, 26.0f, 6.0f), FVector(-0.45f, 0.1f, 0.9f), FVector(0.85f, -0.2f, 0.3f)},
			{ReleasePhase, FVector(30.0f, 18.0f, -9.0f), FVector(0.85f, -0.1f, 0.5f), FVector(0.5f, -0.2f, -0.8f)},
			{0.68f, FVector(48.0f, 2.0f, -32.0f), FVector(0.6f, -0.45f, -0.65f), FVector(-0.5f, -0.5f, -0.6f)},
			{1.0f, FVector(28.0f, 4.0f, -62.0f), FVector(0.25f, -0.2f, -0.95f), FVector(-0.8f, -0.5f, 0.1f)},
		};

		FHandAt HandOf(const FThrowKey& Key)
		{
			FHandAt Hand;
			Hand.At = Key.At;
			Hand.Fingers = Key.Fingers.GetSafeNormal();
			// The palm square to the fingers.
			Hand.Palm = (Key.Palm - Hand.Fingers * FVector::DotProduct(Key.Palm, Hand.Fingers)).GetSafeNormal();
			return Hand;
		}
	}

	FHandAt HeldHand()
	{
		return HandOf(ThrowKeys[0]);
	}

	FHandAt ThrowHand(float Phase)
	{
		const float At = FMath::Clamp(Phase, 0.0f, 1.0f);
		int32 Key = 0;
		while (Key < UE_ARRAY_COUNT(ThrowKeys) - 2 && At > ThrowKeys[Key + 1].Phase)
		{
			++Key;
		}
		const FHandAt From = HandOf(ThrowKeys[Key]);
		const FHandAt To = HandOf(ThrowKeys[Key + 1]);
		const float Along = FMath::SmoothStep(0.0f, 1.0f, (At - ThrowKeys[Key].Phase) / FMath::Max(ThrowKeys[Key + 1].Phase - ThrowKeys[Key].Phase, KINDA_SMALL_NUMBER));
		const FQuat FromTurn = FRotationMatrix::MakeFromXZ(From.Fingers, From.Palm).ToQuat();
		const FQuat ToTurn = FRotationMatrix::MakeFromXZ(To.Fingers, To.Palm).ToQuat();
		const FQuat Turn = FQuat::Slerp(FromTurn, ToTurn, Along);
		FHandAt Out;
		Out.At = FMath::Lerp(From.At, To.At, Along);
		Out.Fingers = Turn.GetAxisX();
		Out.Palm = Turn.GetAxisZ();
		return Out;
	}

	float ThrowWeight(float Phase)
	{
		return Phase < 0.0f ? 0.0f : FMath::SmoothStep(0.0f, 0.1f, Phase);
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
