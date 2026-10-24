// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeShot.h"
#include "Rules/CheeseStrikeDowned.h"

namespace CheeseStrikeWeapons
{
	FString ReserveLabel(int32 Reserve)
	{
		return Reserve < 0 ? FString(TEXT("/ ∞")) : FString::Printf(TEXT("/ %d"), Reserve);
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

	float ShotTime(const FWeaponStats& Weapon, int32 Index)
	{
		const int32 Burst = FMath::Max(Weapon.Burst, 1);
		return (Index / Burst) * Weapon.FireInterval + (Index % Burst) * Weapon.BurstInterval;
	}

	float Cooldown(const FWeaponStats& Weapon, int32 InBurst)
	{
		const int32 Burst = FMath::Max(Weapon.Burst, 1);
		if (Burst > 1 && InBurst < Burst - 1)
		{
			return Weapon.BurstInterval;
		}
		// From the last shot of a burst to the first of the next: what is left of the interval.
		return FMath::Max(Weapon.FireInterval - (Burst - 1) * Weapon.BurstInterval, Weapon.BurstInterval);
	}

	float ShotsPerSecond(const FWeaponStats& Weapon)
	{
		return FMath::Max(Weapon.Burst, 1) / FMath::Max(Weapon.FireInterval, 0.01f);
	}

	int32 ShotsToKill(const FWeaponStats& Weapon)
	{
		const float PerShot = Weapon.Damage * FMath::Max(Weapon.Pellets, 1);
		return PerShot > 0.0f ? FMath::CeilToInt(MaxHealth / PerShot) : 0;
	}

	float TimeToKill(const FWeaponStats& Weapon)
	{
		return ShotTime(Weapon, FMath::Max(ShotsToKill(Weapon) - 1, 0));
	}

	bool IsDrawn(const FWeaponStats& Weapon, float DrawnAt, float Now, float Allowance)
	{
		return Now >= DrawnAt + Weapon.DrawTime - Allowance;
	}

	TArray<FVector> SweepDirections(const FWeaponStats& Knife, const FVector& Direction)
	{
		TArray<FVector> Fan = {Direction};
		// Out from the aim to each side, nearer first, the last at the sweep's edge.
		const int32 Steps = FMath::CeilToInt(Knife.SweepDegrees / SweepStep - KINDA_SMALL_NUMBER);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Degrees = FMath::Min(Step * SweepStep, Knife.SweepDegrees);
			for (const float Side : {-1.0f, 1.0f})
			{
				Fan.Add(Direction.RotateAngleAxis(Degrees * Side, FVector::UpVector));
			}
		}
		return Fan;
	}

	bool FromBehind(const FVector& Direction, float Yaw)
	{
		const FVector Along(Direction.X, Direction.Y, 0.0f);
		const FVector Facing = FRotator(0.0f, Yaw, 0.0f).Vector();
		// Going the way they face is going into their back.
		return Along.SizeSquared() > KINDA_SMALL_NUMBER && FVector::DotProduct(Along.GetSafeNormal(), Facing) > 0.5f;
	}

	float StrokeDamage(const FWeaponStats& Knife, const FVector& Direction, float Yaw)
	{
		return FromBehind(Direction, Yaw) ? Knife.BackstabDamage : Knife.Damage;
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
		// Two guns kick to their own sides in turn, the right gun's to the right; one wanders along a pattern.
		const float Wander = Weapon.bDual
			? Weapon.RecoilSide * (DualHand(Weapon, Index) == 0 ? 1.0f : -1.0f) * FMath::Min(Index / 3.0f, 1.0f)
			: Weapon.RecoilSide * FMath::Sin(Index * 1.3f) * FMath::Min(Index / 3.0f, 1.0f);
		return FVector2D(Wander, Climb) * Steady;
	}

	int32 DualHand(const FWeaponStats& Weapon, int32 Index)
	{
		return Weapon.bDual && Index % 2 != 0 ? 1 : 0;
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
		// The cone's half-angle: the most a shot goes off the aim.
		return Random.VRandCone(Direction, FMath::DegreesToRadians(ConeDegrees / 2.0f));
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

	float RayHitsSegment(const FVector& Origin, const FVector& Direction, const FVector& From, const FVector& To, float Radius)
	{
		float Best = -1.0f;
		const auto Consider = [&Best](float Candidate)
		{
			if (Candidate >= 0.0f && (Best < 0.0f || Candidate < Best))
			{
				Best = Candidate;
			}
		};
		// The side: the ray against the cylinder about the segment, kept to the segment's length; then the round ends.
		const FVector Axis = To - From;
		const FVector Start = Origin - From;
		const double AxisAxis = FVector::DotProduct(Axis, Axis);
		const double AxisRay = FVector::DotProduct(Axis, Direction);
		const double AxisStart = FVector::DotProduct(Axis, Start);
		const double RayStart = FVector::DotProduct(Direction, Start);
		const double StartStart = FVector::DotProduct(Start, Start);
		const double A = AxisAxis - AxisRay * AxisRay;
		const double B = AxisAxis * RayStart - AxisStart * AxisRay;
		const double C = AxisAxis * StartStart - AxisStart * AxisStart - Radius * Radius * AxisAxis;
		const double Discriminant = B * B - A * C;
		if (A > UE_KINDA_SMALL_NUMBER && Discriminant >= 0.0)
		{
			const double Near = (-B - FMath::Sqrt(Discriminant)) / A;
			const double Along = AxisStart + Near * AxisRay;
			if (Near >= 0.0 && Along > 0.0 && Along < AxisAxis)
			{
				Consider(static_cast<float>(Near));
			}
		}
		Consider(RayHitsSphere(Origin, Direction, From, Radius));
		Consider(RayHitsSphere(Origin, Direction, To, Radius));
		return Best;
	}
}

float FHitHistory::RayHitsBody(const FVector& Origin, const FVector& Direction, const FSample& Sample)
{
	using namespace CheeseStrikeWeapons;
	if (!Sample.bLying)
	{
		return RayHitsCapsule(Origin, Direction, Sample.Feet, Sample.Radius, Sample.Height);
	}
	const float Torso = RayHitsSegment(Origin, Direction, Sample.Hips, Sample.Shoulders, CheeseStrikeDowned::TorsoRadius);
	const float Legs = RayHitsSegment(Origin, Direction, Sample.Hips, Sample.LegsTo, CheeseStrikeDowned::LegRadius);
	return Torso < 0.0f ? Legs : (Legs < 0.0f ? Torso : FMath::Min(Torso, Legs));
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
			Between.Yaw = Before.Yaw + FMath::FindDeltaAngleDegrees(Before.Yaw, After.Yaw) * Through;
			Between.Radius = FMath::Lerp(Before.Radius, After.Radius, Through);
			Between.Height = FMath::Lerp(Before.Height, After.Height, Through);
			// Lying as the nearer sample was; where both lie, the body between them.
			Between.bLying = Through < 0.5f ? Before.bLying : After.bLying;
			const FSample& Lain = Before.bLying ? Before : After;
			const bool bBoth = Before.bLying && After.bLying;
			Between.Hips = bBoth ? FMath::Lerp(Before.Hips, After.Hips, Through) : Lain.Hips;
			Between.Shoulders = bBoth ? FMath::Lerp(Before.Shoulders, After.Shoulders, Through) : Lain.Shoulders;
			Between.LegsTo = bBoth ? FMath::Lerp(Before.LegsTo, After.LegsTo, Through) : Lain.LegsTo;
			return Between;
		}
	}
	return Samples.Last();
}
