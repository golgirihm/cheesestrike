// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeLoader.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Rendering/DrawElements.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/Paths.h"
#include "HAL/PlatformApplicationMisc.h"

namespace CheeseStrikeLoader
{
	int32 FrameAt(double Seconds)
	{
		const int32 Frame = FMath::FloorToInt32(Seconds * FramesPerSecond) % Frames;
		return Frame < 0 ? Frame + Frames : Frame;
	}

	float FrameBlendAt(double Seconds)
	{
		const double Position = Seconds * FramesPerSecond;
		return FMath::Clamp(static_cast<float>(Position - FMath::FloorToDouble(Position)), 0.0f, 1.0f);
	}

	double TurnAt(double Seconds)
	{
		return (FrameAt(Seconds) + FrameBlendAt(Seconds)) * 360.0 / Frames;
	}

	FString FramePath(int32 Frame, bool bLarge)
	{
		const FString Folder = bLarge ? FPaths::ProjectContentDir() / TEXT("UI") / TEXT("Spinner") / TEXT("Large") : FPaths::ProjectContentDir() / TEXT("UI") / TEXT("Spinner");
		return Folder / FString::Printf(TEXT("spinner_%02d.png"), FMath::Clamp(Frame, 0, Frames - 1));
	}

	namespace
	{
		/** The frames' brushes, small or large, made once from their files the first time a spinner of them is made (the game's thread), kept for good. */
		const TArray<TUniquePtr<FSlateDynamicImageBrush>>& Brushes(bool bLarge)
		{
			static TArray<TUniquePtr<FSlateDynamicImageBrush>> Made[2];
			TArray<TUniquePtr<FSlateDynamicImageBrush>>& Set = Made[bLarge ? 1 : 0];
			if (Set.IsEmpty())
			{
				const int32 Size = bLarge ? LargeFrameSize : FrameSize;
				for (int32 Frame = 0; Frame < Frames; ++Frame)
				{
					Set.Add(MakeUnique<FSlateDynamicImageBrush>(FName(*FramePath(Frame, bLarge)), FVector2D(Size, Size)));
				}
			}
			return Set;
		}
	}
}

void SCheeseStrikeLoader::Construct(const FArguments& InArgs)
{
	Radius = InArgs._Radius;
	bLarge = InArgs._Large;
	// Loaded now, on the game's thread, so that the loading screen's thread only draws them.
	CheeseStrikeLoader::Brushes(bLarge);
}

FVector2D SCheeseStrikeLoader::ComputeDesiredSize(float) const
{
	return FVector2D(Radius * 2.0f, Radius * 2.0f);
}

int32 SCheeseStrikeLoader::OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool) const
{
	// The frame the clock says, drawn a little bigger than the spinner's own box (the wedge fills most of its frame, not
	// all), in the middle of what it was given.
	const TArray<TUniquePtr<FSlateDynamicImageBrush>>& Frames = CheeseStrikeLoader::Brushes(bLarge);
	const double Now = FPlatformTime::Seconds();
	const int32 Frame = CheeseStrikeLoader::FrameAt(Now);
	if (!Frames.IsValidIndex(Frame) || !Frames[Frame].IsValid())
	{
		return LayerId;
	}
	const FVector2f Size(Radius * 2.0f * CheeseStrikeLoader::DrawnOver, Radius * 2.0f * CheeseStrikeLoader::DrawnOver);
	const FVector2f Corner = (FVector2f(AllottedGeometry.GetLocalSize()) - Size) * 0.5f;
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Corner));
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry, Frames[Frame].Get(), ESlateDrawEffect::None, InWidgetStyle.GetColorAndOpacityTint());
	// The next frame over it as far as the clock is toward it, so that the wedge turns a little on every frame drawn rather
	// than jumping a frame's 5 degrees every other one (the frames are 5 degrees apart, so the two nearly coincide).
	const float Blend = CheeseStrikeLoader::FrameBlendAt(Now);
	const int32 Next = (Frame + 1) % Frames.Num();
	if (Blend > 0.01f && Frames[Next].IsValid())
	{
		FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		Tint.A *= Blend;
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry, Frames[Next].Get(), ESlateDrawEffect::None, Tint);
		return LayerId + 1;
	}
	return LayerId;
}

void SCheeseStrikeSteadyScale::Construct(const FArguments& InArgs)
{
	Target = FMath::Max(0.1f, InArgs._Target);
	bTargetFromSize = InArgs._TargetFromSize;
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

float SCheeseStrikeSteadyScale::TargetForSize(const FVector2D& Pixels, float WindowDpi)
{
	const double Short = FMath::Min(Pixels.X, Pixels.Y);
	return Short < 1.0 ? 1.0f : static_cast<float>(Short / 1080.0) * FMath::Max(0.1f, WindowDpi);
}

float SCheeseStrikeSteadyScale::TargetFor(const FGeometry& AllottedGeometry) const
{
	if (!bTargetFromSize)
	{
		return Target;
	}
	const FVector2D Pixels = FVector2D(AllottedGeometry.GetLocalSize()) * AllottedGeometry.GetAccumulatedLayoutTransform().GetScale();
	const FVector2D Middle = FVector2D(AllottedGeometry.LocalToAbsolute(AllottedGeometry.GetLocalSize() * 0.5f));
	return TargetForSize(Pixels, FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(static_cast<float>(Middle.X), static_cast<float>(Middle.Y)));
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
	const float Wanted = TargetFor(AllottedGeometry);
	const float Relative = RelativeScale(Wanted, Accumulated);
	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot.GetWidget(), FVector2f(ContentArea(FVector2D(AllottedGeometry.GetLocalSize()), Wanted, Accumulated)), FSlateLayoutTransform(Relative)));
}
