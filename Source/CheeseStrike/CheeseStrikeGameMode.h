// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CheeseStrikeGameMode.generated.h"

/**
 * The game's rules, so far: every player is a CheeseStrikeCharacter driven
 * by a CheeseStrikePlayerController. The modes and scoring come later.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACheeseStrikeGameMode();
};
