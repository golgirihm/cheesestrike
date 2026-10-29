// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// What a hit feels like to the one hurt and to the one who hit, as plain
// rules and sound: the hurt "oof" is a recording, said once for a burst of
// hits; the hit's thud made in code is muted and low, never a voice; taking a
// weapon and scoring sound; and the red flash for a fall fades in and out.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeSounds.h"
#include "Sound/SoundWave.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "UI/CheeseStrikeHUDRules.h"

namespace CombatFeedbackTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHitIsAQuietOofAndAMutedThud, "CheeseStrike.Feedback.Hurt", CombatFeedbackTests::Flags)

bool FAHitIsAQuietOofAndAMutedThud::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSounds;
	// The one hurt: a quiet "oof", one for a burst of hits, for any damage.
	TestTrue(TEXT("a hit says oof"), ShouldOof(5.0f, 10.0f));
	TestFalse(TEXT("no damage, nothing"), ShouldOof(0.0f, 10.0f));
	TestFalse(TEXT("a burst of hits is one oof"), ShouldOof(20.0f, OofEvery * 0.5f));
	TestTrue(TEXT("then another"), ShouldOof(20.0f, OofEvery));
	TestTrue(TEXT("quiet, under the shots"), OofVolume < 0.6f);
	TSet<FString> OofPaths;
	for (int32 Variant = 0; Variant < Oofs; ++Variant)
	{
		const FString Path = OofPath(Variant);
		OofPaths.Add(Path);
		const USoundWave* Wave = LoadObject<USoundWave>(nullptr, *Path);
		if (TestNotNull(FString::Printf(TEXT("%s is there"), *Path), Wave))
		{
			TestTrue(FString::Printf(TEXT("and is short (%.2f s)"), Wave->Duration), Wave->Duration > 0.1f && Wave->Duration < 0.5f);
		}
	}
	TestEqual(TEXT("five of them"), OofPaths.Num(), 5);
	// The one who hit: a thud like a smack on a pillow, muted and low, with no tone in it: its sign changes seldom (a low
	// sound's few hundred times a second, where a voice's or a bright noise's are thousands), and it never clips.
	const TArray<int16> Thud = CheeseStrikeSynth::Thump(1.0f);
	int32 Crossings = 0;
	for (int32 Index = 1; Index < Thud.Num(); ++Index)
	{
		Crossings += (Thud[Index - 1] < 0) != (Thud[Index] < 0) ? 1 : 0;
	}
	const float PerSecond = Crossings / (static_cast<float>(Thud.Num()) / CheeseStrikeSynth::SampleRate);
	TestTrue(FString::Printf(TEXT("the thud is low (%.0f crossings a second)"), PerSecond), PerSecond < 900.0f);
	TestTrue(TEXT("and never clipped"), !Thud.ContainsByPredicate([](int16 Value) { return Value == 32767 || Value == -32768; }));
	TestTrue(TEXT("a kill's is heavier"), CheeseStrikeSynth::Thump(1.3f) != Thud);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTakingAWeaponAndScoringSound, "CheeseStrike.Feedback.PickupAndScore", CombatFeedbackTests::Flags)

bool FTakingAWeaponAndScoringSound::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSounds;
	for (int32 Variant = 0; Variant < Pickups; ++Variant)
	{
		TestNotNull(FString::Printf(TEXT("the weapon swap %s is there"), *PickupPath(Variant)), LoadObject<USoundBase>(nullptr, *PickupPath(Variant)));
	}
	TestTrue(TEXT("a kill rings the till"), ScoreSound(3, 4, 1, 1) == EScore::Kill);
	TestTrue(TEXT("a kill with an assist too rings it once"), ScoreSound(3, 4, 1, 2) == EScore::Kill);
	TestTrue(TEXT("an assist a softer ching"), ScoreSound(3, 3, 1, 2) == EScore::Assist);
	TestTrue(TEXT("nothing new, nothing"), ScoreSound(3, 3, 1, 1) == EScore::None);
	TestTrue(TEXT("fewer (a new match) is nothing"), ScoreSound(5, 0, 2, 0) == EScore::None);
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
