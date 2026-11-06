// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCover.h"
#include "CheeseStrikeLoader.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include <atomic>

namespace CheeseStrikeCover
{
	namespace
	{
		double StartupSince = 0.0;

		const FSlateBrush* CoverTile()
		{
			static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("lobby_tile.png"))), FVector2D(512.0, 512.0), FLinearColor::White, ESlateBrushTileType::Both);
			return &Brush;
		}

		/** How many times a counting cover has been drawn, on whichever thread draws it: for the log. */
		std::atomic<int32> CoverFramesDrawn{0};

		/** Draws nothing; counts the frames it is drawn in. */
		class SCheeseStrikeFrameCounter : public SLeafWidget
		{
		public:
			SLATE_BEGIN_ARGS(SCheeseStrikeFrameCounter) {}
			SLATE_END_ARGS()
			void Construct(const FArguments&) {}
			virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
			virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32 LayerId, const FWidgetStyle&, bool) const override
			{
				++CoverFramesDrawn;
				return LayerId;
			}
		};
	}

	int32 FramesDrawn()
	{
		return CoverFramesDrawn.load();
	}

	void ResetFramesDrawn()
	{
		CoverFramesDrawn = 0;
	}

	double StartupCoverSince()
	{
		return StartupSince;
	}

	void NoteStartupCover()
	{
		StartupSince = FPlatformTime::Seconds();
	}

	TSharedRef<SWidget> Make(const TAttribute<FText>& Saying, float Target, bool bCountFrames, bool bTargetFromSize)
	{
		// The words under the spinner, and as much room again over it, so that the spinner is in the screen's middle.
		constexpr float WordsGap = 26.0f;
		constexpr float WordsHigh = 34.0f;
		const TSharedRef<SWidget> Middle = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight() [ SNew(SBox).HeightOverride(WordsGap + WordsHigh) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SCheeseStrikeLoader).Radius(CheeseStrikeLoader::LargeRadius).Large(true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, WordsGap, 0.0f, 0.0f))
			[
				SNew(SBox).HeightOverride(WordsHigh).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Saying).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 22)).ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f))
				]
			];
		// Scaled from the window's size, the scaler is given the whole window to measure and centres the content in it.
		const TSharedRef<SWidget> Scaled = bTargetFromSize
			? StaticCastSharedRef<SWidget>(SNew(SCheeseStrikeSteadyScale).TargetFromSize(true) [ SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center) [ Middle ] ])
			: StaticCastSharedRef<SWidget>(SNew(SCheeseStrikeSteadyScale).Target(Target) [ Middle ]);
		return SNew(SOverlay)
			+ SOverlay::Slot() [ bCountFrames ? StaticCastSharedRef<SWidget>(SNew(SCheeseStrikeFrameCounter)) : SNullWidget::NullWidget ]
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.045f, 1.0f))
			]
			+ SOverlay::Slot() [ SNew(SImage).Image(CoverTile()) ]
			+ SOverlay::Slot().HAlign(bTargetFromSize ? HAlign_Fill : HAlign_Center).VAlign(bTargetFromSize ? VAlign_Fill : VAlign_Center)
			[
				Scaled
			];
	}
}
