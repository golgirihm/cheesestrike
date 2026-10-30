// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeRange.h"
#include "Rules/CheeseStrikeBots.h"

namespace CheeseStrikeRange
{
	const TCHAR* const Level = TEXT("/Game/CheeseStrike/Maps/Lvl_Range");
	const TCHAR* const DummySpotTag = TEXT("DummySpot");

	FString PracticeOptions()
	{
		return FString::Printf(TEXT("?Mode=FreeForAll?Bots=%d?BotSkill=%s?Mice=0?StartNow?Countdown=0?KillLimit=%d?RespawnDelay=%.1f"),
			Dummies, CheeseStrikeBots::DummyKey, KillLimit, RespawnDelay);
	}

	FString PracticeUrl()
	{
		return FString(Level) + PracticeOptions();
	}

	int32 DummySpotFor(const TArray<int32>& Taken, int32 Spots)
	{
		if (Spots <= 0)
		{
			return INDEX_NONE;
		}
		for (int32 Spot = 0; Spot < Spots; ++Spot)
		{
			if (!Taken.Contains(Spot))
			{
				return Spot;
			}
		}
		return Taken.Num() % Spots;
	}

	int32 TargetStage(float Health)
	{
		const float Share = FMath::Clamp(Health / TargetHealth, 0.0f, 1.0f);
		return Share > 2.0f / 3.0f ? 0 : (Share > 1.0f / 3.0f ? 1 : 2);
	}

	float GrownShare(float Since)
	{
		return FMath::Clamp(Since / GrowSeconds, 0.1f, 1.0f);
	}
}
