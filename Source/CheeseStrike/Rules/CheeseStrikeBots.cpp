// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBots.h"
#include "Kismet/GameplayStatics.h"

namespace CheeseStrikeBots
{
	// The design's table (docs/unreal/design.md, "Bots").
	const FSkill Skills[NumSkills] = {
		{TEXT("Easy"), 0.75f, 7.0f, 0.55f, 3, 0.9f, 3.0f},
		{TEXT("Normal"), 0.45f, 4.0f, 0.4f, 5, 0.55f, 5.0f},
		{TEXT("Hard"), 0.25f, 2.0f, 0.3f, 8, 0.3f, 8.0f},
	};

	const FSkill& Skill(int32 Index)
	{
		return Skills[FMath::Clamp(Index, 0, NumSkills - 1)];
	}

	int32 SkillFrom(const FString& Options, int32 Otherwise)
	{
		const FString Value = UGameplayStatics::ParseOption(Options, TEXT("BotSkill"));
		if (!Value.IsNumeric())
		{
			return Otherwise;
		}
		return FMath::Clamp(FCString::Atoi(*Value), 0, NumSkills - 1);
	}

	float AimError(const FSkill& Skill, float Tracking)
	{
		const float Through = FMath::Clamp(Tracking / SteadyTime, 0.0f, 1.0f);
		return Skill.AimError * FMath::Lerp(1.0f, Skill.Steadies, Through);
	}

	FRotator TurnToward(const FRotator& Current, const FRotator& Wanted, float RadiansPerSecond, float DeltaSeconds)
	{
		const float Step = FMath::RadiansToDegrees(RadiansPerSecond) * DeltaSeconds;
		const float Yaw = FRotator::NormalizeAxis(Wanted.Yaw - Current.Yaw);
		const float Pitch = FRotator::NormalizeAxis(Wanted.Pitch - Current.Pitch);
		// Both axes arrive together, as a turn along the straight way between the two.
		const float Size = FMath::Sqrt(Yaw * Yaw + Pitch * Pitch);
		const float Share = Size <= Step || Size <= KINDA_SMALL_NUMBER ? 1.0f : Step / Size;
		return FRotator(FRotator::NormalizeAxis(Current.Pitch + Pitch * Share), FRotator::NormalizeAxis(Current.Yaw + Yaw * Share), 0.0f);
	}

	bool InSight(const FVector& Facing, const FVector& ToTarget)
	{
		const float Distance = ToTarget.Size();
		if (Distance <= NoticeAnywhere)
		{
			return true;
		}
		if (Distance > SightRange)
		{
			return false;
		}
		// The cone is level: a bot sees up and down as far as it sees to the sides.
		const FVector Flat = FVector(Facing.X, Facing.Y, 0.0f).GetSafeNormal();
		const float Cosine = FVector::DotProduct(Flat, FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal());
		return Flat.IsNearlyZero() || Cosine >= FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));
	}

	EMove MoveFor(float Distance, float WeaponRange, float Health, bool bWedges)
	{
		if (Health < LowHealth && bWedges)
		{
			return EMove::Retreat;
		}
		if (Distance < BackOff)
		{
			return EMove::BackOff;
		}
		// Some give either side of the range, so that a bot doesn't flick between closing in and holding.
		return Distance > WeaponRange * FightRangeShare * 1.25f ? EMove::Approach : EMove::Hold;
	}

	bool WouldFireAt(float Distance, float WeaponRange, bool bTheyShootMe)
	{
		return Distance <= WeaponRange && (bTheyShootMe || Distance <= WeaponRange * LongShotShare);
	}

	bool HoldsTrigger(FBursts& Bursts, const FSkill& Skill, bool bWantsToFire, float FireInterval, float Now)
	{
		if (Bursts.bFiring)
		{
			if (!bWantsToFire)
			{
				// Off target: let go, and rest as after a burst.
				Bursts = {false, Now + Skill.Rest};
				return false;
			}
			if (Now >= Bursts.Until)
			{
				Bursts = {false, Now + Skill.Rest};
				return false;
			}
			return true;
		}
		if (bWantsToFire && Now >= Bursts.Until)
		{
			// A burst lasts its shots at the weapon's rate; the first goes at once.
			Bursts = {true, Now + FireInterval * (Skill.Burst - 0.5f)};
			return true;
		}
		return false;
	}
}
