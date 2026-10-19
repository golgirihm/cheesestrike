// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeShot.h"

namespace CheeseStrikeWeapons
{
	const FWeaponStats& Blaster()
	{
		static const FWeaponStats Stats = []
		{
			FWeaponStats Made;
			Made.Id = FName("blaster");
			Made.DisplayName = NSLOCTEXT("CheeseStrike", "Blaster", "Cheddar Blaster");
			return Made;
		}();
		return Stats;
	}

	float DamageAt(const FWeaponStats& Weapon, float Distance, bool bHead)
	{
		float Share = 1.0f;
		if (Weapon.FalloffEnd > Weapon.FalloffStart)
		{
			const float Through = FMath::Clamp((Distance - Weapon.FalloffStart) / (Weapon.FalloffEnd - Weapon.FalloffStart), 0.0f, 1.0f);
			Share = FMath::Lerp(1.0f, Weapon.FalloffFloor, Through);
		}
		return Weapon.Damage * Share * (bHead ? Weapon.HeadMultiplier : 1.0f);
	}

	float TimeToKill(const FWeaponStats& Weapon)
	{
		const int32 Shots = FMath::CeilToInt(MaxHealth / Weapon.Damage);
		return (Shots - 1) * Weapon.FireInterval;
	}

	float SpreadFor(const FWeaponStats& Weapon, int32 Index, bool bAimed)
	{
		const float Full = bAimed ? Weapon.SpreadDegrees : Weapon.HipSpreadDegrees;
		if (Weapon.BloomShots <= 1)
		{
			return Full;
		}
		return Full * FMath::Clamp(float(Index) / float(Weapon.BloomShots), 0.0f, 1.0f);
	}

	FVector2D RecoilFor(const FWeaponStats& Weapon, int32 Index, bool bAimed)
	{
		if (Index <= 0)
		{
			return FVector2D::ZeroVector;
		}
		const float Steady = bAimed ? AimRecoilFactor : 1.0f;
		// The climb slows as it goes, and never passes RecoilCap shots' worth.
		const float Cap = FMath::Max(Weapon.RecoilCap, 1.0f);
		const float Climb = Weapon.RecoilUp * Cap * (1.0f - FMath::Exp(-Index / Cap));
		const float Wander = Weapon.RecoilSide * FMath::Sin(Index * 1.3f) * FMath::Min(Index / 3.0f, 1.0f);
		return FVector2D(Wander, Climb) * Steady;
	}

	int32 NextInRun(const FWeaponStats& Weapon, int32 Last, float SinceLast)
	{
		if (Last < 0 || SinceLast > Weapon.RecoilReset)
		{
			return 0;
		}
		return Last + 1;
	}

	FVector Turned(const FVector& Direction, const FVector2D& Degrees, const FVector& Up)
	{
		// Right is up across the direction; a turn to the right is about up, and a turn upward is about right, the other way.
		const FVector Right = FVector::CrossProduct(Up, Direction).GetSafeNormal();
		const FVector TrueUp = FVector::CrossProduct(Direction, Right).GetSafeNormal();
		FVector Result = Direction.RotateAngleAxis(Degrees.X, TrueUp);
		Result = Result.RotateAngleAxis(-Degrees.Y, Right);
		return Result.GetSafeNormal();
	}

	FVector Spread(const FVector& Direction, float ConeDegrees, FRandomStream& Random)
	{
		if (ConeDegrees <= 0.0f)
		{
			return Direction;
		}
		return Random.VRandCone(Direction, FMath::DegreesToRadians(ConeDegrees));
	}

	float RayHitsSphere(const FVector& Origin, const FVector& Direction, const FVector& Centre, float Radius)
	{
		const FVector ToCentre = Centre - Origin;
		const float Along = FVector::DotProduct(ToCentre, Direction);
		const float Across2 = ToCentre.SizeSquared() - Along * Along;
		const float Radius2 = Radius * Radius;
		if (Across2 > Radius2)
		{
			return -1.0f;
		}
		const float Half = FMath::Sqrt(Radius2 - Across2);
		const float Near = Along - Half;
		// A ray that starts inside doesn't hit it.
		return Near >= 0.0f ? Near : -1.0f;
	}

	float RayHitsCapsule(const FVector& Origin, const FVector& Direction, const FVector& Feet, float Radius, float Height)
	{
		// The capsule's straight part runs from a radius above the feet to a radius below the top.
		const FVector Bottom = Feet + FVector(0, 0, Radius);
		const FVector Top = Feet + FVector(0, 0, FMath::Max(Height - Radius, Radius));
		float Best = -1.0f;
		const auto Consider = [&Best](float Candidate)
		{
			if (Candidate >= 0.0f && (Best < 0.0f || Candidate < Best))
			{
				Best = Candidate;
			}
		};
		// The side: an infinite cylinder about the axis, kept to the straight part.
		const FVector2D O(Origin.X - Feet.X, Origin.Y - Feet.Y);
		const FVector2D D(Direction.X, Direction.Y);
		const float A = D.SizeSquared();
		if (A > KINDA_SMALL_NUMBER)
		{
			const float B = 2.0f * FVector2D::DotProduct(O, D);
			const float C = O.SizeSquared() - Radius * Radius;
			const float Discriminant = B * B - 4.0f * A * C;
			if (Discriminant >= 0.0f)
			{
				const float Near = (-B - FMath::Sqrt(Discriminant)) / (2.0f * A);
				const float Z = Origin.Z + Direction.Z * Near;
				if (Near >= 0.0f && Z >= Bottom.Z && Z <= Top.Z)
				{
					Consider(Near);
				}
			}
		}
		Consider(RayHitsSphere(Origin, Direction, Bottom, Radius));
		Consider(RayHitsSphere(Origin, Direction, Top, Radius));
		return Best;
	}
}

void FHitHistory::Add(const FSample& Sample)
{
	Samples.Add(Sample);
	while (Samples.Num() > 1 && Samples[0].Time < Sample.Time - Kept)
	{
		Samples.RemoveAt(0);
	}
}

FHitHistory::FSample FHitHistory::At(float Time) const
{
	if (Samples.Num() == 0)
	{
		return FSample();
	}
	if (Time >= Samples.Last().Time)
	{
		return Samples.Last();
	}
	if (Time <= Samples[0].Time)
	{
		return Samples[0];
	}
	for (int32 Index = 1; Index < Samples.Num(); ++Index)
	{
		const FSample& After = Samples[Index];
		if (Time <= After.Time)
		{
			const FSample& Before = Samples[Index - 1];
			const float Span = After.Time - Before.Time;
			const float Through = Span > KINDA_SMALL_NUMBER ? (Time - Before.Time) / Span : 1.0f;
			FSample Between = After;
			Between.Time = Time;
			Between.Feet = FMath::Lerp(Before.Feet, After.Feet, Through);
			Between.Head = FMath::Lerp(Before.Head, After.Head, Through);
			Between.Radius = FMath::Lerp(Before.Radius, After.Radius, Through);
			Between.Height = FMath::Lerp(Before.Height, After.Height, Through);
			return Between;
		}
	}
	return Samples.Last();
}
