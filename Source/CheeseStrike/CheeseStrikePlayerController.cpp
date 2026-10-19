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
#include "Engine/GameInstance.h"
#include "Sessions/CheeseStrikeSessionSubsystem.h"

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
	if (PlayScript != nullptr && GetPawn() != nullptr)
	{
		PlayScript->Advance(DeltaSeconds);
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
