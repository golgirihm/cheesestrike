// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSynth.h"
#include "Sound/SoundWaveProcedural.h"

namespace CheeseStrikeSynth
{
	TArray<int16> Whistle(float Seconds)
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate));
		TArray<int16> Samples;
		Samples.SetNumUninitialized(Count);
		double Phase = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Along = static_cast<float>(Index) / Count;
			// From 1900 Hz down to 650, faster toward the end, with a slight waver.
			const float Frequency = FMath::Lerp(1900.0f, 650.0f, FMath::Pow(Along, 1.4f)) * (1.0f + 0.012f * FMath::Sin(2.0f * PI * 6.0f * Index / SampleRate));
			Phase += 2.0 * PI * Frequency / SampleRate;
			// In over a tenth of a second, louder as it nears, cut at the end.
			const float Envelope = FMath::Min(1.0f, Along * Seconds / 0.1f) * FMath::Lerp(0.25f, 1.0f, Along) * FMath::Min(1.0f, (1.0f - Along) * Seconds / 0.02f);
			const float Value = (FMath::Sin(Phase) + 0.15f * FMath::Sin(2.0 * Phase)) * Envelope * 0.45f;
			Samples[Index] = static_cast<int16>(FMath::Clamp(Value, -1.0f, 1.0f) * 32767.0f);
		}
		return Samples;
	}

	TArray<int16> Siren(float Seconds)
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate));
		TArray<int16> Samples;
		Samples.SetNumUninitialized(Count);
		double Phase = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			// Up and down between 420 and 880 Hz every two seconds, like a hand-cranked siren.
			const float Wail = 0.5f - 0.5f * FMath::Cos(2.0f * PI * Time / 2.0f);
			const float Frequency = FMath::Lerp(420.0f, 880.0f, Wail);
			Phase += 2.0 * PI * Frequency / SampleRate;
			const float Envelope = FMath::Min(1.0f, Time / 0.3f) * FMath::Min(1.0f, (Seconds - Time) / 0.4f);
			const float Value = (FMath::Sin(Phase) + 0.45f * FMath::Sin(2.0 * Phase) + 0.2f * FMath::Sin(3.0 * Phase)) * Envelope * 0.32f;
			Samples[Index] = static_cast<int16>(FMath::Clamp(Value, -1.0f, 1.0f) * 32767.0f);
		}
		return Samples;
	}

	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(Outer);
		Wave->SetSampleRate(SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = static_cast<float>(Samples.Num()) / SampleRate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		return Wave;
	}
}
