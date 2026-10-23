// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeHills.h"

class AHUD;
class APlayerState;
class UCanvas;
class UWorld;
namespace CheeseStrikeHUDDraw { struct FShapes; }

/**
 * King of the Hill on the HUD (ACheeseStrikeHill): a marker over the hill
 * wherever it is, held to the screen's edge when it is off it, with what to
 * do there (capture, defend, contested) and how far it is; while the hill
 * warns that it is moving, a smaller one over where it goes; a strip over
 * the score at the bottom saying who holds it (in its colour), whether the
 * player is in it, and when it moves; and the hill on the minimap. The
 * score itself is the HUD's score summary: the sides' points, or in the
 * solo mode the player's and the leader's. Its words and where a marker
 * goes are plain functions, tested without a world.
 */
namespace CheeseStrikeHillHUD
{
	/** The marker and the strip; returns where lines over the strip may begin (`Bottom` with no hill). */
	float Draw(AHUD* Hud, UCanvas* Canvas, float Bottom);
	/** The hill on the minimap: a ring round it in its colour, held to the frame (smaller) when it is further than the minimap reaches; and the next, faint, while it warns. */
	void DrawOnMinimap(const UWorld* World, const APlayerState* Viewer, CheeseStrikeHUDDraw::FShapes& Shapes, TFunctionRef<FVector2D(const FVector&)> OnMap, const FVector2D& Centre, float Half, float Scale, float U);

	/** What the marker over the hill says: "DEFEND" while the viewer (or their side) holds it, "CONTESTED", otherwise "CAPTURE". */
	FString MarkerWord(CheeseStrikeHills::EHold State, bool bMine);
	/** What the strip says about who holds it: "NOBODY HOLDS THE HILL", "YOUR SIDE HOLDS THE HILL", "YOU HOLD THE HILL", "<NAME> HOLD(S) THE HILL", "CONTESTED". */
	FString HoldWords(bool bTeams, CheeseStrikeHills::EHold State, bool bMine, const FString& HolderName);
	/** When it moves, in words: "MOVES IN 0:42", or with the warning on, "MOVING IN 4". */
	FString MoveWords(float SecondsLeft);
	/** Distance in words: "42 M". */
	FString DistanceWords(float Centimetres);
	/**
	 * Where a marker goes for something at `Projected` (screen pixels; for
	 * something behind the view, any point the way it lies from the middle,
	 * far off): there, when it is within `Area` (the screen less what the
	 * HUD keeps at its edges); otherwise on the edge of `Area`, the way it
	 * lies from the middle of it. `bOnEdge` says which.
	 */
	FVector2D MarkerAt(const FVector2D& Projected, const FBox2D& Area, bool& bOnEdge);
}
