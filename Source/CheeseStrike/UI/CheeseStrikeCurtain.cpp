// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCurtain.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrike.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeRules.h"

namespace
{
	/** Over everything, the menu (100) included. */
	constexpr int32 CurtainOrder = 1000;

	const FSlateBrush* Tile()
	{
		static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("lobby_tile.png"))), FVector2D(256.0, 256.0), FLinearColor::White, ESlateBrushTileType::Both);
		return &Brush;
	}
}

void UCheeseStrikeCurtainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCheeseStrikeCurtainSubsystem::OnLevelLoaded);
}

void UCheeseStrikeCurtainSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadedHandle);
	Lift();
	Super::Deinitialize();
}

void UCheeseStrikeCurtainSubsystem::OnLevelLoaded(UWorld* World)
{
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (World == nullptr || !World->IsGameWorld() || Viewport == nullptr || World != GetGameInstance()->GetWorld())
	{
		return;
	}
	Lift();
	// Where to, if it is one of the game's maps.
	const int32 MapIndex = CheeseStrikeRules::MapIndexFor(World->GetOutermost()->GetName());
	const FString Saying = MapIndex >= 0 ? FString::Printf(TEXT("Going to %s"), CheeseStrikeRules::Maps[MapIndex].Name) : FString(TEXT("Loading"));
	Curtain = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.045f, 1.0f))
		]
		+ SOverlay::Slot() [ SNew(SImage).Image(Tile()) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCircularThrobber).Radius(12.0f).NumPieces(8).Period(1.0f).PieceImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk")).ColorAndOpacity(FLinearColor::White)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock).Text(FText::FromString(Saying)).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 22)).ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f))
			]
		];
	Viewport->AddViewportWidgetContent(Curtain.ToSharedRef(), CurtainOrder);
	CoveredWorld = World;
	UpSince = FPlatformTime::Seconds();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCheeseStrikeCurtainSubsystem::Watch));
}

bool UCheeseStrikeCurtainSubsystem::Watch(float DeltaSeconds)
{
	UWorld* World = CoveredWorld.Get();
	const ACheeseStrikePlayerController* Player = World != nullptr && GetGameInstance() != nullptr ? Cast<ACheeseStrikePlayerController>(GetGameInstance()->GetFirstLocalPlayerController(World)) : nullptr;
	if (World == nullptr || (Player != nullptr && Player->IsViewReady()) || FPlatformTime::Seconds() - UpSince > CurtainFor)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("The loading cover is lifted after %.1f s%s"), FPlatformTime::Seconds() - UpSince, Player != nullptr && Player->IsViewReady() ? TEXT("") : TEXT(", the view not ready"));
		Ticker.Reset();
		Lift();
		return false;
	}
	return true;
}

void UCheeseStrikeCurtainSubsystem::Lift()
{
	if (Ticker.IsValid())
	{
		FTSTicker::RemoveTicker(Ticker);
		Ticker.Reset();
	}
	if (Curtain.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Curtain.ToSharedRef());
		}
		Curtain.Reset();
	}
	CoveredWorld.Reset();
}
