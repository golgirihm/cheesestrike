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
		return Lost >= OofFrom && SinceLast >= OofEvery;
	}

	int32 NextOof(int32 Last, int32 Random)
	{
		// One of the others: counted past the last one, so that every other grunt is as likely.
		const int32 Pick = FMath::Abs(Random) % (Oofs - 1);
		if (Last < 0 || Last >= Oofs)
		{
			return FMath::Abs(Random) % Oofs;
		}
		return Pick >= Last ? Pick + 1 : Pick;
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
