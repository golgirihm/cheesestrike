// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CheeseStrikeBotController.generated.h"

/**
 * A bot: a player state with a cheese name and a side, a character like
 * any player's, and for now nothing to do, so it stands where it comes in
 * and takes what comes. Moving, shooting and skill come with the bots
 * proper (docs/unreal/design.md).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeBotController : public AAIController
{
	GENERATED_BODY()

public:
	ACheeseStrikeBotController();
};
