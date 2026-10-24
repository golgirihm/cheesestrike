// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

namespace CheeseStrikeHUDDraw
{
	struct FShapes;
}

/**
 * The cheese grenade as the HUD shows it, beside the ammunition (one for
 * each carried, ACheeseStrikeHUD calls this): the frag grenade the game
 * throws (ACheeseStrikeGrenade), in yellow cheese with its holes, the
 * pineapple's grooves, the steel fuse on top, the spoon down its right
 * and the pin's ring on its left.
 */
namespace CheeseStrikeGrenadeHUD
{
	/** Draws one into `Shapes`, its egg's middle at `Middle`, `Radius` its half height (the fuse rises past it). */
	void DrawIcon(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Middle, float Radius);
}
