// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/**
 * The game's loader: a wheel of cheese with a bite taken out of it that turns
 * round, the bite opening and closing as it goes. Drawn in Slate alone from
 * the clock, so that it turns anywhere Slate draws, the loading screen's own
 * thread included.
 */
class SCheeseStrikeLoader : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikeLoader) : _Radius(12.0f) {}
		SLATE_ARGUMENT(float, Radius)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	float Radius = 12.0f;
};

/**
 * Draws its content at one fixed scale (Target: the window's scale times
 * the game's UI scale) however much is put on it by the widgets above it, so
 * that the loading cover is the same size on the engine's loading screen
 * (which has no UI scale) and on the game's viewport (which has one, and
 * whose changes as a level loads would show as the cover growing and
 * shrinking).
 */
class SCheeseStrikeSteadyScale : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCheeseStrikeSteadyScale) : _Target(1.0f) {}
		SLATE_ARGUMENT(float, Target)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual float GetRelativeLayoutScale(const int32 ChildIndex, float LayoutScaleMultiplier) const override;

private:
	float Target = 1.0f;
};
