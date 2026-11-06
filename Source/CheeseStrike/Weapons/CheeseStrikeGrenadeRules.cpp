// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenadeRules.h"

namespace CheeseStrikeGrenades
{
	const TCHAR* const Name = TEXT("Cheese Grenade");
	const TCHAR* const OwnWords = TEXT("blew themself up");

	const TCHAR* NameOf(EGrenadeKind Kind)
	{
		return Kind == EGrenadeKind::Smoke ? TEXT("Smoked Gouda") : Name;
	}

	const TCHAR* KeyOf(EGrenadeKind Kind)
	{
		return Kind == EGrenadeKind::Smoke ? TEXT("smoke") : TEXT("cheese");
	}

	TOptional<EGrenadeKind> KindFromKey(const FString& Key)
	{
		for (int32 Index = 0; Index < NumKinds; ++Index)
		{
			const EGrenadeKind Kind = static_cast<EGrenadeKind>(Index);
			if (Key.Equals(KeyOf(Kind), ESearchCase::IgnoreCase) || Key.Equals(NameOf(Kind), ESearchCase::IgnoreCase))
			{
				return Kind;
			}
		}
		return TOptional<EGrenadeKind>();
	}

	EGrenadeKind KindAt(int32 Index)
	{
		return Index == static_cast<int32>(EGrenadeKind::Smoke) ? EGrenadeKind::Smoke : EGrenadeKind::Cheese;
	}

	float FuseOf(EGrenadeKind Kind)
	{
		return Kind == EGrenadeKind::Smoke ? SmokeFuse : Fuse;
	}

	bool Harms(EGrenadeKind Kind)
	{
		return Kind == EGrenadeKind::Cheese;
	}

	float CloudRadiusAt(float Age)
	{
		if (Age < 0.0f || Age >= CloudLife)
		{
			return 0.0f;
		}
		// Quick at first, settling to its size: eased out.
		const float Grown = FMath::Clamp(Age / CloudGrowSeconds, 0.0f, 1.0f);
		return CloudAcross * 0.5f * (1.0f - FMath::Square(1.0f - Grown));
	}

	float CloudOpacityAt(float Age)
	{
		if (Age < 0.0f || Age >= CloudLife)
		{
			return 0.0f;
		}
		return FMath::Clamp((CloudLife - Age) / CloudThinSeconds, 0.0f, 1.0f);
	}

	bool CloudBlocksSightAt(float Age)
	{
		return Age >= 0.0f && CloudOpacityAt(Age) >= 0.5f;
	}

	/** How finely the veil's size is found round the core: rings from bottom to top, and points round each. */
	constexpr int32 VeilRings = 10;
	constexpr int32 VeilTurns = 18;

	bool CloudCoreHides(const FVector& Feet, float Height, float Age)
	{
		if (!CloudBlocksSightAt(Age))
		{
			return false;
		}
		// The ellipsoid about the middle.
		const float Across = CloudRadiusAt(Age) * CloudCoreShare;
		const float Up = Across * CloudCoreFlat;
		if (Across <= 0.0f)
		{
			return false;
		}
		const auto Inside = [Across, Up](const FVector& Point)
		{
			const FVector FromMiddle = Point - FVector(0.0f, 0.0f, CloudMiddleUp);
			return FMath::Square(FromMiddle.X / Across) + FMath::Square(FromMiddle.Y / Across) + FMath::Square(FromMiddle.Z / Up) <= 1.0f;
		};
		// A body is an upright line: inside if its feet and its head both are (the ellipsoid is convex).
		return Inside(Feet) && Inside(Feet + FVector(0.0f, 0.0f, Height));
	}

	FVeil CloudVeil(const FVector& Eye, float Age)
	{
		FVeil Veil;
		const float Across = CloudRadiusAt(Age) * CloudCoreShare;
		if (!CloudBlocksSightAt(Age) || Across <= 0.0f)
		{
			return Veil;
		}
		const float Up = Across * CloudCoreFlat;
		// How far the core reaches along a way (its support): the ellipsoid's, its middle at CloudMiddleUp.
		const auto Reach = [Across, Up](const FVector& Way)
		{
			return FMath::Sqrt(FMath::Square(Across) * (FMath::Square(Way.X) + FMath::Square(Way.Y)) + FMath::Square(Up) * FMath::Square(Way.Z));
		};
		const FVector FromMiddle = Eye - FVector(0.0f, 0.0f, CloudMiddleUp);
		const float Distance = FromMiddle.Size();
		const FVector Toward = Distance > UE_KINDA_SMALL_NUMBER ? FromMiddle / Distance : FVector::BackwardVector;
		const FVector Looking = -Toward;
		Veil.Right = FVector::CrossProduct(FVector::UpVector, Looking).GetSafeNormal();
		if (Veil.Right.IsNearlyZero())
		{
			Veil.Right = FVector::RightVector;
		}
		Veil.Up = FVector::CrossProduct(Looking, Veil.Right).GetSafeNormal();
		// The veil stands CloudVeilAhead before the core along the way to the eye.
		const float Before = Reach(Toward) + CloudVeilAhead;
		const float Ahead = Distance - Before;
		if (Ahead < CloudVeilNearest)
		{
			// The eye in the core or at its edge: a veil just before it, over anything it could look at.
			Veil.bAtTheEye = true;
			Veil.Middle = FromMiddle + Looking * CloudVeilNearest + FVector(0.0f, 0.0f, CloudMiddleUp);
			Veil.HalfRight = CloudVeilNearest * 4.0f;
			Veil.HalfUp = CloudVeilNearest * 4.0f;
			return Veil;
		}
		Veil.Middle = Toward * Before + FVector(0.0f, 0.0f, CloudMiddleUp);
		// The core's outline as the eye sees it, where the veil stands: its shape from the cone that touches it each way,
		// then made as big as every point round the core needs (seen from near, an ellipsoid's outline is no simple
		// ellipse about the middle), and a little over.
		const auto Half = [Distance, Ahead](float Extent)
		{
			const float Tangent = Extent / FMath::Sqrt(FMath::Max(FMath::Square(Distance) - FMath::Square(Extent), FMath::Square(Distance * 0.2f)));
			return Ahead * Tangent;
		};
		Veil.HalfRight = FMath::Max(1.0f, Half(Reach(Veil.Right)));
		Veil.HalfUp = FMath::Max(1.0f, Half(Reach(Veil.Up)));
		float Widest = 0.0f;
		for (int32 Ring = 0; Ring <= VeilRings; ++Ring)
		{
			const float Pitch = PI * Ring / VeilRings - HALF_PI;
			for (int32 Turn = 0; Turn < VeilTurns; ++Turn)
			{
				const float Yaw = 2.0f * PI * Turn / VeilTurns;
				const FVector Point(Across * FMath::Cos(Pitch) * FMath::Cos(Yaw), Across * FMath::Cos(Pitch) * FMath::Sin(Yaw), Up * FMath::Sin(Pitch));
				// Where the line from the eye to it crosses the veil's plane, in the veil's axes.
				const FVector Line = Point - FromMiddle;
				const float Deep = FVector::DotProduct(Line, Looking);
				if (Deep <= UE_KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector Crossing = FromMiddle + Line * (Ahead / Deep) - (Veil.Middle - FVector(0.0f, 0.0f, CloudMiddleUp));
				Widest = FMath::Max(Widest, FMath::Square(FVector::DotProduct(Crossing, Veil.Right) / Veil.HalfRight) + FMath::Square(FVector::DotProduct(Crossing, Veil.Up) / Veil.HalfUp));
			}
		}
		const float Grow = FMath::Sqrt(Widest) * 1.06f;
		Veil.HalfRight *= Grow;
		Veil.HalfUp *= Grow;
		return Veil;
	}

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
		// tan(a) = (vÂ² - sqrt(vâ´ - g(g xÂ² + 2 y vÂ²))) / (g x): the lower arc.
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
