// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "CheeseStrikeCameraManager.h"
#include "CheeseStrike.h"
#include "Input/CheeseStrikeInput.h"
#include "Dev/CheeseStrikePlayScript.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CheeseStrikeBotController.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeMouse.h"
#include "CheeseStrikeCharacter.h"
#include "EngineUtils.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Rules/CheeseStrikeRules.h"
#include "Settings/CheeseStrikeSettings.h"
#include "UI/CheeseStrikeTones.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "Sessions/CheeseStrikeSessions.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikeMenuFigure.h"
#include "UI/CheeseStrikeMusic.h"
#include "UI/CheeseStrikePostGame.h"
#include "Animation/CheeseStrikeBodies.h"
#include "UI/CheeseStrikePodium.h"
#include "UI/CheeseStrikePodiumHUD.h"
#include "Rules/CheeseStrikePodiums.h"

ACheeseStrikePlayerController::ACheeseStrikePlayerController()
{
	PlayerCameraManagerClass = ACheeseStrikeCameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ACheeseStrikePlayerController::BeginPlay()
{
	Super::BeginPlay();
	// The script under way, if any, goes on through this controller: a travel gives the player a new one.
	if (IsLocalPlayerController() && GetGameInstance() != nullptr)
	{
		if (UCheeseStrikePlayScriptSubsystem* Scripts = GetGameInstance()->GetSubsystem<UCheeseStrikePlayScriptSubsystem>())
		{
			PlayScript = Scripts->ScriptFor(this);
		}
		// The volumes, on the sounds of this level too.
		UCheeseStrikeSettings::ApplySound();
		// The game's window can't be made so small that the menus crowd (not the editor's, which isn't the game's).
		UGameViewportClient* Viewport = GetLocalPlayer() != nullptr ? GetLocalPlayer()->ViewportClient : nullptr;
		const TSharedPtr<SWindow> Window = Viewport != nullptr && !GIsEditor ? Viewport->GetWindow() : nullptr;
		if (Window.IsValid())
		{
			Window->SetSizeLimits(FWindowSizeLimits().SetMinWidth(CheeseStrikeMenu::MinWindowWidth).SetMinHeight(CheeseStrikeMenu::MinWindowHeight));
		}
	}
}

void ACheeseStrikePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The menu and the menu camera belong to this level: a travel leaves none of them behind over the next.
	ShowMenu(EMenuPage::None);
	HideMenuCamera();
	HidePodium();
	Super::EndPlay(EndPlayReason);
}

UCheeseStrikePlayScript* ACheeseStrikePlayerController::Play(const FString& Text)
{
	UCheeseStrikePlayScriptSubsystem* Scripts = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikePlayScriptSubsystem>() : nullptr;
	UCheeseStrikePlayScript* Next = UCheeseStrikePlayScript::FromText(Scripts != nullptr ? static_cast<UObject*>(Scripts) : this, this, Text);
	Next->TakeOver(PlayScript);
	PlayScript = Next;
	if (Scripts != nullptr)
	{
		Scripts->SetScript(Next);
	}
	return PlayScript;
}

void ACheeseStrikePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocalPlayerController() || GetWorld() == nullptr)
	{
		return;
	}
	// The script goes on with or without a character: in the lobby it may start the match.
	if (PlayScript != nullptr)
	{
		PlayScript->Advance(DeltaSeconds);
	}
	// On a level the lobby sent everyone to, the match begins when they have all said they are ready.
	if (!bReadySent && CanSayReady())
	{
		bReadySent = true;
		ServerReady();
	}
	SoundCountdown();
	UpdatePodium();
	// The results stay up until the lobby they lead back to is there: a moment before the server takes everyone back.
	if (const ACheeseStrikeGameState* Ended = GetWorld()->GetGameState<ACheeseStrikeGameState>();
		Ended != nullptr && Ended->HasMatchEnded() && Ended->GetLobbyAt() > 0.0f && Ended->GetServerWorldTimeSeconds() >= Ended->GetLobbyAt() - 0.25f)
	{
		HoldTheResults();
	}
	// The menu follows where the match is. A joiner told to follow the host to another level is still in the match,
	// though the connection to the level being left is closed while the next is reached: the lobby says where to.
	const AGameStateBase* State = GetWorld()->GetGameState();
	const bool bStarted = State != nullptr && State->HasMatchStarted();
	const bool bEnded = State != nullptr && State->HasMatchEnded();
	const AGameState* Match = Cast<AGameState>(State);
	const bool bTravelling = (Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap) || bLeaving;
	const UCheeseStrikePostGameSubsystem* PostGame = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>() : nullptr;
	const bool bResultsHeld = PostGame != nullptr && PostGame->IsHeld();
	PublishPresence(bStarted);
	const EMenuPage Wanted = CheeseStrikeMenu::PageFor(bStarted, bEnded, IsInAMatch() || bLeaving, bPaused, ChosenPage, bTravelling, bResultsHeld);
	if (Wanted != ShownPage)
	{
		ShowMenu(Wanted);
	}
	else if (Menu.IsValid())
	{
		// The pages show things that change (who is in, what was found, a spinner while hosting or joining, the options):
		// each is drawn again in the frame it changes, and only then.
		Menu->Refresh();
	}
	PlayMusic();
	// And the level is watched from the menu camera, with the figure before it, until the match is on.
	if (!bStarted || bTravelling)
	{
		ShowMenuCamera();
	}
	else
	{
		HideMenuCamera();
	}
}

void ACheeseStrikePlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
	// A player in a match told to follow it to another level (a travel relative to the server; leaving is absolute) shows
	// where to at once: the level it leaves gets no more news from the host, and would show the countdown at 0 until the new
	// one has loaded. Hosting, from a level of one's own, is not following a match: the front end says so meanwhile.
	bLeaving = TravelType != TRAVEL_Absolute && IsInAMatch();
	LeavingFor = bLeaving ? CheeseStrikeRules::MapIndexFor(FURL(nullptr, *PendingURL, TRAVEL_Absolute).Map) : INDEX_NONE;
	// Back to the lobby after the results: they stay up until it is there.
	const ACheeseStrikeGameState* Ended = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	if (bLeaving && Ended != nullptr && Ended->HasMatchEnded() && Ended->GetLobbyAt() > 0.0f)
	{
		HoldTheResults();
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid)
{
	// The server sends its own URL's options, the host's name among them; a travel keeps the options it doesn't name, so
	// without the host's this player keeps their own.
	FString Url = URL;
	const int32 At = TravelType == TRAVEL_Relative ? Url.Find(TEXT("?Nick="), ESearchCase::IgnoreCase) : INDEX_NONE;
	if (At != INDEX_NONE)
	{
		const int32 End = Url.Find(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1);
		Url.RemoveAt(At, (End == INDEX_NONE ? Url.Len() : End) - At);
	}
	Super::ClientTravelInternal_Implementation(Url, TravelType, bSeamless, MapPackageGuid);
}

bool ACheeseStrikePlayerController::IsViewReady() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	const APawn* Body = GetPawn();
	const bool bThroughCharacter = Body != nullptr && PlayerCameraManager != nullptr && PlayerCameraManager->GetViewTarget() == Body;
	// A joiner has nothing to show until the match's state has come from the host.
	const ACheeseStrikeGameState* Match = World->GetGameState<ACheeseStrikeGameState>();
	if (Match == nullptr)
	{
		return false;
	}
	// A level the lobby sent everyone to begins by itself once they are all ready, and for everyone at once: until then
	// the cover stays, saying how many are, and then the character, not the lobby, is what to wait for.
	if (Match->GetPlayersExpected() > 0)
	{
		return Match->HasMatchEnded() || (Match->HasMatchStarted() && bThroughCharacter);
	}
	return ShownPage != EMenuPage::None || bThroughCharacter;
}

bool ACheeseStrikePlayerController::IsLobbyReady() const
{
	return ShownPage == EMenuPage::Lobby && Menu.IsValid() && Menu->IsLobbyReady();
}

bool ACheeseStrikePlayerController::IsDoneWithTheResults() const
{
	const bool bFrontEnd = ShownPage == EMenuPage::Main || ShownPage == EMenuPage::Find || ShownPage == EMenuPage::Credits || ShownPage == EMenuPage::Options;
	// A level that skips the lobby (?StartNow, as the play scripts' do) goes straight into a match: the player's in it.
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	const bool bPlaying = State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded() && GetPawn() != nullptr && GetViewTarget() == GetPawn();
	return IsLobbyReady() || (bFrontEnd && !IsInAMatch()) || bPlaying;
}

void ACheeseStrikePlayerController::HoldTheResults()
{
	UCheeseStrikePostGameSubsystem* PostGame = IsLocalController() && GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>() : nullptr;
	if (PostGame != nullptr && !PostGame->IsHeld() && ShownPage == EMenuPage::None)
	{
		// The podium (or the results) as last drawn, the HUD's words over it; the host's button, which can't be pressed any
		// more, isn't in it, and goes.
		if (PodiumSkip.IsValid() && GetLocalPlayer() != nullptr && GetLocalPlayer()->ViewportClient != nullptr)
		{
			GetLocalPlayer()->ViewportClient->RemoveViewportWidgetForPlayer(GetLocalPlayer(), PodiumSkip.ToSharedRef());
			PodiumSkip.Reset();
		}
		PostGame->HoldWhatIsShown();
	}
}

void ACheeseStrikePlayerController::UpdatePodium()
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bTravelling = bLeaving || (Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap);
	const bool bWanted = Match != nullptr && !bTravelling && CheeseStrikePodiums::IsShowing(Match->HasMatchEnded(), Match->GetLobbyAt(), Match->GetServerWorldTimeSeconds());
	if (bWanted && Podium == nullptr)
	{
		ShowPodium();
	}
	if (Podium == nullptr)
	{
		return;
	}
	// The player states may still be coming to a joiner; and nothing else (a death's camera, a respawn) takes the view.
	const TArray<FPodiumPlace> Now = PodiumPlaces();
	const TArray<FPodiumPlace>& Shown = Podium->GetPlaces();
	bool bSame = Now.Num() == Shown.Num();
	for (int32 Index = 0; bSame && Index < Now.Num(); ++Index)
	{
		bSame = Now[Index].Name == Shown[Index].Name && Now[Index].Body == Shown[Index].Body && Now[Index].Team == Shown[Index].Team && Now[Index].Score == Shown[Index].Score;
	}
	if (!bSame)
	{
		Podium->Stage(Now);
	}
	if (GetViewTarget() != Podium)
	{
		SetViewTarget(Podium);
	}
}

void ACheeseStrikePlayerController::ShowPodium()
{
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Podium = GetWorld()->SpawnActor<ACheeseStrikePodium>(ACheeseStrikePodium::StaticClass(), ACheeseStrikePodium::StageAt, FRotator::ZeroRotator, Parameters);
	if (Podium == nullptr)
	{
		return;
	}
	Podium->Stage(PodiumPlaces());
	PodiumShownAt = GetWorld()->GetRealTimeSeconds();
	SetViewTarget(Podium);
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.6f, FLinearColor::Black, false, false);
	}
	FString Who;
	for (const FPodiumPlace& Placed : Podium->GetPlaces())
	{
		Who += FString::Printf(TEXT("%s%d. %s (%s, %s)"), Who.IsEmpty() ? TEXT("") : TEXT(", "), Placed.Place + 1, *Placed.Name, CheeseStrikeBodies::All()[Placed.Body].Name, *Placed.Score);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Podium: %s"), Who.IsEmpty() ? TEXT("nobody") : *Who);
	// Nothing held stays held; the host gets a button to go back sooner, which takes the keys and the pad.
	FlushPressedKeys();
	if (ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(GetPawn()))
	{
		Body->DoFire(false);
		Body->DoAimDownSights(false);
		Body->DoSprint(false);
	}
	UGameViewportClient* Viewport = GetLocalPlayer() != nullptr ? GetLocalPlayer()->ViewportClient : nullptr;
	if (CanSkipPodium() && Viewport != nullptr)
	{
		PodiumSkip = SNew(SCheeseStrikePodiumSkip).Controller(this);
		Viewport->AddViewportWidgetForPlayer(GetLocalPlayer(), PodiumSkip.ToSharedRef(), 50);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(PodiumSkip);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
}

TArray<FPodiumPlace> ACheeseStrikePlayerController::PodiumPlaces() const
{
	TArray<FPodiumPlace> Places = ACheeseStrikePodium::PlacesFor(GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr, PlayerState);
	for (int32 Place = 0; Place < Places.Num() && Place < PodiumBodies.Num(); ++Place)
	{
		if (CheeseStrikeBodies::All().IsValidIndex(PodiumBodies[Place]))
		{
			Places[Place].Body = PodiumBodies[Place];
		}
	}
	return Places;
}

void ACheeseStrikePlayerController::CheeseStrikePodiumBodies(int32 First, int32 Second, int32 Third)
{
	PodiumBodies.Reset();
	if (First != -2)
	{
		PodiumBodies = {First, Second, Third};
	}
}

void ACheeseStrikePlayerController::HidePodium()
{
	if (PodiumSkip.IsValid())
	{
		if (GetLocalPlayer() != nullptr && GetLocalPlayer()->ViewportClient != nullptr)
		{
			GetLocalPlayer()->ViewportClient->RemoveViewportWidgetForPlayer(GetLocalPlayer(), PodiumSkip.ToSharedRef());
		}
		PodiumSkip.Reset();
	}
	if (Podium != nullptr)
	{
		Podium->Destroy();
		Podium = nullptr;
	}
}

bool ACheeseStrikePlayerController::CanSkipPodium() const
{
	return IsLocalController() && GetNetMode() != NM_Client;
}

void ACheeseStrikePlayerController::SkipPodium()
{
	ACheeseStrikeGameMode* Mode = CanSkipPodium() && GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Mode != nullptr && Podium != nullptr)
	{
		Mode->BackToLobbyNow();
	}
}

void ACheeseStrikePlayerController::CheeseStrikeSkipPodium()
{
	SkipPodium();
}

void ACheeseStrikePlayerController::PlayMusic()
{
	UCheeseStrikeMusicSubsystem* Music = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeMusicSubsystem>() : nullptr;
	if (Music == nullptr)
	{
		return;
	}
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bCounting = Match != nullptr && Match->IsCountingDown();
	const bool bLeavingNow = bLeaving || (Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap);
	const UCheeseStrikePostGameSubsystem* PostGame = GetGameInstance()->GetSubsystem<UCheeseStrikePostGameSubsystem>();
	const bool bWanted = CheeseStrikeMusic::IsWanted(ShownPage, bPaused, bCounting, bLeavingNow, PostGame != nullptr && PostGame->IsHeld());
	// Out over the countdown, into the match; otherwise quicker.
	Music->Want(GetWorld(), bWanted, bCounting && Match != nullptr ? FMath::Max(Match->SecondsToStart(), 0.5f) : CheeseStrikeMusic::FadeOutSeconds);
}

bool ACheeseStrikePlayerController::CanSayReady() const
{
	const UWorld* World = GetWorld();
	const ACheeseStrikeGameState* Match = World != nullptr ? World->GetGameState<ACheeseStrikeGameState>() : nullptr;
	return IsLocalController() && World != nullptr && World->HasBegunPlay() && Match != nullptr && Match->IsWaitingForPlayers() && PlayerState != nullptr;
}

void ACheeseStrikePlayerController::ServerReady_Implementation()
{
	if (!bSaidReady)
	{
		bSaidReady = true;
		UE_LOG(LogCheeseStrike, Display, TEXT("Ready: %s"), PlayerState != nullptr ? *PlayerState->GetPlayerName() : *GetName());
	}
}

FVector2D ACheeseStrikePlayerController::LookScale() const
{
	const UCheeseStrikeSettings* Settings = UCheeseStrikeSettings::Get();
	if (Settings == nullptr || PlayScript != nullptr)
	{
		return FVector2D(1.0, 1.0);
	}
	const ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(GetPawn());
	const bool bAimed = Body != nullptr && Body->GetWeapon() != nullptr && Body->GetWeapon()->IsAimed();
	return CheeseStrikeSettings::LookScale(Settings->GetMouseSensitivity(), Settings->GetAimedSensitivity(), Settings->IsLookInverted(), bAimed);
}

void ACheeseStrikePlayerController::SoundCountdown()
{
	const ACheeseStrikeGameState* Match = GetWorld() != nullptr ? GetWorld()->GetGameState<ACheeseStrikeGameState>() : nullptr;
	const bool bCounting = Match != nullptr && Match->IsCountingDown();
	const float Left = bCounting ? Match->SecondsToStart() : 0.0f;
	// Over on the server: the match has begun here, or the server is taking everyone to its level.
	const bool bOver = bLeaving || (Match != nullptr && (Match->HasMatchStarted() || Match->GetMatchState() == MatchState::LeavingMap));
	const CheeseStrikeMenu::ECountdownTone Tone = CheeseStrikeMenu::CountdownTone(SecondSounded, bCounting, Left, bOver);
	if (Tone != CheeseStrikeMenu::ECountdownTone::None)
	{
		CheeseStrikeTones::Play(GetWorld(), Tone == CheeseStrikeMenu::ECountdownTone::End ? CheeseStrikeTones::ETone::End : CheeseStrikeTones::ETone::Second);
	}
	// The count before the match and each round sounds the same: a tone each second, the chime as it ends. It is heard of
	// until a second after its end, so that the chime sounds however this machine's clock of the server's falls.
	const float GoAt = Match != nullptr ? Match->GetGoAt() : 0.0f;
	const float SinceGo = Match != nullptr ? static_cast<float>(Match->GetServerWorldTimeSeconds()) - GoAt : 0.0f;
	const bool bCountingIn = GoAt > 0.0f && SinceGo < 1.0f && Match->HasMatchStarted() && !Match->HasMatchEnded();
	const CheeseStrikeMenu::ECountdownTone GoTone = CheeseStrikeMenu::CountdownTone(GoSounded, bCountingIn, FMath::Max(0.0f, -SinceGo), false);
	if (GoTone != CheeseStrikeMenu::ECountdownTone::None)
	{
		CheeseStrikeTones::Play(GetWorld(), GoTone == CheeseStrikeMenu::ECountdownTone::End ? CheeseStrikeTones::ETone::End : CheeseStrikeTones::ETone::Second);
	}
}

void ACheeseStrikePlayerController::PublishPresence(bool bMatchStarted)
{
	UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions == nullptr)
	{
		return;
	}
	int32 Humans = 0;
	if (const AGameStateBase* State = GetWorld()->GetGameState())
	{
		for (const APlayerState* Each : State->PlayerArray)
		{
			Humans += Each != nullptr && !Each->IsABot() ? 1 : 0;
		}
	}
	using CheeseStrikeFriends::EStatus;
	Sessions->PublishPresence(!IsInAMatch() ? EStatus::MainMenu : (bMatchStarted ? EStatus::InMatch : EStatus::InLobby), Humans);
}

bool ACheeseStrikePlayerController::IsInAMatch() const
{
	// While joining, the level left behind reports itself a client (the engine counts the connection being made) though it
	// is still the player's own: only a level with a connection of its own is a match, or the lobby would show the
	// player's own level as one, every seat a bot.
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_Standalone && World->GetNetDriver() != nullptr;
}

void ACheeseStrikePlayerController::ShowMenu(EMenuPage Page)
{
	static const TCHAR* const PageNames[] = {TEXT("none"), TEXT("main"), TEXT("find"), TEXT("lobby"), TEXT("pause"), TEXT("credits"), TEXT("settings")};
	const int32 PageIndex = static_cast<int32>(Page);
	UE_LOG(LogCheeseStrike, Display, TEXT("Menu: %s"), PageIndex < UE_ARRAY_COUNT(PageNames) ? PageNames[PageIndex] : TEXT("?"));
	ShownPage = Page;
	// The local player's viewport outlives the level, so a travel can take the menu off it.
	UGameViewportClient* Viewport = GetLocalPlayer() != nullptr ? GetLocalPlayer()->ViewportClient : nullptr;
	if (Page == EMenuPage::None)
	{
		if (Menu.IsValid() && Viewport != nullptr)
		{
			Viewport->RemoveViewportWidgetForPlayer(GetLocalPlayer(), Menu.ToSharedRef());
		}
		Menu.Reset();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		return;
	}
	if (!Menu.IsValid())
	{
		Menu = SNew(SCheeseStrikeMenu).Controller(this);
		if (Viewport != nullptr)
		{
			Viewport->AddViewportWidgetForPlayer(GetLocalPlayer(), Menu.ToSharedRef(), 100);
		}
	}
	Menu->Show(Page);
	// Only the menu takes input: nothing reaches the character, and Escape comes back through the menu.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	// Whatever was held when the menu came up is let go of.
	FlushPressedKeys();
	if (ACheeseStrikeCharacter* Body = Cast<ACheeseStrikeCharacter>(GetPawn()))
	{
		Body->DoFire(false);
		Body->DoAimDownSights(false);
		Body->DoSprint(false);
		Body->DoCrouch(false);
		Body->DoJumpEnd();
	}
}

void ACheeseStrikePlayerController::ShowMenuCamera()
{
	if (MenuCamera != nullptr || GetWorld() == nullptr)
	{
		return;
	}
	// On the shooter level's open ground: the figure on its pedestal in the
	// middle of the view, the arena behind it, the camera a few metres back
	// above head height looking down, so that the game's name clears the figure.
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Pedestal(-231.0f, -500.0f, 2.0f + ACheeseStrikeMenuFigure::PedestalHeight / 2.0f);
	MenuFigure = GetWorld()->SpawnActor<ACheeseStrikeMenuFigure>(ACheeseStrikeMenuFigure::StaticClass(), Pedestal, FRotator(0.0f, -90.0f, 0.0f), Parameters);
	MenuCamera = GetWorld()->SpawnActor<ACameraActor>(Pedestal + FVector(0.0f, -470.0f, 175.0f), FRotator(-7.0f, 90.0f, 0.0f), Parameters);
	if (MenuCamera != nullptr)
	{
		MenuCamera->GetCameraComponent()->SetFieldOfView(60.0f);
		SetViewTargetWithBlend(MenuCamera, 0.0f);
	}
}

void ACheeseStrikePlayerController::HideMenuCamera()
{
	if (MenuFigure != nullptr)
	{
		MenuFigure->Destroy();
		MenuFigure = nullptr;
	}
	if (MenuCamera == nullptr)
	{
		return;
	}
	if (GetPawn() != nullptr)
	{
		SetViewTargetWithBlend(GetPawn(), 0.0f);
	}
	MenuCamera->Destroy();
	MenuCamera = nullptr;
}

void ACheeseStrikePlayerController::ChooseMenu(EMenuPage Page)
{
	ChosenPage = Page;
	if (Page == EMenuPage::Find)
	{
		FindFromMenu();
	}
}

void ACheeseStrikePlayerController::BackFromMenu()
{
	// To the front end's main page, or, during a match, to the pause menu, still paused.
	ChosenPage = EMenuPage::Main;
}

void ACheeseStrikePlayerController::HostFromMenu()
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Host(CheeseStrikeRules::HostOptions(), Sessions->ServiceName() != TEXT("STEAM"));
	}
}

void ACheeseStrikePlayerController::SetRulesFromMenu(int32 KillLimit, float RespawnDelay, int32 Map)
{
	ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Mode == nullptr || !Mode->CanChangeLobby())
	{
		return;
	}
	Mode->SetRules(KillLimit, RespawnDelay, Map);
	// The match as offered says the same, for those still looking for one.
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->UpdateRules(FString::Printf(TEXT("?KillLimit=%d?RespawnDelay=%.1f?Map=%d"), KillLimit, RespawnDelay, Mode->GetMapIndex()));
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SetModeFromMenu(int32 ModeIndex)
{
	ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Mode == nullptr || !Mode->CanChangeLobby())
	{
		return;
	}
	Mode->SetMode(CheeseStrikeModes::ModeAt(ModeIndex));
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->UpdateRules(FString::Printf(TEXT("?KillLimit=%d?RespawnDelay=%.1f?Map=%d?Mode=%s"), Mode->GetKillLimit(), Mode->GetRespawnDelay(), Mode->GetMapIndex(), CheeseStrikeModes::Info(Mode->GetMode()).Key));
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SetRoundsFromMenu(int32 Rounds)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr; Mode != nullptr && Mode->CanChangeLobby())
	{
		Mode->SetRoundLimit(Rounds);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SetRoundTimeFromMenu(float Seconds)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr; Mode != nullptr && Mode->CanChangeLobby())
	{
		Mode->SetRoundTime(Seconds);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SetBotsFromMenu(int32 Wanted)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr; Mode != nullptr && Mode->CanChangeLobby())
	{
		Mode->SetBotsWanted(Wanted);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SetBotSkillFromMenu(int32 Skill)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr; Mode != nullptr && Mode->CanChangeLobby())
	{
		Mode->SetBotSkill(Skill);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::SwitchSideFromMenu()
{
	ServerSwitchSide();
}

void ACheeseStrikePlayerController::ServerSwitchSide_Implementation()
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->SwitchSide(this);
	}
}

void ACheeseStrikePlayerController::MoveBotFromMenu(int32 Bot)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->MoveBot(Bot);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::CheeseStrikeSwitchSide()
{
	SwitchSideFromMenu();
}

void ACheeseStrikePlayerController::CheeseStrikeMoveBot(int32 Bot)
{
	MoveBotFromMenu(Bot);
}

void ACheeseStrikePlayerController::CheeseStrikeBots()
{
	for (TActorIterator<ACheeseStrikeBotController> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("Bot: %s"), *It->Describe());
	}
}

void ACheeseStrikePlayerController::CheeseStrikeMice()
{
	static const TCHAR* const Doings[] = {TEXT("entering"), TEXT("walking"), TEXT("eating"), TEXT("fleeing")};
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It; ++It)
	{
		const ACheeseStrikeCheese* Target = It->GetTarget();
		UE_LOG(LogCheeseStrike, Display, TEXT("Mouse: %s %s at %s, fear %.0f, ate %d, body %s, sounds %s, after %s"), *It->GetName(), Doings[static_cast<int32>(It->GetDoing())],
			*It->GetActorLocation().ToCompactString(), It->GetFear(), It->GetEaten(), It->HasBody() ? TEXT("yes") : TEXT("missing"), It->HasSounds() ? TEXT("yes") : TEXT("missing"), Target != nullptr ? *FString::Printf(TEXT("%s (%s, %.0f left, bottom at %.0f)"), *Target->GetName(), *Target->GetStructure().ToString(), Target->GetHealth(), Target->GetComponentsBoundingBox().Min.Z) : TEXT("nothing"));
	}
	int32 Blocks = 0;
	for (TActorIterator<ACheeseStrikeCheese> It(GetWorld()); It; ++It)
	{
		++Blocks;
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("Mice: %d blocks of cheese left"), Blocks);
}

void ACheeseStrikePlayerController::FindFromMenu()
{
	// The friends and their games: there is no public search.
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->RefreshFriends();
	}
}

void ACheeseStrikePlayerController::StartMatchFromMenu()
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->AskToStart(false);
	}
}

void ACheeseStrikePlayerController::CancelStartFromMenu()
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->CancelStart();
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
}

void ACheeseStrikePlayerController::CheeseStrikePage(const FString& Page, const FString& Tab)
{
	EMenuPage Chosen = EMenuPage::Main;
	if (Page.Equals(TEXT("Credits"), ESearchCase::IgnoreCase))
	{
		Chosen = EMenuPage::Credits;
	}
	else if (Page.Equals(TEXT("Find"), ESearchCase::IgnoreCase))
	{
		Chosen = EMenuPage::Find;
	}
	else if (Page.Equals(TEXT("Settings"), ESearchCase::IgnoreCase) || Page.Equals(TEXT("Options"), ESearchCase::IgnoreCase))
	{
		Chosen = EMenuPage::Options;
	}
	else if (Page.Equals(TEXT("Pause"), ESearchCase::IgnoreCase))
	{
		// The pause menu: during a match, paused (or still paused) on it; with `Tab` "Ask", the host's Back to Lobby asks.
		Chosen = EMenuPage::Main;
		bPaused = true;
	}
	ChooseMenu(Chosen);
	if (Page.Equals(TEXT("Pause"), ESearchCase::IgnoreCase) && Menu.IsValid())
	{
		ShowMenu(EMenuPage::Pause);
		Menu->ConfirmBackToLobby(Tab.Equals(TEXT("Ask"), ESearchCase::IgnoreCase) && CanGoBackToLobby());
	}
	static const TCHAR* const Tabs[] = {TEXT("Controls"), TEXT("Aiming"), TEXT("Sound"), TEXT("Graphics")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs) && Menu.IsValid(); ++Index)
	{
		if (Tab.Equals(Tabs[Index], ESearchCase::IgnoreCase))
		{
			Menu->SetOptionsTab(static_cast<EOptionsTab>(Index));
		}
	}
}

void ACheeseStrikePlayerController::CheeseStrikeDropdown(int32 Number)
{
	if (Menu.IsValid())
	{
		Menu->OpenDropdown(Number);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeStart(const FString& AtOnce)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->AskToStart(!AtOnce.IsEmpty());
	}
}

void ACheeseStrikePlayerController::CheeseStrikeCancel()
{
	CancelStartFromMenu();
}

bool ACheeseStrikePlayerController::CanGoBackToLobby() const
{
	// The host of a match others can join, while it is played.
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return HasAuthority() && IsLocalController() && GetNetMode() == NM_ListenServer && State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded();
}

void ACheeseStrikePlayerController::BackToLobbyFromMenu()
{
	ACheeseStrikeGameMode* Mode = CanGoBackToLobby() ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Mode == nullptr)
	{
		return;
	}
	bPaused = false;
	ChosenPage = EMenuPage::Main;
	Mode->BackToLobbyMidMatch();
}

void ACheeseStrikePlayerController::CheeseStrikeBackToLobby()
{
	BackToLobbyFromMenu();
}

void ACheeseStrikePlayerController::LeaveFromMenu()
{
	bPaused = false;
	ChosenPage = EMenuPage::Main;
	UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>();
	// The host takes the match with them: the joiners are told first.
	if (Sessions != nullptr)
	{
		Sessions->SayGoodbye(GetWorld());
	}
	if (Sessions != nullptr && Sessions->IsInAMatch())
	{
		Sessions->Leave();
	}
	else
	{
		ClientTravel(CheeseStrikeSessions::Level, TRAVEL_Absolute);
	}
}

void ACheeseStrikePlayerController::ClientHostLeft_Implementation(bool bInMatch)
{
	UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions == nullptr || HasAuthority())
	{
		return;
	}
	Sessions->SayFarewell(bInMatch ? CheeseStrikeSessions::EFarewell::HostLeftTheMatch : CheeseStrikeSessions::EFarewell::HostClosedTheLobby);
	// Back to the front end at once, rather than waiting for the connection to time out.
	bPaused = false;
	ChosenPage = EMenuPage::Main;
	if (Sessions->IsInAMatch())
	{
		Sessions->Leave();
	}
	else
	{
		ClientTravel(CheeseStrikeSessions::Level, TRAVEL_Absolute);
	}
}

void ACheeseStrikePlayerController::ClientKicked_Implementation()
{
	UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions == nullptr || HasAuthority())
	{
		return;
	}
	Sessions->SayFarewell(CheeseStrikeSessions::EFarewell::Kicked);
	bPaused = false;
	ChosenPage = EMenuPage::Main;
	if (Sessions->IsInAMatch())
	{
		Sessions->Leave();
	}
	else
	{
		ClientTravel(CheeseStrikeSessions::Level, TRAVEL_Absolute);
	}
}

void ACheeseStrikePlayerController::ServerKick_Implementation(ACheeseStrikePlayerState* Who)
{
	// Only the host's own controller, the one on the listen server, may kick.
	if (!IsLocalController() || Who == nullptr || Who->GetPlayerController() == this)
	{
		return;
	}
	// Their controller: the one whose player state it is.
	ACheeseStrikePlayerController* Target = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACheeseStrikePlayerController* Each = Cast<ACheeseStrikePlayerController>(It->Get()); Each != nullptr && Each != this && Each->PlayerState == Who)
		{
			Target = Each;
		}
	}
	if (ACheeseStrikeGameMode* Mode = GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>(); Mode != nullptr && Target != nullptr)
	{
		Mode->KickPlayer(Target);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeKick(const FString& Name)
{
	if (!IsLocalController() || !HasAuthority())
	{
		return;
	}
	const UWorld* World = GetWorld();
	for (TActorIterator<ACheeseStrikePlayerState> It(World); It; ++It)
	{
		if (It->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase) && It->GetPlayerController() != this)
		{
			ServerKick(*It);
			return;
		}
	}
}

void ACheeseStrikePlayerController::KickFromMenu(ACheeseStrikePlayerState* Who)
{
	ServerKick(Who);
}

void ACheeseStrikePlayerController::QuitFromMenu()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ACheeseStrikePlayerController::TogglePause()
{
	bPaused = !bPaused;
	// Paused, the pause menu shows first; resumed, nothing of it is left to come back to.
	if (ChosenPage == EMenuPage::Options)
	{
		ChosenPage = EMenuPage::Main;
	}
}

void ACheeseStrikePlayerController::MenuPressed()
{
	// While hosting or joining, nothing: the level it leads to is on its way.
	const UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>() : nullptr;
	if (Sessions != nullptr && Sessions->GetPending() != ESessionPending::None)
	{
		return;
	}
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	// Marking where a Cheese Strike goes, Escape calls it off rather than pausing.
	if (ACheeseStrikePlayerState* Mine = GetPlayerState<ACheeseStrikePlayerState>(); Mine != nullptr && Mine->IsMarkingStrike() && !bPaused)
	{
		Mine->CancelStrike();
		return;
	}
	if (State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded())
	{
		// From the settings back a page, still paused; otherwise pause or resume.
		if (bPaused && ChosenPage == EMenuPage::Options)
		{
			BackFromMenu();
		}
		else
		{
			TogglePause();
		}
	}
	else if (ChosenPage != EMenuPage::Main)
	{
		BackFromMenu();
	}
}

void ACheeseStrikePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalPlayerController())
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}
	// The game's own bindings, made in code.
	const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(this);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input != nullptr)
	{
		Subsystem->AddMappingContext(Input->Context, 0);
		if (EnhancedInput != nullptr)
		{
			EnhancedInput->BindAction(Input->Scoreboard, ETriggerEvent::Started, this, &ACheeseStrikePlayerController::ScoreboardStarted);
			EnhancedInput->BindAction(Input->Scoreboard, ETriggerEvent::Completed, this, &ACheeseStrikePlayerController::ScoreboardEnded);
			EnhancedInput->BindAction(Input->Menu, ETriggerEvent::Started, this, &ACheeseStrikePlayerController::MenuPressed);
		}
	}
	else
	{
		UE_LOG(LogCheeseStrike, Error, TEXT("'%s' has no input set to put on."), *GetNameSafe(this));
	}
	// And any a Blueprint of this controller adds, as the template's do.
	for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
	{
		Subsystem->AddMappingContext(CurrentContext, 0);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeHost(const FString& Options)
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Host(Options, false);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeHostLan(const FString& Options)
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Host(Options, true);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeFind()
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Find(false);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeFindLan()
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Find(true);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeJoin(int32 Number)
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Join(Number - 1);
	}
}

void ACheeseStrikePlayerController::CheeseStrikeLeave()
{
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Leave();
	}
}

bool ACheeseStrikePlayerController::TakeStartAt()
{
	const bool bFirst = !bStartAtTaken;
	bStartAtTaken = true;
	return bFirst;
}

void ACheeseStrikePlayerController::ScoreboardStarted()
{
	bScoreboardHeld = true;
}

void ACheeseStrikePlayerController::ScoreboardEnded()
{
	bScoreboardHeld = false;
}
