// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCurtain.h"
#include "CheeseStrikeLoader.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
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
#include "Widgets/Layout/SBorder.h"
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

	/**
	 * The scale the cover is drawn at, on the loading screen and in the game alike: the window's, times the game's UI scale at
	 * `Viewport`'s size. The loading screen draws at the window's scale alone, and the game's UI scale can change as a level
	 * loads, so the cover measures the scale it is given and makes up the difference (SCheeseStrikeSteadyScale).
	 */
	float CoverTarget(const UGameViewportClient* Viewport)
	{
		float Window = 1.0f;
		if (Viewport != nullptr && Viewport->GetWindow().IsValid() && Viewport->GetWindow()->GetNativeWindow().IsValid())
		{
			Window = Viewport->GetWindow()->GetNativeWindow()->GetDPIScaleFactor();
		}
		return FSlateApplication::Get().GetApplicationScale() * Window * ViewportScale(Viewport);
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
	TSharedRef<SWidget> MakeCover(const TAttribute<FText>& Saying, float Target, bool bCountFrames = false)
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
				SNew(SCheeseStrikeSteadyScale).Target(Target)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCheeseStrikeLoader).Radius(16.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock).Text(Saying).Font(FCoreStyle::GetDefaultFontStyle(FName("Bold"), 22)).ColorAndOpacity(FLinearColor(0.92f, 0.9f, 0.86f))
					]
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
	CoverScale = CoverTarget(GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr);
	if (!IsMoviePlayerEnabled() || GetMoviePlayer() == nullptr)
	{
		return;
	}
	FLoadingScreenAttributes Screen;
	Screen.bAutoCompleteWhenLoadingCompletes = true;
	Screen.bMoviesAreSkippable = false;
	Screen.bWaitForManualStop = false;
	Screen.MinimumLoadingScreenDisplayTime = -1.0f;
	Screen.WidgetLoadingScreen = MakeCover(FText::FromString(LoadingTo), CoverScale, true);
	LoadingScreenFrames = 0;
	LoadingSince = FPlatformTime::Seconds();
	GetMoviePlayer()->SetupLoadingScreen(Screen);
	UE_LOG(LogCheeseStrike, Display, TEXT("The loading screen is up: %s"), *LoadingTo);
}

void UCheeseStrikeCurtainSubsystem::RaiseForTravel(const FString& PendingURL)
{
	// A listen server's travel waits the net driver's ServerTravelPause (a few seconds by default, see DefaultEngine.ini) for its
	// clients to follow before the level loads. Without this the old level stays on, a spinner in a corner of its lobby, and
	// then the loading screen comes: two trips for one. The cover goes up now, the loading screen takes over from it.
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;
	if (Viewport == nullptr || World == nullptr || !IsMoviePlayerEnabled() || GetMoviePlayer() == nullptr)
	{
		return;
	}
	Lift();
	const FString Where = WhereTo(FURL(nullptr, *PendingURL, TRAVEL_Absolute).Map, World);
	Curtain = MakeCover(FText::FromString(Where), CoverTarget(Viewport));
	Viewport->AddViewportWidgetContent(Curtain.ToSharedRef(), CurtainOrder);
	UE_LOG(LogCheeseStrike, Display, TEXT("The cover goes up for the travel: %s"), *Where);
}

void UCheeseStrikeCurtainSubsystem::OnLevelLoaded(UWorld* World)
{
	UGameViewportClient* Viewport = GetGameInstance() != nullptr ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (World == nullptr || !World->IsGameWorld() || Viewport == nullptr || World != GetGameInstance()->GetWorld())
	{
		return;
	}
	Lift();
	if (!LoadingTo.IsEmpty() && LoadingSince > 0.0)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("The loading screen was drawn %d times over the %.1f s the level took to load"), LoadingScreenFrames.load(), FPlatformTime::Seconds() - LoadingSince);
	}
	// Where to (as the loading screen said it, if there was one), then, on a level the lobby sent everyone to, how many are ready.
	const FString Where = !LoadingTo.IsEmpty() ? LoadingTo : WhereTo(World->GetOutermost()->GetName(), nullptr);
	LoadingTo.Reset();
	LoadingSince = 0.0;
	TWeakObjectPtr<UWorld> Watched = World;
	// The text and loader stay the size they had under the loading screen, whatever the new world's viewport scales its UI by
	// as it takes over (the cover measures the scale it is given and makes it up: SCheeseStrikeSteadyScale).
	const float Held = CoverScale > 0.0f ? CoverScale : CoverTarget(Viewport);
	Curtain = MakeCover(TAttribute<FText>::CreateLambda([Watched, Where]() { return FText::FromString(SayingFor(Watched.Get(), Where)); }), Held);
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
