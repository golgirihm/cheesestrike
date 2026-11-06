// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGoo.h"

namespace CheeseStrikeGoo
{
	float RadiusFor(float Wash)
	{
		return FMath::Lerp(ThinRadius, ThickRadius, FMath::Clamp(Wash, 0.0f, 1.0f));
	}

	bool PuddleDue(float MovedSince, float SecondsSince)
	{
		return MovedSince >= PuddleEvery || SecondsSince >= PuddleRenew;
	}

	float RadiusAt(float Full, float Since)
	{
		// Eased out: quick at first, then slowing to its full size.
		const float Grown = FMath::Clamp(Since / PuddleGrowSeconds, 0.0f, 1.0f);
		return Full * FMath::Lerp(PuddleStart, 1.0f, 1.0f - FMath::Square(1.0f - Grown));
	}

	float OpacityAt(float Since)
	{
		if (Since < 0.0f)
		{
			return 0.0f;
		}
		return FMath::Clamp(1.0f - (Since - PuddleStays) / PuddleFades, 0.0f, 1.0f);
	}

	float BlobHeightAt(float Since)
	{
		const float Grown = FMath::Clamp(Since / PuddleGrowSeconds, 0.0f, 1.0f);
		const float Slumped = FMath::Lerp(BlobHeap, 1.0f, 1.0f - FMath::Square(1.0f - Grown));
		return Slumped * OpacityAt(Since);
	}

	bool IsGone(float Since)
	{
		return Since >= PuddleStays + PuddleFades;
	}
}
