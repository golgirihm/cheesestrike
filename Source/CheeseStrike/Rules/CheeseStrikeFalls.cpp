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
}
