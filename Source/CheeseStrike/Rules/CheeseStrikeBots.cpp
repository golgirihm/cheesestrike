// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBots.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/CheeseStrikeArsenal.h"
#include "Rules/CheeseStrikeDowned.h"

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
		// A weapon made for close quarters isn't kept at arm's length.
		if (Distance < FMath::Min(BackOff, WeaponRange * FightRangeShare * 0.5f))
		{
			return EMove::BackOff;
		}
		// Some give either side of the range, so that a bot doesn't flick between closing in and holding.
		return Distance > WeaponRange * FightRangeShare * 1.25f ? EMove::Approach : EMove::Hold;
	}

	TArray<TPair<FName, float>> PrimaryWeights(int32 SkillIndex)
	{
		using namespace CheeseStrikeWeapons;
		// Blaster, Carbine, Grater, Munster, Shotgun, Roquefort: an easy bot sprays, a hard one aims.
		static const float Weights[NumSkills][6] = {
			{3.0f, 1.0f, 3.0f, 2.0f, 2.0f, 0.0f},
			{3.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1.0f},
			{2.0f, 3.0f, 2.0f, 1.0f, 2.0f, 2.0f},
		};
		const FWeaponStats* Order[] = {&Blaster(), &Carbine(), &Grater(), &Munster(), &Shotgun(), &Roquefort()};
		const int32 Row = FMath::Clamp(SkillIndex, 0, NumSkills - 1);
		TArray<TPair<FName, float>> Out;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Out.Add({Order[Index]->Id, Weights[Row][Index]});
		}
		return Out;
	}

	FName PrimaryFor(int32 SkillIndex, FRandomStream& Random)
	{
		const TArray<TPair<FName, float>> Weights = PrimaryWeights(SkillIndex);
		float Total = 0.0f;
		for (const TPair<FName, float>& Each : Weights)
		{
			Total += Each.Value;
		}
		float Pick = Random.FRandRange(0.0f, Total);
		for (const TPair<FName, float>& Each : Weights)
		{
			if (Each.Value > 0.0f && Pick < Each.Value)
			{
				return Each.Key;
			}
			Pick -= Each.Value;
		}
		return CheeseStrikeArsenal::DefaultPrimary();
	}

	EWeaponSlot ChooseWeapon(const FArmed& Armed, bool bFighting, float Distance, EWeaponSlot Current)
	{
		const bool bPrimaryUsable = Armed.bPrimary && (Armed.PrimaryMagazine > 0 || Armed.bPrimaryReserve);
		if (bFighting && Armed.bKnife && (Distance < KnifeFrom || (Current == EWeaponSlot::Knife && Distance < KnifeLetGo)))
		{
			return EWeaponSlot::Knife;
		}
		if (bFighting && Armed.bSidearm && Armed.PrimaryMagazine <= 0 && (Current == EWeaponSlot::Sidearm || Distance < SidearmWithin || !bPrimaryUsable))
		{
			return EWeaponSlot::Sidearm;
		}
		if (bPrimaryUsable)
		{
			return EWeaponSlot::Primary;
		}
		if (Armed.bSidearm)
		{
			return EWeaponSlot::Sidearm;
		}
		if (Armed.bKnife)
		{
			return EWeaponSlot::Knife;
		}
		return Current;
	}

	bool WouldStrikeAt(float Distance, float Reach)
	{
		return Distance <= Reach * 0.95f;
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
	float BearingFromDowned(const FVector& At, const FVector& Downed, float Yaw)
	{
		const FVector Offset = FVector(At.X - Downed.X, At.Y - Downed.Y, 0.0f);
		if (Offset.SizeSquared() < 1.0f)
		{
			return 0.0f;
		}
		return FRotator::NormalizeAxis(Offset.Rotation().Yaw - Yaw);
	}

	bool InDownedAim(const FVector& At, const FVector& Downed, float Yaw, float Margin)
	{
		return FMath::Abs(BearingFromDowned(At, Downed, Yaw)) <= CheeseStrikeDowned::ConeYaw + Margin;
	}

	FVector FinishApproach(const FVector& At, const FVector& Downed, float Yaw, float Now, float Way)
	{
		const float Bearing = BearingFromDowned(At, Downed, Yaw);
		const float Safe = CheeseStrikeDowned::ConeYaw + FlankMargin;
		// Out of their aim by half the margin is out (so that, come round to the edge, it goes in rather than circling on).
		if (FMath::Abs(Bearing) >= CheeseStrikeDowned::ConeYaw + FlankMargin * 0.5f)
		{
			// Out of their aim already: straight in, along the same bearing, to stand beside them.
			return Downed + FRotator(0.0f, Yaw + Bearing, 0.0f).Vector() * FlankStandOff;
		}
		// In their aim: round to the nearer edge and past it, swerving on the way.
		const float Side = (Bearing >= 0.0f ? 1.0f : -1.0f) * (Way < 0.0f ? -1.0f : 1.0f);
		const float Distance = FVector::Dist2D(At, Downed);
		const float Radius = FMath::Clamp(Distance, FlankStandOff, FlankRadius);
		const FVector Edge = Downed + FRotator(0.0f, Yaw + Side * Safe, 0.0f).Vector() * Radius;
		const FVector ToEdge = FVector(Edge.X - At.X, Edge.Y - At.Y, 0.0f);
		const float Left = ToEdge.Size();
		if (Left < 1.0f)
		{
			return Edge;
		}
		const FVector Along = ToEdge / Left;
		// The swerve is across their line of fire, not across the way it goes.
		const FVector ToThem = FVector(Downed.X - At.X, Downed.Y - At.Y, 0.0f).GetSafeNormal();
		const FVector Across(-ToThem.Y, ToThem.X, 0.0f);
		const float Swerve = FMath::Sin(2.0f * PI * Now / ZigzagPeriod) >= 0.0f ? 1.0f : -1.0f;
		const float Ahead = FMath::Min(Left, ZigzagStep);
		// The swerve dies down as the edge comes near, so as not to swing back into the aim.
		return FVector(At.X, At.Y, Downed.Z) + Along * Ahead + Across * Swerve * ZigzagWidth * FMath::Min(1.0f, Left / ZigzagStep);
	}

	bool GoesToExecute(float DownedDistance, bool bSeesEnemy, float EnemyDistance)
	{
		if (!bSeesEnemy)
		{
			return DownedDistance <= ExecuteRange;
		}
		return DownedDistance <= ExecuteWhileFighting && DownedDistance < EnemyDistance;
	}

	float RoamScore(float FromHere, float NearestMate)
	{
		return FMath::Clamp(FromHere, 0.0f, RoamFar) + AloneWeight * FMath::Clamp(NearestMate, 0.0f, AloneAt);
	}
}
