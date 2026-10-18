// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "CheeseStrikeCameraManager.h"
#include "CheeseStrike.h"
#include "Input/CheeseStrikeInput.h"

ACheeseStrikePlayerController::ACheeseStrikePlayerController()
{
	PlayerCameraManagerClass = ACheeseStrikeCameraManager::StaticClass();
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
	if (const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(this))
	{
		Subsystem->AddMappingContext(Input->Context, 0);
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
