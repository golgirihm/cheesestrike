// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CheeseStrikeHUDRules.h"

class ACheeseStrikeCheese;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;
namespace CheeseStrikeHUDDraw { struct FShapes; }

/**
 * The minimap's picture of a level, made from the level itself the first
 * time it shows, so that every map has one without anything made for it:
 * every visible static mesh that blocks players, outlined from above by
 * its collision shapes (a box, a convex hull, a sphere or capsule; its
 * bounds if it has none), with how high each runs. Each frame the pieces
 * near the player are turned and drawn by their height against the
 * player's (CheeseStrikeHUD::LayerFor): what is over their head is left
 * out, a level below is dark, floors a little lighter, cover lighter, walls
 * light. The cheese is kept apart and read as it is now: an eaten block
 * is gone, a bitten one paler, a falling one where it is. So are the
 * doors, whose leaves slide aside as anyone comes up to them.
 */
class CHEESESTRIKE_API FCheeseStrikeMinimap
{
public:
	/** One collision shape of the level seen from above: its outline (world X and Y, convex), its middle and reach, and how high it runs. */
	struct FPiece
	{
		TArray<FVector2D> Outline;
		FVector2D Middle = FVector2D::ZeroVector;
		float Radius = 0.0f;
		float Bottom = 0.0f;
		float Top = 0.0f;
		bool bRamp = false;
		/** At least CheeseStrikeHUD::WideFrom across: ground, not a wall. */
		bool bWide = false;
	};

	/** Reads `World`'s pieces, unless they were read already. */
	void Gather(UWorld* World);
	bool IsGatheredFor(const UWorld* World) const { return World != nullptr && GatheredFor.Get() == World; }
	const TArray<FPiece>& GetPieces() const { return Pieces; }
	/** What the level's pieces cover, from above (world X and Y); not valid before they are gathered, or with none. */
	const FBox2D& GetBounds() const { return Bounds; }
	/** The blocks of cheese still there. */
	int32 NumCheese() const;

	/**
	 * Adds to `Shapes` the level round `Middle` (world X and Y) as a player
	 * facing `ViewYaw` with their feet at `Feet` sees it, on the screen's
	 * square from `Min` to `Max`, its corners rounded to `Corner` pixels,
	 * at `Scale` pixels a cm.
	 */
	void Draw(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Min, const FVector2D& Max, const FVector2D& Middle, float ViewYaw, float Feet, float Scale, float Corner = 0.0f);

	/** Adds the pieces of `Mesh`, placed at `ToWorld`, to `Out`. */
	static void PiecesOf(const UStaticMesh* Mesh, const FTransform& ToWorld, TArray<FPiece>& Out);

private:
	struct FCheese
	{
		TWeakObjectPtr<ACheeseStrikeCheese> Block;
		TArray<FPiece> Pieces;
		FTransform At;
	};

	void DrawPiece(CheeseStrikeHUDDraw::FShapes& Shapes, const FPiece& Piece, const FLinearColor& Colour, const FVector2D& Min, const FVector2D& Max, const FVector2D& Middle, float ViewYaw, float Scale) const;

	/** The level's pieces, lowest top first, so that higher ones are drawn over lower. */
	TArray<FPiece> Pieces;
	FBox2D Bounds = FBox2D(ForceInit);
	TArray<FCheese> Cheese;
	/** The doors, read as they are each frame: a door that opens shows open. */
	TArray<TWeakObjectPtr<AActor>> Doors;
	TWeakObjectPtr<UWorld> GatheredFor;
	/** The edge of the square being drawn, its corners rounded: what a piece reaching past it is cut to. */
	TArray<FVector2D> Edge;
	float Corner = 0.0f;
};
