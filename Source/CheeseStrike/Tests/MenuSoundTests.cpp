// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "Misc/AutomationTest.h"
#include "Sound/SoundBase.h"
#include "Streaks/CheeseStrikeSynth.h"
#include "UI/CheeseStrikeMenu.h"

namespace MenuSoundTests
{
	float Peak(const TArray<int16>& Samples)
	{
		int32 Loudest = 0;
		for (const int16 Value : Samples)
		{
			Loudest = FMath::Max(Loudest, FMath::Abs(static_cast<int32>(Value)));
		}
		return Loudest / 32767.0f;
	}

	/**
	 * A sound's brightness: its RMS frequency in Hz (the square root of the energy of its slope over its own, by the sample
	 * rate over two pi), where its energy sits on the whole. White noise at this rate is about 5 kHz; the old hover, the
	 * mechanical click, about 3.1 kHz.
	 */
	float Brightness(const TArray<int16>& Samples)
	{
		double Energy = 0.0;
		double Slope = 0.0;
		for (int32 Index = 1; Index < Samples.Num(); ++Index)
		{
			const double Value = Samples[Index] / 32767.0;
			const double Step = Value - Samples[Index - 1] / 32767.0;
			Energy += Value * Value;
			Slope += Step * Step;
		}
		return Energy > 0.0 ? static_cast<float>(FMath::Sqrt(Slope / Energy) * CheeseStrikeSynth::SampleRate / (2.0 * PI)) : 0.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheMenuSoundsAreClearOverTheMusic, "CheeseStrike.Rules.TheMenuSoundsAreClearOverTheMusic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FTheMenuSoundsAreClearOverTheMusic::RunTest(const FString& Parameters)
{
	using namespace MenuSoundTests;
	// Wave 17: the hover was a 35 ms mechanical click at 0.4 of full scale, lost under the music at its usual 60% and a buzz
	// when the mouse swept across the buttons. Now a bright, short tick near full scale, a little under the press's tap.
	const TArray<int16> Hover = CheeseStrikeSynth::MenuHover();
	const TArray<int16> Click = CheeseStrikeSynth::MenuClick();
	const float HoverPeak = Peak(Hover);
	const float ClickPeak = Peak(Click);
	AddInfo(FString::Printf(TEXT("Hover: %d ms, peak %.2f (%.1f dBFS), brightness %.0f Hz; press: %d ms, peak %.2f (%.1f dBFS), brightness %.0f Hz"),
		Hover.Num() * 1000 / CheeseStrikeSynth::SampleRate, HoverPeak, 20.0f * FMath::LogX(10.0f, HoverPeak), Brightness(Hover),
		Click.Num() * 1000 / CheeseStrikeSynth::SampleRate, ClickPeak, 20.0f * FMath::LogX(10.0f, ClickPeak), Brightness(Click)));
	TestTrue(TEXT("the hover is a short tick, 25 ms at most"), Hover.Num() > CheeseStrikeSynth::SampleRate / 100 && Hover.Num() <= CheeseStrikeSynth::SampleRate * 25 / 1000);
	TestTrue(FString::Printf(TEXT("bright: its energy high up, well over the old click's 3.1 kHz (%.0f Hz)"), Brightness(Hover)), Brightness(Hover) > 4500.0f);
	TestTrue(TEXT("loud: near full scale (it was 0.4)"), HoverPeak >= 0.75f && HoverPeak < 1.0f);
	TestTrue(TEXT("a little under the press's tap"), HoverPeak < ClickPeak && ClickPeak - HoverPeak <= 0.15f && ClickPeak < 1.0f);
	TestTrue(TEXT("the press is a short sound too"), Click.Num() <= CheeseStrikeSynth::SampleRate / 10);
	// One voice for the hover, and none within HoverRetrigger of the last: a sweep across the buttons ticks, never buzzes.
	TestEqual(TEXT("one hover at a time"), CheeseStrikeMenu::HoverVoices, 1);
	TestTrue(TEXT("at most one every 40 to 50 ms"), CheeseStrikeMenu::HoverRetrigger >= 0.04f && CheeseStrikeMenu::HoverRetrigger <= 0.05f);
	const USoundBase* HoverSound = CheeseStrikeMenu::UISound(false);
	if (TestNotNull(TEXT("the hover's sound"), HoverSound))
	{
		TestTrue(TEXT("its own concurrency"), HoverSound->bOverrideConcurrency);
		TestEqual(TEXT("one voice"), HoverSound->ConcurrencyOverrides.GetMaxCount(), CheeseStrikeMenu::HoverVoices);
		TestTrue(TEXT("the newest cutting the last off"), HoverSound->ConcurrencyOverrides.ResolutionRule == EMaxConcurrentResolutionRule::StopOldest);
		TestEqual(TEXT("and none sooner than HoverRetrigger after the last"), HoverSound->ConcurrencyOverrides.RetriggerTime, CheeseStrikeMenu::HoverRetrigger);
	}
	const USoundBase* ClickSound = CheeseStrikeMenu::UISound(true);
	TestTrue(TEXT("the press plays as it always has, every one heard"), ClickSound != nullptr && !ClickSound->bOverrideConcurrency);
	return true;
}
