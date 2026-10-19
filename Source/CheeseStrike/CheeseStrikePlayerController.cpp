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

ACheeseStrikePlayerController::ACheeseStrikePlayerController()
{
	PlayerCameraManagerClass = ACheeseStrikeCameraManager::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ACheeseStrikePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (PlayScript == nullptr)
	{
		PlayScript = UCheeseStrikePlayScript::FromCommandLine(this);
	}
}

UCheeseStrikePlayScript* ACheeseStrikePlayerController::Play(const FString& Text)
{
	UCheeseStrikePlayScript* Next = UCheeseStrikePlayScript::FromText(this, Text);
	Next->TakeOver(PlayScript);
	PlayScript = Next;
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
