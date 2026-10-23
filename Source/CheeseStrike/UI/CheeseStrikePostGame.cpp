// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePostGame.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrike.h"
#include "CheeseStrikePlayerController.h"

namespace
{
	/** Over everything: the menu (100) and the loading cover (1000) too. */
	constexpr int32 PostGameOrder = 1100;

	const FSlateBrush* PostGameTile()
	{
		static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("lobby_tile.png"))), FVector2D(256.0, 256.0), FLinearColor::White, ESlateBrushTileType::Both);
		return &Brush;
	}
}

void UCheeseStrikePostGameSubsystem::Deinitialize()
{
	Release();
	Super::Deinitialize();
}

void UCheeseStrikePostGameSubsystem::HoldWhatIsShown()
{
	if (IsHeld())
	{
		return;
	}
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	FViewport* Surface = Viewport != nullptr ? Viewport->Viewport : nullptr;
	TArray<FColor> Pixels;
	const FIntPoint Size = Surface != nullptr ? Surface->GetSizeXY() : FIntPoint::ZeroValue;
	// The view as last drawn: the match behind, the HUD's results over it (the menu's Slate isn't in it, and there is none).
	if (Size.X > 0 && Size.Y > 0 && Surface->ReadPixels(Pixels) && Pixels.Num() == Size.X * Size.Y)
	{
		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(Pixels.Num() * 4);
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			// FColor is laid out as BGRA, as the brush wants; the view's alpha means nothing here.
			Bytes[Index * 4 + 0] = Pixels[Index].B;
			Bytes[Index * 4 + 1] = Pixels[Index].G;
			Bytes[Index * 4 + 2] = Pixels[Index].R;
			Bytes[Index * 4 + 3] = 255;
		}
		Picture = FSlateDynamicImageBrush::CreateWithImageData(FName(*FString::Printf(TEXT("CheeseStrikePostGame%d"), ++Grabs)), FVector2D(Size.X, Size.Y), Bytes);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: the results held up until the lobby is ready (%s)"), Picture.IsValid() ? *FString::Printf(TEXT("%d x %d"), Size.X, Size.Y) : TEXT("no picture"));
	TSharedRef<SWidget> Screen = Picture.IsValid()
		? StaticCastSharedRef<SWidget>(SNew(SScaleBox).Stretch(EStretch::Fill) [ SNew(SImage).Image(Picture.Get()) ])
		: StaticCastSharedRef<SWidget>(SNew(SImage).Image(PostGameTile()));
	Show(Screen);
}

void UCheeseStrikePostGameSubsystem::Hold(TSharedRef<SWidget> Screen)
{
	Release();
	Show(Screen);
}

void UCheeseStrikePostGameSubsystem::Show(TSharedRef<SWidget> Screen)
{
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (Viewport == nullptr)
	{
		return;
	}
	Cover = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.045f, 1.0f))
		]
		+ SOverlay::Slot() [ Screen ]
		// Where the lobby's own status shows, so that the eye goes from one to the other.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 56.0f, 40.0f))
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)).Padding(FMargin(16.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCircularThrobber).Radius(10.0f).NumPieces(8).Period(1.0f).PieceImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk")).ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Back to the lobby"))).Font(FCoreStyle::GetDefaultFontStyle(FName("Regular"), 16)).ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f))
				]
			]
		];
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), PostGameOrder);
	HeldSince = FPlatformTime::Seconds();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCheeseStrikePostGameSubsystem::Watch));
}

bool UCheeseStrikePostGameSubsystem::Watch(float DeltaSeconds)
{
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	const ACheeseStrikePlayerController* Player = World != nullptr ? Cast<ACheeseStrikePlayerController>(GetGameInstance()->GetFirstLocalPlayerController(World)) : nullptr;
	const bool bTimedOut = FPlatformTime::Seconds() - HeldSince > HoldFor;
	if ((Player != nullptr && Player->IsDoneWithTheResults()) || bTimedOut)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: let go after %.1f s%s"), FPlatformTime::Seconds() - HeldSince, bTimedOut ? TEXT(", the lobby never ready") : TEXT(""));
		Ticker.Reset();
		Release();
		return false;
	}
	return true;
}

void UCheeseStrikePostGameSubsystem::Release()
{
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	if (Cover.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
		}
		Cover.Reset();
	}
	Picture.Reset();
}
