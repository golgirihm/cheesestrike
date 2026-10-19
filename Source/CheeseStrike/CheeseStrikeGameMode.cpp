// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeGameMode.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerController.h"
#include "UI/CheeseStrikeHUD.h"

ACheeseStrikeGameMode::ACheeseStrikeGameMode()
{
	DefaultPawnClass = ACheeseStrikeCharacter::StaticClass();
	PlayerControllerClass = ACheeseStrikePlayerController::StaticClass();
	HUDClass = ACheeseStrikeHUD::StaticClass();
}
