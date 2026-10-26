// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCredit.h"

namespace CheeseStrikeCredit
{
	TArray<int32> Assisters(TConstArrayView<FHit> Hits, int32 Killer, int32 Victim, float Now)
	{
		TArray<int32> Found;
		for (const FHit& Hit : Hits)
		{
			if (Hit.By < 0 || Hit.By == Killer || Hit.By == Victim || Now - Hit.At > AssistWindow || Hit.At > Now)
			{
				continue;
			}
			Found.AddUnique(Hit.By);
		}
		return Found;
	}

	int32 CreditIndex(float Credited, int32 Count)
	{
		if (Count <= 0)
		{
			return -1;
		}
		return FMath::FloorToInt(FMath::Max(0.0f, Credited) / CreditRotation) % Count;
	}
}
