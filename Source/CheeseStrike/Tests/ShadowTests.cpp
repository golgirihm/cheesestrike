// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// Virtual shadow maps held off for a run's first moments (Rules/CheeseStrikeShadows.h).

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeShadows.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualShadowsWaitForTheFirstFrames, "CheeseStrike.Rules.VirtualShadowsWaitForTheFirstFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVirtualShadowsWaitForTheFirstFrames::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeShadows;
	TestFalse(TEXT("off on the first frame, when the GPU faulted"), ShouldTurnOn(1, 0.02f));
	TestFalse(TEXT("off for a few frames however slowly they come (a run's start is seconds a frame)"), ShouldTurnOn(3, 12.0f));
	TestFalse(TEXT("off for a few seconds however fast the frames come"), ShouldTurnOn(400, 1.0f));
	TestTrue(TEXT("on once both have passed"), ShouldTurnOn(Frames, Seconds));
	TestTrue(TEXT("the project wants them"), WantedFromConfig());
	return true;
}

#endif
