// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The rules of moving, as arithmetic: how fast in each state, when a slide
// can begin and how it slows, and how high a jump goes.

#include "Misc/AutomationTest.h"
#include "Movement/CheeseStrikeMovement.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MovementTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpeedsKeepTheirRatios, "CheeseStrike.Movement.SpeedsKeepTheirRatios", MovementTests::Flags)

bool FSpeedsKeepTheirRatios::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMovement;
	TestEqual(TEXT("walking"), Speed(false, false, false), WalkSpeed);
	TestEqual(TEXT("sprinting is half as fast again"), Speed(true, false, false), WalkSpeed * 1.5f);
	TestEqual(TEXT("crouched is half"), Speed(false, false, true), WalkSpeed * 0.5f);
	TestEqual(TEXT("and rules out sprinting"), Speed(true, false, true), WalkSpeed * 0.5f);
	TestEqual(TEXT("aiming is six tenths"), Speed(false, true, false), WalkSpeed * 0.6f);
	TestEqual(TEXT("and rules out sprinting too"), Speed(true, true, false), WalkSpeed * 0.6f);
	TestEqual(TEXT("crouched and aiming, both"), Speed(false, true, true), WalkSpeed * 0.3f);
	// A shooter's pace at life size: not the Godot game's 2 m/s scaled down.
	TestTrue(TEXT("a brisk walk"), WalkSpeed >= 400.0f && WalkSpeed <= 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASlideBeginsAtASprintAndEasesToACrouch, "CheeseStrike.Movement.ASlideBeginsAtASprintAndEasesToACrouch", MovementTests::Flags)

bool FASlideBeginsAtASprintAndEasesToACrouch::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMovement;
	TestFalse(TEXT("not from a walk"), CanSlide(WalkSpeed));
	TestFalse(TEXT("not from just over a walk"), CanSlide(WalkSpeed * 1.19f));
	TestTrue(TEXT("from a fifth over a walk"), CanSlide(WalkSpeed * 1.2f));
	TestTrue(TEXT("from a sprint"), CanSlide(Speed(true, false, false)));

	const float From = Speed(true, false, false);
	TestEqual(TEXT("a little faster at first"), SlideSpeed(From, 0.0f), From * SlideBoost);
	TestEqual(TEXT("a crouched walk at the end"), SlideSpeed(From, SlideTime), WalkSpeed * CrouchFactor);
	TestEqual(TEXT("and after it"), SlideSpeed(From, SlideTime * 2.0f), WalkSpeed * CrouchFactor);
	// Holding most of its speed at first, then dropping away: never speeding up.
	float Last = SlideSpeed(From, 0.0f);
	for (float Elapsed = 0.05f; Elapsed <= SlideTime; Elapsed += 0.05f)
	{
		const float Now = SlideSpeed(From, Elapsed);
		TestTrue(FString::Printf(TEXT("slowing at %.2f s"), Elapsed), Now <= Last + KINDA_SMALL_NUMBER);
		Last = Now;
	}
	TestTrue(TEXT("still going at a sprint halfway through"), SlideSpeed(From, SlideTime / 2.0f) > From * 0.8f);
	TestTrue(TEXT("a slide outlasts a step"), SlideTime > 0.5f && SlideTime < 1.5f);
	TestTrue(TEXT("and the rest after one is short"), SlideRest > 0.0f && SlideRest < SlideTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAJumpIsQuickAndAboutHalfAMetre, "CheeseStrike.Movement.AJumpIsQuickAndAboutHalfAMetre", MovementTests::Flags)

bool FAJumpIsQuickAndAboutHalfAMetre::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMovement;
	const float Height = JumpHeight();
	TestTrue(TEXT("over a knee-high block"), Height >= 45.0f);
	TestTrue(TEXT("but not over a player"), Height <= 70.0f);
	TestEqual(TEXT("as the formula has it"), JumpHeight(1000.0f, 1000.0f), 500.0f);
	TestTrue(TEXT("a step is stepped over"), StepHeight >= 15.0f && StepHeight < Height);
	TestTrue(TEXT("air steering is a share of ground acceleration"), AirControl() > 0.1f && AirControl() < 0.5f);
	TestTrue(TEXT("crouched, the capsule still holds the radius"), CrouchedHalfHeight >= CapsuleRadius && CrouchedHalfHeight < CapsuleHalfHeight * 0.6f);
	return true;
}

#endif
