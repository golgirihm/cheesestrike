// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeExecution.h"
#include "CheeseStrikeDowned.h"

namespace CheeseStrikeExecution
{
	EKind Pick(int32 Roll)
	{
		return (Roll & 1) == 0 ? EKind::Stab : EKind::Stomp;
	}

	EKind KindAt(int32 Index)
	{
		return Index >= 1 && Index <= NumKinds ? static_cast<EKind>(Index) : EKind::None;
	}

	const TCHAR* Name(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Stab:
			return TEXT("stab");
		case EKind::Stomp:
			return TEXT("stomp");
		default:
			return TEXT("none");
		}
	}

	float Seconds(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Stab:
			return StabSeconds;
		case EKind::Stomp:
			return StompSeconds;
		default:
			return 0.0f;
		}
	}

	float KillAt(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Stab:
			return StabLands[1] + 0.4f;
		case EKind::Stomp:
			return StompLands;
		default:
			return 0.0f;
		}
	}

	bool IsPlaying(EKind Kind, float Elapsed)
	{
		return Kind != EKind::None && Elapsed >= -0.5f && Elapsed < Seconds(Kind);
	}

	FVector AimPoint(EKind Kind)
	{
		if (Kind == EKind::Stomp)
		{
			return CheeseStrikeDowned::HeadAt();
		}
		// The chest: between the shoulders and the hips, nearer the shoulders.
		const FVector Hips(CheeseStrikeDowned::HipsAhead, 0.0f, CheeseStrikeDowned::PelvisHeight);
		return FMath::Lerp(CheeseStrikeDowned::ShouldersAt(), Hips, 0.3f);
	}

	FVector SpotInBody(EKind Kind, float Side)
	{
		const FVector Aim = AimPoint(Kind);
		const float Way = Side >= 0.0f ? 1.0f : -1.0f;
		return FVector(Aim.X, Way * (Kind == EKind::Stomp ? StompStandOff : StabStandOff), 0.0f);
	}

	float FacingYaw(const FVector& Spot, const FVector& Aim)
	{
		const FVector To = FVector(Aim.X - Spot.X, Aim.Y - Spot.Y, 0.0f);
		return To.IsNearlyZero() ? 0.0f : To.Rotation().Yaw;
	}

	FVector LookAtInBody(EKind Kind, float Side)
	{
		// Between the blow and the executor, a little up.
		const FVector Aim = AimPoint(Kind);
		const FVector Spot = SpotInBody(Kind, Side);
		return FVector(FMath::Lerp(Aim.X, Spot.X, 0.4f), FMath::Lerp(Aim.Y, Spot.Y, 0.4f), Kind == EKind::Stomp ? 70.0f : 55.0f);
	}

	FVector CameraInBody(EKind Kind, float Side, float Elapsed)
	{
		const float Way = Side >= 0.0f ? 1.0f : -1.0f;
		const FVector Look = LookAtInBody(Kind, Side);
		// From the far side of the one down, toward their feet and up, drifting round toward the feet as it plays.
		const float Drift = FMath::DegreesToRadians(CameraDrift * FMath::Clamp(Elapsed / FMath::Max(Seconds(Kind), 0.1f), 0.0f, 1.0f));
		const FVector2D Flat(CameraAlong, -Way * CameraBack);
		const float Angle = Way * Drift;
		const FVector2D Turned(Flat.X * FMath::Cos(Angle) - Flat.Y * FMath::Sin(Angle), Flat.X * FMath::Sin(Angle) + Flat.Y * FMath::Cos(Angle));
		return FVector(Look.X + Turned.X, Look.Y + Turned.Y, CameraUp);
	}

	float Ease(float Time, float From, float To)
	{
		if (To <= From)
		{
			return Time >= To ? 1.0f : 0.0f;
		}
		return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((Time - From) / (To - From), 0.0f, 1.0f));
	}

	float Envelope(float Time, float In, float Full, float Out, float Gone)
	{
		return Ease(Time, In, Full) * (1.0f - Ease(Time, Out, Gone));
	}
}
