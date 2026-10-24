// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenadeHUD.h"
#include "UI/CheeseStrikeHUDDraw.h"

namespace CheeseStrikeGrenadeHUD
{
	void DrawIcon(CheeseStrikeHUDDraw::FShapes& Shapes, const FVector2D& Middle, float Radius)
	{
		const FLinearColor Cheese(1.0f, 0.78f, 0.18f, 0.95f);
		const FLinearColor Groove(0.62f, 0.4f, 0.06f, 0.9f);
		const FLinearColor Hole(0.7f, 0.46f, 0.06f, 0.95f);
		const FLinearColor Steel(0.72f, 0.72f, 0.74f, 0.95f);
		const float Thin = FMath::Max(1.0f, Radius * 0.12f);
		// The egg: a little narrower than high, fuller below.
		const float Wide = Radius * 0.8f;
		TArray<FVector2D, TInlineAllocator<28>> Egg;
		for (int32 Index = 0; Index < 28; ++Index)
		{
			const float Angle = UE_TWO_PI * Index / 28.0f;
			const float Down = FMath::Sin(Angle);
			Egg.Add(Middle + FVector2D(FMath::Cos(Angle) * Wide, Down * Radius * (Down > 0.0f ? 1.0f : 0.92f)));
		}
		// The fuse on top, under its cap, and the ring on its left, drawn first so that the egg's rim sits over them.
		const FVector2D Top = Middle + FVector2D(0.0f, -Radius * 0.88f);
		Shapes.Arc(Top + FVector2D(-Radius * 0.62f, -Radius * 0.28f), Radius * 0.32f, Thin, 0.0f, 360.0f, Steel, false);
		Shapes.Rect(Top.X - Radius * 0.3f, Top.Y - Radius * 0.42f, Radius * 0.6f, Radius * 0.5f, Steel);
		Shapes.Rect(Top.X - Radius * 0.38f, Top.Y - Radius * 0.56f, Radius * 0.76f, Radius * 0.18f, Steel);
		Shapes.SmoothPolygon(Egg, Cheese);
		// The pineapple's grooves: two down it, two across.
		for (const float Across : {-0.36f, 0.36f})
		{
			Shapes.Line(Middle + FVector2D(Across * Wide, -Radius * 0.72f), Middle + FVector2D(Across * Wide * 1.15f, Radius * 0.74f), Thin, Groove);
		}
		for (const float Down : {-0.3f, 0.3f})
		{
			const float Half = Wide * FMath::Sqrt(1.0f - Down * Down) * 0.9f;
			Shapes.Line(Middle + FVector2D(-Half, Down * Radius), Middle + FVector2D(Half, Down * Radius), Thin, Groove);
		}
		// A few holes in the cheese.
		Shapes.Disc(Middle + FVector2D(-Wide * 0.62f, Radius * 0.02f), Radius * 0.12f, Hole, 10);
		Shapes.Disc(Middle + FVector2D(Wide * 0.05f, Radius * 0.56f), Radius * 0.1f, Hole, 10);
		Shapes.Disc(Middle + FVector2D(Wide * 0.12f, -Radius * 0.5f), Radius * 0.09f, Hole, 10);
		// The spoon: from the cap over to the right and down the egg's side.
		const FVector2D Cap = Top + FVector2D(Radius * 0.3f, -Radius * 0.48f);
		const FVector2D Over = Middle + FVector2D(Wide * 0.98f, -Radius * 0.62f);
		const FVector2D Down = Middle + FVector2D(Wide * 1.06f, Radius * 0.35f);
		Shapes.Line(Cap, Over, Thin * 1.3f, Steel);
		Shapes.Line(Over, Down, Thin * 1.3f, Steel);
	}
}
