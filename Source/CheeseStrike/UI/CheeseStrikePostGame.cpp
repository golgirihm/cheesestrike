// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikePostGame.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SOverlay.h"
#include "CheeseStrike.h"
#include "CheeseStrikePlayerController.h"

namespace
{
	/** Over everything: the menu (100) and the loading cover (1000) too. */
	constexpr int32 PostGameOrder = 1100;
	/** What the frame asked of the engine's screenshots is called, to know it from a play script's. */
	const TCHAR* const PostGameFrameName = TEXT("CheeseStrikePostGameFrame");

	const FSlateBrush* PostGameTile()
	{
		static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("lobby_tile.png"))), FVector2D(512.0, 512.0), FLinearColor::White, ESlateBrushTileType::Both);
		return &Brush;
	}

	bool IsPostGameFrame(const FString& Filename)
	{
		return FPaths::GetBaseFilename(Filename).StartsWith(PostGameFrameName);
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
	// Fading away over a lobby, and held again (a lobby left at once): from the start.
	Release();
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (Viewport == nullptr)
	{
		return;
	}
	HeldSince = FPlatformTime::Seconds();
	Waited = 0.0f;
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCheeseStrikePostGameSubsystem::Watch));
	if (!FApp::CanEverRender() || Viewport->Viewport == nullptr)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: the tile held up until the lobby is ready (nothing drawn)"));
		Show(SNew(SImage).Image(PostGameTile()));
		return;
	}
	// The next frame, as the engine's screenshots read it: after the view is drawn (the scene and the HUD's canvas, not
	// Slate) and before it is shown. Until it comes the screen is as it was.
	bWaiting = true;
	Listening = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UCheeseStrikePostGameSubsystem::OnFrame);
	if (!FScreenshotRequest::IsScreenshotRequested())
	{
		FScreenshotRequest::RequestScreenshot(PostGameFrameName, false, false);
	}
}

void UCheeseStrikePostGameSubsystem::OnFrame(int32 Width, int32 Height, const TArray<FColor>& Colours)
{
	const FString Asked = FScreenshotRequest::GetFilename();
	if (!IsPostGameFrame(Asked))
	{
		// A play script's, asked while ours was: saved as the engine would have, and ours asked again next tick.
		if (Width > 0 && Height > 0 && Colours.Num() == Width * Height)
		{
			FImageUtils::SaveImageByExtension(*Asked, FImageView(Colours.GetData(), Width, Height));
		}
		return;
	}
	StopListening();
	bWaiting = false;
	if (Width <= 0 || Height <= 0 || Colours.Num() != Width * Height)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: no frame to hold; the tile held up until the lobby is ready"));
		Show(SNew(SImage).Image(PostGameTile()));
		return;
	}
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(Colours.Num() * 4);
	// FColor is laid out as BGRA, as the brush wants; the alpha the engine set opaque.
	FMemory::Memcpy(Bytes.GetData(), Colours.GetData(), Bytes.Num());
	Picture = FSlateDynamicImageBrush::CreateWithImageData(FName(*FString::Printf(TEXT("CheeseStrikePostGame%d"), ++Grabs)), FVector2D(Width, Height), Bytes);
	PictureSize = FIntPoint(Width, Height);
	UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: the last frame (%d x %d) held up until the lobby is ready"), Width, Height);
	Show(SNew(SScaleBox).Stretch(EStretch::Fill) [ SNew(SImage).Image(Picture.Get()) ]);
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
		// A wait past the usual (a joiner's for the host's lobby) shows a spinner where the lobby's own status shows.
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 56.0f, 40.0f))
		[
			SNew(SBorder)
			.Visibility_Lambda([this]() { return Waited > SpinnerAfter && FadingSince < 0.0 ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)).Padding(FMargin(12.0f))
			[
				SNew(SCircularThrobber).Radius(10.0f).NumPieces(8).Period(1.0f).PieceImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk")).ColorAndOpacity(FLinearColor::White)
			]
		];
	Viewport->AddViewportWidgetContent(Cover.ToSharedRef(), PostGameOrder);
}

bool UCheeseStrikePostGameSubsystem::Watch(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	// Counted in frames drawn, as the player saw it: a level loading holds a single frame for seconds.
	Waited += FMath::Min(DeltaSeconds, 0.1f);
	if (bWaiting)
	{
		if (Now - HeldSince > FrameWait)
		{
			// No frame came (the view isn't drawn): the tile instead.
			StopListening();
			bWaiting = false;
			UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: no frame came; the tile held up until the lobby is ready"));
			Show(SNew(SImage).Image(PostGameTile()));
		}
		else if (!FScreenshotRequest::IsScreenshotRequested())
		{
			FScreenshotRequest::RequestScreenshot(PostGameFrameName, false, false);
		}
		return true;
	}
	if (FadingSince >= 0.0)
	{
		const float Left = 1.0f - static_cast<float>((Now - FadingSince) / FadeFor);
		if (Left <= 0.0f || !Cover.IsValid())
		{
			Ticker.Reset();
			Release();
			return false;
		}
		Cover->SetRenderOpacity(FMath::InterpEaseInOut(0.0f, 1.0f, Left, 2.0f));
		return true;
	}
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	const ACheeseStrikePlayerController* Player = World != nullptr ? Cast<ACheeseStrikePlayerController>(GetGameInstance()->GetFirstLocalPlayerController(World)) : nullptr;
	const bool bTimedOut = Now - HeldSince > HoldFor;
	if ((Player != nullptr && Player->IsDoneWithTheResults()) || bTimedOut)
	{
		// Faded away over the lobby, now there under it.
		UE_LOG(LogCheeseStrike, Display, TEXT("Post-game: let go after %.1f s%s"), Now - HeldSince, bTimedOut ? TEXT(", the lobby never ready") : TEXT(""));
		FadingSince = Now;
	}
	return true;
}

void UCheeseStrikePostGameSubsystem::StopListening()
{
	if (Listening.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(Listening);
		Listening.Reset();
	}
}

void UCheeseStrikePostGameSubsystem::Release()
{
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	StopListening();
	// Our frame still asked for would be saved as a file.
	if (bWaiting && FScreenshotRequest::IsScreenshotRequested() && IsPostGameFrame(FScreenshotRequest::GetFilename()))
	{
		FScreenshotRequest::Reset();
	}
	bWaiting = false;
	FadingSince = -1.0;
	if (Cover.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Cover.ToSharedRef());
		}
		Cover.Reset();
	}
	Picture.Reset();
	PictureSize = FIntPoint::ZeroValue;
}
