// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

class FAutomationTestBase;
class UWorld;

/** Checks of a map's weapons' stands (ACheeseStrikePickup), for the tests that play every map (CheeseStrike.Play.AfterTheMice). */
namespace PickupChecks
{
	/**
	 * Before the mice: four to eight stands, fair to both sides and the
	 * better at the riskier spots (CheeseStrikePickups::AreFair,
	 * BetterLieRiskier), each on its floor with room for a player over
	 * it, clear of every hill's spot, and a way to it from a side's start. After them (`bAfterTheMice`):
	 * each still has its floor and a way to it from a start, but for a stand the map strands on purpose
	 * (tagged `Stranded` by `kit.pickup(stranded=True)`: Gruyère Grotto's Roquefort Rifle in the Spine's arch, reached
	 * over bridges of cheese alone), which must have no way to it then.
	 */
	void CheckStands(FAutomationTestBase& Test, UWorld* World, bool bAfterTheMice);
}

#endif
