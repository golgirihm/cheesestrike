// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSounds.h"

namespace CheeseStrikeSounds
{
	FString OofPath(int32 Variant)
	{
		const int32 Number = FMath::Abs(Variant) % Oofs + 1;
		return FString::Printf(TEXT("/Game/OpenGameArt/Hurt/SW_Oof%d.SW_Oof%d"), Number, Number);
	}

	bool ShouldOof(float Lost, float SinceLast)
	{
		return Lost > 0.0f && SinceLast >= OofEvery;
	}

	FString PickupPath(int32 Variant)
	{
		const int32 Number = FMath::Abs(Variant) % Pickups + 1;
		return FString::Printf(TEXT("/Game/Audio/Sounds/Foley_Misc/Lyra_WeaponSwap_0%d.Lyra_WeaponSwap_0%d"), Number, Number);
	}

	EScore ScoreSound(int32 KillsBefore, int32 Kills, int32 AssistsBefore, int32 Assists)
	{
		if (Kills > KillsBefore && KillsBefore >= 0)
		{
			return EScore::Kill;
		}
		if (Assists > AssistsBefore && AssistsBefore >= 0)
		{
			return EScore::Assist;
		}
		return EScore::None;
	}
}
