// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// What a hit feels like to the one hurt, as plain rules and sound: the grunt
// made in code is a sound of its length that is never silent nor clipped, and
// the red flash for a fall fades in and out over its time.

#include "Misc/AutomationTest.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "UI/CheeseStrikeHUDRules.h"

namespace CombatFeedbackTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheGruntIsASoundOfItsLengthAndNotSilent, "CheeseStrike.Feedback.Grunt", CombatFeedbackTests::Flags)

bool FTheGruntIsASoundOfItsLengthAndNotSilent::RunTest(const FString& Parameters)
{
	const TArray<int16> Low = CheeseStrikeSynth::Grunt(0.3f, 0.9f);
	const TArray<int16> High = CheeseStrikeSynth::Grunt(0.3f, 1.1f);
	TestEqual(TEXT("as long as it says"), Low.Num(), FMath::RoundToInt(0.3f * CheeseStrikeSynth::SampleRate));
	TestTrue(TEXT("and not silent"), High.ContainsByPredicate([](int16 Value) { return FMath::Abs(static_cast<int32>(Value)) > 2000; }));
	TestTrue(TEXT("and never clipped"), !High.ContainsByPredicate([](int16 Value) { return Value == 32767 || Value == -32768; }));
	TestTrue(TEXT("it starts and ends quietly"), FMath::Abs(static_cast<int32>(High[0])) < 1000 && FMath::Abs(static_cast<int32>(High.Last())) < 1000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheFallFlashFadesInAndOut, "CheeseStrike.Feedback.FallFlash", CombatFeedbackTests::Flags)

bool FTheFallFlashFadesInAndOut::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeHUD;
	TestEqual(TEXT("nothing before the fall"), FallFlashAlpha(-1.0f), 0.0f);
	TestEqual(TEXT("at its strongest as the fall lands"), FallFlashAlpha(0.0f), FallFlashPeak);
	TestTrue(TEXT("fading as it goes"), FallFlashAlpha(FallFlashTime * 0.5f) < FallFlashPeak && FallFlashAlpha(FallFlashTime * 0.5f) > 0.0f);
	TestEqual(TEXT("gone after its time"), FallFlashAlpha(FallFlashTime), 0.0f);
	return true;
}
