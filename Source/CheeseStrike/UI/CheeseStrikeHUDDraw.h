// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "PixelFormat.h"

class UCanvas;
class UTexture2D;

/**
 * What the HUD draws with: its typeface, text, and shapes made of
 * triangles (polygons, discs, arcs, lines) gathered into one batch and
 * drawn at once, with a colour at each corner for gradients.
 */
namespace CheeseStrikeHUDDraw
{
	/** The HUD's colours: white and greys, the cheese's yellow for an accent, red for danger. */
	inline const FLinearColor Ink(1.0f, 1.0f, 1.0f, 0.93f);
	inline const FLinearColor Soft(1.0f, 1.0f, 1.0f, 0.62f);
	inline const FLinearColor Dim(1.0f, 1.0f, 1.0f, 0.24f);
	inline const FLinearColor Rule(1.0f, 1.0f, 1.0f, 0.2f);
	inline const FLinearColor Panel(0.02f, 0.02f, 0.025f, 0.42f);
	inline const FLinearColor Accent(0.98f, 0.76f, 0.22f, 1.0f);
	inline const FLinearColor Danger(0.93f, 0.2f, 0.15f, 1.0f);
	/** A side's colour, as the lobby has it (CheeseStrikeMenu::TeamColour): The Gouda Guys' orange, the Mozza Radicals' sky blue. */
	FLinearColor SideColour(int32 Team);
	/** `Colour` with its alpha times `Alpha`. */
	FLinearColor Faded(const FLinearColor& Colour, float Alpha);

	/** The HUD's typeface: the engine's Roboto, bold and condensed, at `Size` points. */
	FSlateFontInfo Font(float Size);
	/** How big `Text` is in `InFont`, in the canvas's pixels. */
	FVector2D Measure(const FString& Text, const FSlateFontInfo& InFont);
	/**
	 * Draws `Text` with its top at `At.Y`, and its left (`Align` 0), middle
	 * (0.5) or right (1) at `At.X`, with a soft shadow unless `bShadow` is
	 * false; `Stretch` scales it.
	 */
	void Text(UCanvas* Canvas, const FString& Text, const FVector2D& At, const FSlateFontInfo& InFont, const FLinearColor& Colour, float Align = 0.0f, bool bShadow = true, const FVector2D& Stretch = FVector2D(1.0f, 1.0f));
	/** Draws `Texture` over the rectangle at `At`, `Size` big, tinted `Colour`, blended by its alpha. */
	void Picture(UCanvas* Canvas, UTexture2D* Texture, const FVector2D& At, const FVector2D& Size, const FLinearColor& Colour);
	/** A texture made in code: `Width` by `Height` pixels of `Format` from `Pixels` (four bytes each). */
	UTexture2D* MakeTexture(int32 Width, int32 Height, const TArray<uint8>& Pixels, EPixelFormat Format);

	/** Shapes gathered into one batch of triangles. */
	struct FShapes
	{
		TArray<FCanvasUVTri> Triangles;

		void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Colour);
		void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& ColourA, const FLinearColor& ColourB, const FLinearColor& ColourC);
		/** Four corners in order round. */
		void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Colour);
		void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& ColourA, const FLinearColor& ColourB, const FLinearColor& ColourC, const FLinearColor& ColourD);
		void Rect(float X, float Y, float Width, float Height, const FLinearColor& Colour);
		/** A rectangle shaded from `Left` to `Right`. */
		void RectAcross(float X, float Y, float Width, float Height, const FLinearColor& Left, const FLinearColor& Right);
		/** The outline of a rectangle, `Thickness` wide, inside it. */
		void Frame(float X, float Y, float Width, float Height, float Thickness, const FLinearColor& Colour);
		/** A convex polygon, its corners in order round. */
		void Polygon(TConstArrayView<FVector2D> Corners, const FLinearColor& Colour);
		void Disc(const FVector2D& Middle, float Radius, const FLinearColor& Colour, int32 Sides = 20);
		/** A half disc hanging below `Middle` (a drop's end), shaded from `Inner` in the middle to `Outer` at the rim. */
		void Bulb(const FVector2D& Middle, float Radius, float Depth, const FLinearColor& Inner, const FLinearColor& Outer, int32 Sides = 14);
		void Line(const FVector2D& From, const FVector2D& To, float Thickness, const FLinearColor& Colour);
		/**
		 * A band round `Middle` at `Radius`, `Thickness` wide, from `From` to
		 * `To` degrees clockwise from straight up; with `bFade`, it fades out
		 * towards both ends.
		 */
		void Arc(const FVector2D& Middle, float Radius, float Thickness, float From, float To, const FLinearColor& Colour, bool bFade);

		/** Draws them all, in the order they were added, and starts again. */
		void Draw(UCanvas* Canvas);
	};
}
