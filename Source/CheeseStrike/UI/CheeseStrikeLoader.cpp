// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLoader.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Rendering/DrawElements.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/Paths.h"

namespace CheeseStrikeLoader
{
	int32 FrameAt(double Seconds)
	{
		const int32 Frame = FMath::FloorToInt32(Seconds * FramesPerSecond) % Frames;
		return Frame < 0 ? Frame + Frames : Frame;
	}

	FString FramePath(int32 Frame)
	{
		return FPaths::ProjectContentDir() / TEXT("UI") / TEXT("Spinner") / FString::Printf(TEXT("spinner_%02d.png"), FMath::Clamp(Frame, 0, Frames - 1));
	}

	namespace
	{
		/** The frames' brushes, made once from their files the first time a spinner is made (the game's thread), kept for good. */
		const TArray<TUniquePtr<FSlateDynamicImageBrush>>& Brushes()
		{
			static TArray<TUniquePtr<FSlateDynamicImageBrush>> Made;
			if (Made.IsEmpty())
			{
				for (int32 Frame = 0; Frame < Frames; ++Frame)
				{
					Made.Add(MakeUnique<FSlateDynamicImageBrush>(FName(*FramePath(Frame)), FVector2D(FrameSize, FrameSize)));
				}
			}
			return Made;
		}
	}
}

void SCheeseStrikeLoader::Construct(const FArguments& InArgs)
{
	Radius = InArgs._Radius;
	// Loaded now, on the game's thread, so that the loading screen's thread only draws them.
	CheeseStrikeLoader::Brushes();
}

FVector2D SCheeseStrikeLoader::ComputeDesiredSize(float) const
{
	return FVector2D(Radius * 2.0f, Radius * 2.0f);
}

int32 SCheeseStrikeLoader::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool) const
{
	// The frame the clock says, drawn a little bigger than the spinner's own box (the wedge fills most of its frame, not
	// all), in the middle of what it was given.
	const TArray<TUniquePtr<FSlateDynamicImageBrush>>& Frames = CheeseStrikeLoader::Brushes();
	const int32 Frame = CheeseStrikeLoader::FrameAt(FPlatformTime::Seconds());
	if (!Frames.IsValidIndex(Frame) || !Frames[Frame].IsValid())
	{
		return LayerId;
	}
	const FVector2f Size(Radius * 2.0f * CheeseStrikeLoader::DrawnOver, Radius * 2.0f * CheeseStrikeLoader::DrawnOver);
	const FVector2f Corner = (FVector2f(AllottedGeometry.GetLocalSize()) - Size) * 0.5f;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Corner)), Frames[Frame].Get(), ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
	return LayerId;
}

void SCheeseStrikeSteadyScale::Construct(const FArguments& InArgs)
{
	Target = FMath::Max(0.1f, InArgs._Target);
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

float SCheeseStrikeSteadyScale::RelativeScale(float Target, float Accumulated)
{
	return Target / FMath::Max(0.01f, Accumulated);
}

FVector2D SCheeseStrikeSteadyScale::DesiredFor(const FVector2D& Content, float Target, float Accumulated)
{
	return Content * RelativeScale(Target, Accumulated);
}

FVector2D SCheeseStrikeSteadyScale::ContentArea(const FVector2D& Allotted, float Target, float Accumulated)
{
	return Allotted / RelativeScale(Target, Accumulated);
}

float SCheeseStrikeSteadyScale::GetRelativeLayoutScale(const int32, float LayoutScaleMultiplier) const
{
	return RelativeScale(Target, LayoutScaleMultiplier);
}

FVector2D SCheeseStrikeSteadyScale::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// The content is drawn at Target over what is given, so it takes that much more (or less) room than it asks for in its own
	// units; asking for its own size alone gave it a box that much too small wherever the scale given was below the target (the
	// loading screen's, above 1920x1080), clipping its right and bottom.
	return DesiredFor(FVector2D(ChildSlot.GetWidget()->GetDesiredSize()), Target, LayoutScaleMultiplier);
}

void SCheeseStrikeSteadyScale::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	if (!ArrangedChildren.Accepts(GetVisibility()))
	{
		return;
	}
	const float Accumulated = AllottedGeometry.GetAccumulatedLayoutTransform().GetScale();
	const float Relative = RelativeScale(Target, Accumulated);
	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2f(ContentArea(FVector2D(AllottedGeometry.GetLocalSize()), Target, Accumulated)), FSlateLayoutTransform(Relative)));
}
