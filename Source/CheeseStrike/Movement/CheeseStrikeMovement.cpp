// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMovement.h"

namespace CheeseStrikeMovement
{
	float Speed(bool bSprinting, bool bAiming, bool bCrouching)
	{
		float Factor = 1.0f;
		if (bCrouching)
		{
			Factor *= CrouchFactor;
		}
		if (bAiming)
		{
			Factor *= AimFactor;
		}
		else if (bSprinting && !bCrouching)
		{
			Factor *= SprintFactor;
		}
		return WalkSpeed * Factor;
	}

	bool CanSlide(float GroundSpeed)
	{
		return GroundSpeed >= WalkSpeed * SlideFrom;
	}

	float WithWeapon(float Speed, bool bHeavy, bool bLunging)
	{
		const float Carried = bHeavy ? Speed * HeavyFactor : Speed;
		return bLunging ? FMath::Max(Carried, LungeSpeed) : Carried;
	}

	float SlideSpeed(float From, float Elapsed)
	{
		const float Through = FMath::Clamp(Elapsed / SlideTime, 0.0f, 1.0f);
		// Holding most of its speed at first, then dropping away.
		return FMath::Lerp(From * SlideBoost, WalkSpeed * CrouchFactor, Through * Through);
	}

	float JumpHeight(float InJumpSpeed, float InGravity)
	{
		return InJumpSpeed * InJumpSpeed / (2.0f * InGravity);
	}

	float AirControl()
	{
		return AirSteer / Acceleration;
	}
}
