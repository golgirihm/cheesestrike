// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// What a hit feels like to the one hurt and to the one who hit, as plain
// rules and sound: the hurt grunt is one of ten recordings, said for every
// ten health lost and once for a burst of hits, never the same twice running;
// the hit's thud made in code is heavy and low, never a voice; taking a
// weapon clacks and scoring rings; and the red flash for a fall fades in and
// out.

#include "Misc/AutomationTest.h"
#include "Rules/CheeseStrikeSounds.h"
#include "Sound/SoundWave.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "UI/CheeseStrikeHUDRules.h"
#include "Weapons/CheeseStrikeGunfire.h"

namespace CombatFeedbackTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Sign changes a second: a low sound's are a few hundred, a voice's or a bright noise's thousands. */
	float CrossingsPerSecond(const TArray<int16>& Samples)
	{
		int32 Crossings = 0;
		for (int32 Index = 1; Index < Samples.Num(); ++Index)
		{
			Crossings += (Samples[Index - 1] < 0) != (Samples[Index] < 0) ? 1 : 0;
		}
		return Crossings / (static_cast<float>(Samples.Num()) / CheeseStrikeSynth::SampleRate);
	}

	int32 Loudest(const TArray<int16>& Samples)
	{
		int32 Peak = 0;
		for (const int16 Sample : Samples)
		{
			Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Sample)));
		}
		return Peak;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHitIsAGruntAndAHeavyThud, "CheeseStrike.Feedback.Hurt", CombatFeedbackTests::Flags)

bool FAHitIsAGruntAndAHeavyThud::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSounds;
	// The one hurt: a grunt for every ten health lost since the last, smaller hits adding up, one for a burst of hits.
	TestTrue(TEXT("a hit of ten says it"), ShouldOof(10.0f, 10.0f));
	TestTrue(TEXT("a big hit says it"), ShouldOof(75.0f, 10.0f));
	TestFalse(TEXT("no damage, nothing"), ShouldOof(0.0f, 10.0f));
	TestFalse(TEXT("a shard of five alone, nothing yet"), ShouldOof(5.0f, 10.0f));
	TestTrue(TEXT("two shards add up"), ShouldOof(10.0f, 10.0f));
	TestFalse(TEXT("a burst of hits is one grunt"), ShouldOof(20.0f, OofEvery * 0.5f));
	TestTrue(TEXT("then another"), ShouldOof(20.0f, OofEvery));
	TestTrue(TEXT("plainly over the shots"), OofVolume >= 0.85f && OofVolume > CheeseStrikeGunfire::Loudness);
	// Ten of them, never the same twice running, every other one possible.
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
	TestEqual(TEXT("ten of them"), OofPaths.Num(), 10);
	for (int32 Last = 0; Last < Oofs; ++Last)
	{
		TSet<int32> Next;
		for (int32 Random = 0; Random < 100; ++Random)
		{
			const int32 Pick = NextOof(Last, Random);
			TestTrue(FString::Printf(TEXT("after %d, %d is another"), Last, Pick), Pick != Last && Pick >= 0 && Pick < Oofs);
			Next.Add(Pick);
		}
		TestEqual(FString::Printf(TEXT("after %d every other one comes"), Last), Next.Num(), Oofs - 1);
	}
	TestTrue(TEXT("the first is any"), NextOof(-1, 7) >= 0 && NextOof(-1, 7) < Oofs);
	// The one who hit: a thud like a smack on a pillow, heavy and low, with no tone in it: its sign changes seldom (a low
	// sound's few hundred times a second, where a voice's or a bright noise's are thousands), it peaks near -3 dBFS
	// (so that it is heard over the gunfire) and never clips.
	const TArray<int16> Thud = CheeseStrikeSynth::Thump(1.0f);
	const float PerSecond = CombatFeedbackTests::CrossingsPerSecond(Thud);
	TestTrue(FString::Printf(TEXT("the thud is low (%.0f crossings a second)"), PerSecond), PerSecond < 900.0f);
	const int32 Peak = CombatFeedbackTests::Loudest(Thud);
	TestTrue(FString::Printf(TEXT("and loud, near -3 dBFS (%d)"), Peak), Peak >= 22000 && Peak <= 24500);
	TestTrue(TEXT("and never clipped"), !Thud.ContainsByPredicate([](int16 Value) { return Value == 32767 || Value == -32768; }));
	TestTrue(TEXT("a kill's is heavier"), CheeseStrikeSynth::Thump(1.3f) != Thud);
	TestTrue(TEXT("a heavy one never clips either"), !CheeseStrikeSynth::Thump(1.8f).ContainsByPredicate([](int16 Value) { return Value == 32767 || Value == -32768; }));
	// With a body under it: more of its energy in the 80-150 Hz band than the wave 14 pillow thud had (half; its centroid
	// 161 Hz), measured bin by bin over its whole spectrum.
	double Low = 0.0;
	double All = 0.0;
	double Weighted = 0.0;
	for (int32 Bin = 0; Bin <= Thud.Num() / 2; ++Bin)
	{
		double Real = 0.0;
		double Imaginary = 0.0;
		for (int32 Index = 0; Index < Thud.Num(); ++Index)
		{
			const double Angle = 2.0 * PI * Bin * Index / Thud.Num();
			Real += Thud[Index] * FMath::Cos(Angle);
			Imaginary += Thud[Index] * FMath::Sin(Angle);
		}
		const double Power = Real * Real + Imaginary * Imaginary;
		const double Hertz = static_cast<double>(Bin) * CheeseStrikeSynth::SampleRate / Thud.Num();
		All += Power;
		Weighted += Power * Hertz;
		Low += Hertz >= 80.0 && Hertz <= 150.0 ? Power : 0.0;
	}
	TestTrue(FString::Printf(TEXT("with low body (%.2f of its energy at 80-150 Hz)"), Low / All), Low / All > 0.55);
	TestTrue(FString::Printf(TEXT("and a low centre (%.0f Hz)"), Weighted / All), Weighted / All < 150.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTakingAWeaponAndScoringSound, "CheeseStrike.Feedback.PickupAndScore", CombatFeedbackTests::Flags)

bool FTakingAWeaponAndScoringSound::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeSounds;
	// Taking a weapon: a short clack of gun metal, bright (its sign changes thousands of times a second), in at once, over
	// in 70 ms, never clipped.
	const TArray<int16> Clack = CheeseStrikeSynth::MetalClack();
	TestTrue(FString::Printf(TEXT("the clack is 60 to 80 ms (%d samples)"), Clack.Num()), Clack.Num() >= CheeseStrikeSynth::SampleRate * 6 / 100 && Clack.Num() <= CheeseStrikeSynth::SampleRate * 8 / 100);
	const float PerSecond = CombatFeedbackTests::CrossingsPerSecond(Clack);
	TestTrue(FString::Printf(TEXT("and metallic, bright (%.0f crossings a second)"), PerSecond), PerSecond > 2000.0f && PerSecond < 8000.0f);
	int32 LoudestAt = 0;
	for (int32 Index = 0; Index < Clack.Num(); ++Index)
	{
		LoudestAt = FMath::Abs(static_cast<int32>(Clack[Index])) > FMath::Abs(static_cast<int32>(Clack[LoudestAt])) ? Index : LoudestAt;
	}
	TestTrue(FString::Printf(TEXT("with a sharp attack (loudest at %.1f ms)"), LoudestAt * 1000.0f / CheeseStrikeSynth::SampleRate), LoudestAt < CheeseStrikeSynth::SampleRate * 5 / 1000);
	TestTrue(TEXT("loud enough"), CombatFeedbackTests::Loudest(Clack) > 20000);
	TestTrue(TEXT("and never clipped"), !Clack.ContainsByPredicate([](int16 Value) { return Value == 32767 || Value == -32768; }));
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
