// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeFootsteps.h"

namespace CheeseStrikeFootsteps
{
	EGait GaitFor(bool bAlive, bool bOnGround, bool bSliding, bool bCrouched, bool bSprinting, float GroundSpeed)
	{
		if (!bAlive || !bOnGround || bSliding || GroundSpeed < MinSpeed)
		{
			return EGait::None;
		}
		if (bCrouched)
		{
			return EGait::Crouch;
		}
		return bSprinting ? EGait::Sprint : EGait::Walk;
	}

	float Stride(EGait Gait)
	{
		switch (Gait)
		{
		case EGait::Crouch:
			return CrouchStride;
		case EGait::Walk:
			return WalkStride;
		case EGait::Sprint:
			return SprintStride;
		default:
			return 0.0f;
		}
	}

	float Volume(EGait Gait)
	{
		switch (Gait)
		{
		case EGait::Crouch:
			return CrouchVolume;
		case EGait::Walk:
			return WalkVolume;
		case EGait::Sprint:
			return SprintVolume;
		default:
			return 0.0f;
		}
	}

	bool Advance(float& Progress, EGait Gait, float Distance)
	{
		const float Long = Stride(Gait);
		if (Long <= 0.0f)
		{
			Progress = StartProgress;
			return false;
		}
		Progress += FMath::Max(0.0f, Distance) / Long;
		if (Progress >= 1.0f)
		{
			// A long frame still makes one step; what is left over carries on, but not past a stride.
			Progress = FMath::Min(Progress - 1.0f, 0.99f);
			return true;
		}
		return false;
	}
}
