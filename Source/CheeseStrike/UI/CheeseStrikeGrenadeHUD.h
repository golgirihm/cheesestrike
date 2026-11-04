// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/CheeseStrikeGrenadeRules.h"

namespace CheeseStrikeHUDDraw
{
	struct FShapes;
}

/**
 * The grenades as the HUD shows them, beside the ammunition (one for each
 * carried, ACheeseStrikeHUD calls this): the cheese grenade, the frag the
 * game throws (ACheeseStrikeGrenade), in yellow cheese with its holes, the
 * pineapple's grooves, the steel fuse on top, the spoon down its right and
 * the pin's ring on its left; or the Smoked Gouda, a dark smoked wheel with
 * a wisp of smoke rising off it. The loadout page's pictures of them are
 * drawn alike by tools/art/grenade_icons.py.
 */
namespace CheeseStrikeGrenadeHUD
{
	/** Draws one of `Kind` into `Shapes`, its middle at `Middle`, `Radius` its half height (the fuse, or the wisp, rises past it). */
	void DrawIcon(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Middle, float Radius, CheeseStrikeGrenades::EGrenadeKind Kind = CheeseStrikeGrenades::EGrenadeKind::Cheese);
	/** The Smoked Gouda alone: a dark smoked wheel, its rind edge lighter, a paler cut face, and a wisp. */
	void DrawSmokeIcon(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Middle, float Radius);
}
