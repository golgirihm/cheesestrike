// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeDowned.h"

namespace CheeseStrikeDowned
{
	float Breath(float Phase)
	{
		// A third of the breath drawing in, two thirds letting it out.
		const float Cycle = FMath::Fmod(Phase / UE_TWO_PI, 1.0f);
		const float At = Cycle < 0.0f ? Cycle + 1.0f : Cycle;
		const float Shaped = At < 0.35f ? 0.5f * At / 0.35f : 0.5f + 0.5f * (At - 0.35f) / 0.65f;
		return -FMath::Cos(Shaped * UE_TWO_PI);
	}

	FRotator ClampView(const FRotator& View, float BodyYaw)
	{
		const float Off = FMath::Clamp(FRotator::NormalizeAxis(View.Yaw - BodyYaw), -ConeYaw, ConeYaw);
		return FRotator(FMath::Clamp(FRotator::NormalizeAxis(View.Pitch), LowestPitch, HighestPitch), FRotator::NormalizeAxis(BodyYaw + Off), 0.0f);
	}

	bool InCone(const FRotator& View, float BodyYaw, float Give)
	{
		const float Pitch = FRotator::NormalizeAxis(View.Pitch);
		return FMath::Abs(FRotator::NormalizeAxis(View.Yaw - BodyYaw)) <= ConeYaw + Give && Pitch >= LowestPitch - Give && Pitch <= HighestPitch + Give;
	}

	float TurnBody(float BodyYaw, float ViewYaw, float DeltaSeconds)
	{
		const float Off = FRotator::NormalizeAxis(ViewYaw - BodyYaw);
		const float Beyond = FMath::Abs(Off) - TurnFree;
		if (Beyond <= 0.0f || DeltaSeconds <= 0.0f)
		{
			return FRotator::NormalizeAxis(BodyYaw);
		}
		return FRotator::NormalizeAxis(BodyYaw + FMath::Sign(Off) * FMath::Min(Beyond, TurnRate * DeltaSeconds));
	}

	float TurnRising(float BodyYaw, float ViewYaw, float DeltaSeconds)
	{
		const float Off = FRotator::NormalizeAxis(ViewYaw - BodyYaw);
		return FRotator::NormalizeAxis(BodyYaw + FMath::Sign(Off) * FMath::Min(FMath::Abs(Off), RiseTurnRate * FMath::Max(DeltaSeconds, 0.0f)));
	}

	FVector ScootInput(const FVector& Input, const FVector& Forward, float LegRoom)
	{
		const FVector Along = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
		if (Along.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		float Push = FVector::DotProduct(Input, Along);
		// Toward the feet only while the legs have room.
		if (Push > 0.0f && LegRoom < LegRoomToScoot)
		{
			Push = 0.0f;
		}
		return Along * Push;
	}

	FVector HeadAt()
	{
		return FVector(-2.0f, 0.0f, 44.0f);
	}

	FVector ShouldersAt()
	{
		return FVector(-10.0f, 0.0f, 33.0f);
	}

	float LegReach(float Room)
	{
		return FMath::Clamp(Room - ToeGap, LegReachLeast, LegLength);
	}

	FShape Shape(const FVector& Floor, float Yaw, float LegRoom)
	{
		const FRotator Facing(0.0f, Yaw, 0.0f);
		const auto Place = [&Floor, &Facing](const FVector& InBody) { return Floor + Facing.RotateVector(InBody); };
		FShape Body;
		Body.Head = Place(HeadAt());
		Body.Shoulders = Place(ShouldersAt());
		Body.Hips = Place(FVector(HipsAhead, 0.0f, PelvisHeight));
		Body.Feet = Place(FVector(HipsAhead + LegReach(LegRoom), 0.0f, LegRadius));
		return Body;
	}

	FVector Middle(const FShape& Body)
	{
		return FMath::Lerp(Body.Shoulders, Body.Hips, 0.35f);
	}
}
