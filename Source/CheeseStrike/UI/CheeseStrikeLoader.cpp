// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLoader.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Layout/ArrangedChildren.h"

namespace
{
	/** Slivers the wheel is drawn of, each a thin box turned about the middle. */
	constexpr int32 Slivers = 48;
	/** Seconds the wheel takes to turn once. */
	constexpr double TurnSeconds = 1.3;
	/** Thickness of the rind, as a share of the radius. */
	constexpr float RindShare = 0.16f;
}

void SCheeseStrikeLoader::Construct(const FArguments& InArgs)
{
	Radius = InArgs._Radius;
}

FVector2D SCheeseStrikeLoader::ComputeDesiredSize(float) const
{
	return FVector2D(Radius * 2.0f, Radius * 2.0f);
}

int32 SCheeseStrikeLoader::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const
{
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* Dot = FCoreStyle::Get().GetBrush("Throbber.CircleChunk");
	const FVector2f Middle = FVector2f(AllottedGeometry.GetLocalSize()) * 0.5f;
	const double Now = FPlatformTime::Seconds();
	const float Turn = static_cast<float>(FMath::Fmod(Now / TurnSeconds, 1.0)) * UE_TWO_PI;
	// The bite opens and closes as the wheel turns: between about 30 and 70 degrees.
	const float Bite = FMath::DegreesToRadians(50.0f + 20.0f * FMath::Sin(static_cast<float>(Now) * 5.0f));
	const float Step = UE_TWO_PI / Slivers;
	const float Width = UE_TWO_PI * Radius / Slivers * 1.6f;
	const float RindLength = Radius;
	const float BodyLength = Radius * (1.0f - RindShare);
	const FLinearColor Rind(0.78f, 0.42f, 0.06f);
	const FLinearColor Body(1.0f, 0.8f, 0.18f);
	const FLinearColor Hole(0.82f, 0.55f, 0.08f);

	const auto Sliver = [&](int32 Layer, float Angle, float Length, const FLinearColor& Colour)
	{
		const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(FVector2f(Length, Width), FSlateLayoutTransform(FVector2f(Middle.X, Middle.Y - Width * 0.5f)));
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, Layer, Geometry, White, ESlateDrawEffect::None, Angle, FVector2f(0.0f, Width * 0.5f), FSlateDrawElement::RelativeToElement, Colour);
	};
	for (int32 i = 0; i < Slivers; ++i)
	{
		const float Relative = i * Step;
		if (Relative < Bite)
		{
			continue;
		}
		Sliver(LayerId, Turn + Relative, RindLength, Rind);
	}
	for (int32 i = 0; i < Slivers; ++i)
	{
		const float Relative = i * Step;
		if (Relative < Bite)
		{
			continue;
		}
		Sliver(LayerId + 1, Turn + Relative, BodyLength, Body);
	}
	// A few holes, turning with the wheel (none where the bite is).
	const float Holes[][2] = { { 1.6f, 0.55f }, { 2.9f, 0.34f }, { 4.1f, 0.6f }, { 5.3f, 0.3f } };
	for (const float (&At)[2] : Holes)
	{
		if (At[0] < Bite + 0.35f)
		{
			continue;
		}
		const float Size = Radius * 0.2f;
		const float Away = Radius * At[1];
		const float Angle = Turn + At[0];
		const FVector2f Centre(Middle.X + FMath::Cos(Angle) * Away, Middle.Y + FMath::Sin(Angle) * Away);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(Centre - FVector2f(Size, Size) * 0.5f)), Dot, ESlateDrawEffect::None, Hole);
	}
	return LayerId + 2;
}

void SCheeseStrikeSteadyScale::Construct(const FArguments& InArgs)
{
	Target = FMath::Max(0.1f, InArgs._Target);
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

float SCheeseStrikeSteadyScale::GetRelativeLayoutScale(const int32, float LayoutScaleMultiplier) const
{
	return Target / FMath::Max(0.01f, LayoutScaleMultiplier);
}

void SCheeseStrikeSteadyScale::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	if (!ArrangedChildren.Accepts(GetVisibility()))
	{
		return;
	}
	const float Relative = Target / FMath::Max(0.01f, AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());
	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2f(AllottedGeometry.GetLocalSize()) / Relative, FSlateLayoutTransform(Relative)));
}
