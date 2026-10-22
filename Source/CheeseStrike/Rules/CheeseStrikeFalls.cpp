// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeFalls.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace CheeseStrikeFalls
{
	bool HasKillHeight(float KillZ)
	{
		return KillZ > NoKillHeight;
	}

	float KillHeight(const UWorld* World)
	{
		const AWorldSettings* Settings = World != nullptr ? World->GetWorldSettings() : nullptr;
		if (Settings == nullptr || !Settings->AreWorldBoundsChecksEnabled() || !HasKillHeight(Settings->KillZ))
		{
			return NoKillHeight;
		}
		return Settings->KillZ;
	}

	bool IsBelowKillHeight(const UWorld* World, float Z)
	{
		const float Height = KillHeight(World);
		return HasKillHeight(Height) && Z < Height;
	}

	bool CreditsHurter(float HurtAt, float Now)
	{
		return HurtAt >= 0.0f && Now - HurtAt <= CreditFor;
	}

	FString Words(const FString& Place)
	{
		const FString Where = Place.TrimStartAndEnd();
		return Where.IsEmpty() ? FString(TEXT("fell")) : FString::Printf(TEXT("fell into %s"), *Where.ToLower());
	}

	float DropFor(float Speed, float Gravity)
	{
		return Speed > 0.0f && Gravity > 0.0f ? Speed * Speed / (2.0f * Gravity) : 0.0f;
	}

	float DamageFor(float Drop)
	{
		if (Drop <= SafeDrop)
		{
			return 0.0f;
		}
		// Linear in the speed of the landing (as Counter-Strike's is), which goes as the root of the drop.
		const float Through = (FMath::Sqrt(Drop) - FMath::Sqrt(SafeDrop)) / (FMath::Sqrt(DeadlyDrop) - FMath::Sqrt(SafeDrop));
		return FMath::Clamp(FMath::CeilToFloat(DeadlyDamage * Through), 0.0f, DeadlyDamage);
	}

	bool BotMayStepOff(float Drop)
	{
		return DamageFor(Drop) <= 0.0f;
	}

	FString HardLandingWords()
	{
		return Words(FString());
	}
}
