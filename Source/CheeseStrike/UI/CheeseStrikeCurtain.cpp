// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCurtain.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "MoviePlayer.h"
#include "Widgets/SLeafWidget.h"
#include <atomic>
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/GameViewportClient.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "CheeseStrike.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeRules.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"

namespace
{
	/** Over everything, the menu (100) included. */
	constexpr int32 CurtainOrder = 1000;

	const FSlateBrush* Tile()
	{
		static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("lobby_tile.png"))), FVector2D(512.0, 512.0), FLinearColor::White, ESlateBrushTileType::Both);
		return &Brush;
	}

	/** The scale the game's UI is drawn at in `Viewport` (the DPI curve at its size), which the cover is drawn at too. */
	float ViewportScale(const UGameViewportClient* Viewport)
	{
		FVector2D Size(1920.0, 1080.0);
		if (Viewport != nullptr)
		{
			Viewport->GetViewportSize(Size);
		}
		if (Size.X < 1.0 || Size.Y < 1.0)
		{
			return 1.0f;
		}
		return GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y)));
	}

	/** How many times the loading screen has been drawn, on its own thread, since it went up: for the log. */
	std::atomic<int32> LoadingScreenFrames{0};

	/** Draws nothing; counts the frames it is drawn in (LoadingScreenFrames). */
	class SCheeseStrikeFrameCounter : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCheeseStrikeFrameCounter) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32 LayerId, const FWidgetStyle&, bool) const override
		{
			++LoadingScreenFrames;
			return LayerId;
		}
	};

	/**
	 * The cover's looks: the lobby's dark tile, a spinner and `Saying`. Made
	 * of Slate alone, nothing of the game, so that the loading screen can draw
	 * it on its own thread while the game's is busy loading a level.
	 */
	TSharedRef<SWidget> MakeCover(const TAttribute<FText>& Saying, bool bCountFrames = false)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot() [ bCountFrames ? StaticCastSharedRef<SWidget>(SNew(SCheeseStrikeFrameCounter)) : SNullWidget::NullWidget ]
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
					SNew(STextBlock).Text(Saying).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 22)).ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f))
				]
			];
	}
}

void UCheeseStrikeCurtainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCheeseStrikeCurtainSubsystem::OnLevelLoaded);
	LoadingHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UCheeseStrikeCurtainSubsystem::OnLevelLoading);
}

void UCheeseStrikeCurtainSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadedHandle);
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(LoadingHandle);
	Lift();
	Super::Deinitialize();
}

FString UCheeseStrikeCurtainSubsystem::WhereTo(const FString& Level, const UWorld* From) const
{
	// Hosting or joining goes into the match's lobby, whatever level it waits on.
	const UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions != nullptr && Sessions->GetPending() != ESessionPending::None)
	{
		return TEXT("Going to the lobby");
	}
	// The front end's level is the lobby's too, so a lobby (or a match's results) going back there is the lobby while its session is
	// still open. Once the session is closed, leaving the lobby is going back to the menu, which is the same level.
	const bool bSessionOpen = Sessions != nullptr && Sessions->IsInAMatch();
	const bool bFrontEnd = Level.EndsWith(TEXT("Lvl_Shooter"));
	// (Not by the match's state: leaving any level, the lobby too, counts as the match's end.)
	const ACheeseStrikeGameState* Match = From != nullptr ? From->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Match != nullptr && (Match->GetPlayersExpected() > 0 || !Match->GetWinnerName().IsEmpty()))
	{
		return bSessionOpen ? TEXT("Back to the lobby") : TEXT("Back to the menu");
	}
	if (bFrontEnd)
	{
		// Starting the game is no trip anywhere: the front end is where it opens.
		return Match != nullptr ? TEXT("Back to the menu") : TEXT("Loading");
	}
	const int32 MapIndex = CheeseStrikeRules::MapIndexFor(Level);
	return MapIndex >= 0 ? FString::Printf(TEXT("Going to %s"), CheeseStrikeRules::Maps[MapIndex].Name) : FString(TEXT("Loading"));
}

void UCheeseStrikeCurtainSubsystem::OnLevelLoading(const FWorldContext& Context, const FString& Level)
{
	// Loading a level stops the game's thread, and with it the menu's spinner, for seconds (hosting, travelling): the engine's
	// loading screen draws the cover on a thread of its own meanwhile, turning, until the level has loaded and the cover
	// proper (OnLevelLoaded) goes up in its place. Not in the editor, which has no loading screen.
	if (Context.OwningGameInstance != GetGameInstance())
	{
		return;
	}
	LoadingTo = WhereTo(Level, Context.World());
	// The cover that follows is drawn at the scale of this viewport, not whatever the next world's has for its first frames.
	CoverScale = ViewportScale(GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr);
	if (LoadedAt > 0.0 && FPlatformTime::Seconds() - LoadedAt < 20.0 && Level == LoadedLevel)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("A second trip to %s, %.1f s after the last arrived there (%s):"), *Level, FPlatformTime::Seconds() - LoadedAt, *LoadingTo);
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
	}
	if (!IsMoviePlayerEnabled() || GetMoviePlayer() == nullptr)
	{
		return;
	}
	FLoadingScreenAttributes Screen;
	Screen.bAutoCompleteWhenLoadingCompletes = true;
	Screen.bMoviesAreSkippable = false;
	Screen.bWaitForManualStop = false;
	Screen.MinimumLoadingScreenDisplayTime = -1.0f;
	Screen.WidgetLoadingScreen = MakeCover(FText::FromString(LoadingTo), true);
	LoadingScreenFrames = 0;
	LoadingSince = FPlatformTime::Seconds();
	GetMoviePlayer()->SetupLoadingScreen(Screen);
	UE_LOG(LogCheeseStrike, Display, TEXT("The loading screen is up: %s"), *LoadingTo);
}

void UCheeseStrikeCurtainSubsystem::OnLevelLoaded(UWorld* World)
{
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (World == nullptr || !World->IsGameWorld() || Viewport == nullptr || World != GetGameInstance()->GetWorld())
	{
		return;
	}
	Lift();
	LoadedAt = FPlatformTime::Seconds();
	LoadedLevel = World->GetOutermost()->GetName();
	if (!LoadingTo.IsEmpty() && LoadingSince > 0.0)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("The loading screen was drawn %d times over the %.1f s the level took to load"), LoadingScreenFrames.load(), FPlatformTime::Seconds() - LoadingSince);
	}
	// Where to (as the loading screen said it, if there was one), then, on a level the lobby sent everyone to, how many are ready.
	const FString Where = !LoadingTo.IsEmpty() ? LoadingTo : WhereTo(World->GetOutermost()->GetName(), nullptr);
	LoadingTo.Reset();
	LoadingSince = 0.0;
	TWeakObjectPtr<UWorld> Watched = World;
	TSharedRef<SWidget> Cover = MakeCover(TAttribute<FText>::CreateLambda([Watched, Where]() { return FText::FromString(SayingFor(Watched.Get(), Where)); }));
	// The text and spinner stay the size they had under the loading screen: as the new world's viewport takes over its DPI scale
	// may differ for a few frames, which grew them. Drawn at the scale held from the start, whatever the viewport's now.
	const float Held = CoverScale > 0.0f ? CoverScale : ViewportScale(Viewport);
	TWeakObjectPtr<UGameViewportClient> WatchedViewport = Viewport;
	Curtain = SNew(SDPIScaler).DPIScale_Lambda([Held, WatchedViewport]()
	{
		return Held / FMath::Max(0.1f, ViewportScale(WatchedViewport.Get()));
	})
	[
		Cover
	];
	CoverScale = 0.0f;
	Viewport->AddViewportWidgetContent(Curtain.ToSharedRef(), CurtainOrder);
	GoingTo = Where;
	CoveredWorld = World;
	UpSince = FPlatformTime::Seconds();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCheeseStrikeCurtainSubsystem::Watch));
}

FString UCheeseStrikeCurtainSubsystem::SayingFor(const UWorld* World, const FString& Otherwise)
{
	const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (Match != nullptr && Match->IsWaitingForPlayers())
	{
		return FString::Printf(TEXT("Waiting for players (%d/%d)"), FMath::Min(Match->GetPlayersReady(), Match->GetPlayersExpected()), Match->GetPlayersExpected());
	}
	return Otherwise;
}

FString UCheeseStrikeCurtainSubsystem::GetSaying() const
{
	return IsUp() ? SayingFor(CoveredWorld.Get(), GoingTo) : FString();
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
