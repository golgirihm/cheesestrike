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
#include "Rules/CheeseStrikeRules.h"
#include "Settings/CheeseStrikeSettings.h"
#include "UI/CheeseStrikeTones.h"
#include "Weapons/CheeseStrikeWeapon.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"
#include "Sessions/CheeseStrikeSessions.h"
#include "UI/CheeseStrikeMenu.h"
#include "UI/CheeseStrikeMenuFigure.h"

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
	}
}

void ACheeseStrikePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The menu and the menu camera belong to this level: a travel leaves none of them behind over the next.
	ShowMenu(EMenuPage::None);
	HideMenuCamera();
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
	// The menu follows where the match is. A joiner told to follow the host to another level is still in the match,
	// though the connection to the level being left is closed while the next is reached: the lobby says where to.
	const AGameStateBase* State = GetWorld()->GetGameState();
	const bool bStarted = State != nullptr && State->HasMatchStarted();
	const bool bEnded = State != nullptr && State->HasMatchEnded();
	const AGameState* Match = Cast<AGameState>(State);
	const bool bTravelling = (Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap) || bLeaving;
	const EMenuPage Wanted = CheeseStrikeMenu::PageFor(bStarted, bEnded, IsInAMatch() || bLeaving, bPaused, ChosenPage, bTravelling);
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
	static const TCHAR* const PageNames[] = {TEXT("none"), TEXT("main"), TEXT("find"), TEXT("lobby"), TEXT("pause"), TEXT("credits"), TEXT("options")};
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
		Sessions->Host(FString::Printf(TEXT("?KillLimit=%d?RespawnDelay=%.1f?Map=0"), CheeseStrikeRules::KillLimit, CheeseStrikeRules::RespawnDelay), Sessions->ServiceName() != TEXT("STEAM"));
	}
}

void ACheeseStrikePlayerController::SetRulesFromMenu(int32 KillLimit, float RespawnDelay, int32 Map)
{
	ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr;
	if (Mode == nullptr)
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

void ACheeseStrikePlayerController::SetBotsFromMenu(int32 Wanted)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
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
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->SetBotSkill(Skill);
	}
	if (Menu.IsValid())
	{
		Menu->Refresh();
	}
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
	if (UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>())
	{
		Sessions->Find(Sessions->ServiceName() != TEXT("STEAM"));
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
	else if (Page.Equals(TEXT("Options"), ESearchCase::IgnoreCase))
	{
		Chosen = EMenuPage::Options;
	}
	ChooseMenu(Chosen);
	static const TCHAR* const Tabs[] = {TEXT("Controls"), TEXT("View"), TEXT("Sound"), TEXT("Graphics")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs) && Menu.IsValid(); ++Index)
	{
		if (Tab.Equals(Tabs[Index], ESearchCase::IgnoreCase))
		{
			Menu->SetOptionsTab(static_cast<EOptionsTab>(Index));
		}
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

void ACheeseStrikePlayerController::LeaveFromMenu()
{
	bPaused = false;
	ChosenPage = EMenuPage::Main;
	UCheeseStrikeSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCheeseStrikeSessionSubsystem>();
	if (Sessions != nullptr && Sessions->IsInAMatch())
	{
		Sessions->Leave();
	}
	else
	{
		ClientTravel(CheeseStrikeSessions::Level, TRAVEL_Absolute);
	}
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
	if (State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded())
	{
		// From the options back to the pause menu, still paused; otherwise pause or resume.
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
