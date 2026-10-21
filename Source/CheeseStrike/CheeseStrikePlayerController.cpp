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
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CheeseStrikeBotController.h"
#include "Map/CheeseStrikeCheese.h"
#include "Map/CheeseStrikeMouse.h"
#include "CheeseStrikeCharacter.h"
#include "EngineUtils.h"
#include "CheeseStrikeGameMode.h"
#include "Rules/CheeseStrikeRules.h"
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
	// The menu follows where the match is.
	const AGameStateBase* State = GetWorld()->GetGameState();
	const bool bStarted = State != nullptr && State->HasMatchStarted();
	const bool bEnded = State != nullptr && State->HasMatchEnded();
	const AGameState* Match = Cast<AGameState>(State);
	const bool bTravelling = Match != nullptr && Match->GetMatchState() == MatchState::LeavingMap;
	const EMenuPage Wanted = CheeseStrikeMenu::PageFor(bStarted, bEnded, IsInAMatch(), bPaused, ChosenPage, bTravelling);
	if (Wanted != ShownPage)
	{
		ShowMenu(Wanted);
	}
	else if (Menu.IsValid() && (Wanted == EMenuPage::Lobby || Wanted == EMenuPage::Find))
	{
		// These pages show things that change.
		SinceMenuRefresh += DeltaSeconds;
		if (SinceMenuRefresh >= CheeseStrikeMenu::RefreshEvery)
		{
			SinceMenuRefresh = 0.0f;
			Menu->Refresh();
		}
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

bool ACheeseStrikePlayerController::IsInAMatch() const
{
	return GetWorld() != nullptr && GetWorld()->GetNetMode() != NM_Standalone;
}

void ACheeseStrikePlayerController::ShowMenu(EMenuPage Page)
{
	ShownPage = Page;
	SinceMenuRefresh = 0.0f;
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

void ACheeseStrikePlayerController::CheeseStrikePage(const FString& Page)
{
	ChooseMenu(Page.Equals(TEXT("Credits"), ESearchCase::IgnoreCase) ? EMenuPage::Credits : (Page.Equals(TEXT("Find"), ESearchCase::IgnoreCase) ? EMenuPage::Find : EMenuPage::Main));
}

void ACheeseStrikePlayerController::CheeseStrikeStart(const FString& AtOnce)
{
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->AskToStart(!AtOnce.IsEmpty());
	}
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
}

void ACheeseStrikePlayerController::MenuPressed()
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	if (State != nullptr && State->HasMatchStarted() && !State->HasMatchEnded())
	{
		TogglePause();
	}
	else if (ChosenPage != EMenuPage::Main)
	{
		ChosenPage = EMenuPage::Main;
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
