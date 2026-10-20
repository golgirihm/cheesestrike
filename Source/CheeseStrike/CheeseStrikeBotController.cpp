// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBotController.h"
#include "CheeseStrikePlayerState.h"

ACheeseStrikeBotController::ACheeseStrikeBotController()
{
	// A bot is on the scoreboard and in the lobby like anyone else.
	bWantsPlayerState = true;
}
