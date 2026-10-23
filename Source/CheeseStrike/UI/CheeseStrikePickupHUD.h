// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AHUD;
class UCanvas;
class UWorld;

namespace CheeseStrikeHUDDraw
{
	struct FShapes;
}

/**
 * What the HUD shows of the weapons' stands (ACheeseStrikePickup), drawn
 * into its canvas in its look (ACheeseStrikeHUD calls these): the prompt
 * to press Interact for a weapon in reach that would be swapped for the
 * primary carried, naming both; the seconds until an empty stand in sight
 * has its weapon back, over it; and every stand on the minimap, a diamond
 * in the colour of what its weapon is worth, faint while empty.
 */
namespace CheeseStrikePickupHUD
{
	void Draw(AHUD* Hud, UCanvas* Canvas);
	/** On the minimap: each stand within it, at `OnMap(where it is)`, inside `Min` to `Max`, `U` the HUD's unit. */
	void DrawOnMap(CheeseStrikeHUDDraw::FShapes& Shapes, UWorld* World, TFunctionRef<FVector2D(const FVector&)> OnMap, const FVector2D& Min, const FVector2D& Max, float U);

	/** The prompt's two lines, for the key `Key` ("E", "LB"), the weapon offered and the one carried: "PRESS E  ·  TAKE THE ROQUEFORT RIFLE", "for your Cheddar Blaster". */
	FString PromptLine(const FString& Key, const FString& Offered);
	FString PromptUnder(const FString& Carried);
}
