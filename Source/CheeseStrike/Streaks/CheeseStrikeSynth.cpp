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

	namespace
	{
		/** The next noise value, from -1 to 1, of a small fixed generator, so that a sound made twice is the same. */
		float Noise(uint32& Seed)
		{
			Seed = Seed * 1664525u + 1013904223u;
			return static_cast<float>(Seed >> 8) / 8388608.0f - 1.0f;
		}

		/** Scaled so that its loudest sample is `Peak`, then made 16-bit. */
		TArray<int16> Normalised(const TArray<float>& Values, float Peak)
		{
			float Loudest = 1e-6f;
			for (const float Value : Values)
			{
				Loudest = FMath::Max(Loudest, FMath::Abs(Value));
			}
			TArray<int16> Samples;
			Samples.SetNumUninitialized(Values.Num());
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				Samples[Index] = static_cast<int16>(FMath::Clamp(Values[Index] * Peak / Loudest, -1.0f, 1.0f) * 32767.0f);
			}
			return Samples;
		}

		/** The share of the gap a one-pole low-pass closes each sample, for a cutoff in Hz. */
		float PoleFor(float Hertz)
		{
			return 1.0f - FMath::Exp(-2.0f * PI * Hertz / SampleRate);
		}

		/** A string plucked at `Start` (seconds) for `Length` seconds, added to `Mix`: Karplus-Strong, a noise burst in a delay line that loses its highs as it rings. */
		void Pluck(TArray<float>& Mix, float Start, float Length, float Hertz, float Loudness, float Ring, uint32& Seed)
		{
			const int32 Delay = FMath::Max(2, FMath::RoundToInt(SampleRate / Hertz));
			TArray<float> Line;
			Line.SetNumUninitialized(Delay);
			float Low = 0.0f;
			for (int32 Index = 0; Index < Delay; ++Index)
			{
				// The pick: noise, softened.
				Low += (Noise(Seed) - Low) * 0.6f;
				Line[Index] = Low;
			}
			const int32 First = FMath::RoundToInt(Start * SampleRate);
			const int32 Count = FMath::RoundToInt(Length * SampleRate);
			int32 At = 0;
			for (int32 Index = 0; Index < Count && First + Index < Mix.Num(); ++Index)
			{
				const float Here = Line[At];
				const float Next = Line[(At + 1) % Delay];
				Line[At] = Ring * 0.5f * (Here + Next);
				At = (At + 1) % Delay;
				// A short fade at the note's end, so that a muted note stops cleanly.
				const float Tail = FMath::Min(1.0f, (Count - Index) / (0.01f * SampleRate));
				Mix[First + Index] += Here * Loudness * Tail;
			}
		}
	}

	TArray<int16> Thump(float Weight)
	{
		// Wave 14: a pillow thud and nothing else, unlike the hurt "oof": noise through two low-pass poles at 260 Hz
		// (so no pitch to it), a soft start and a quick fall, about 80 ms.
		const int32 Count = FMath::RoundToInt(0.08f * SampleRate);
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Seed = 987654u;
		const float Pole = PoleFor(260.0f);
		float A = 0.0f;
		float B = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			A += (Noise(Seed) - A) * Pole;
			B += (A - B) * Pole;
			const float Envelope = FMath::Min(1.0f, Time / 0.006f) * FMath::Exp(-Time / 0.022f) * FMath::Min(1.0f, (Count - Index) / (0.01f * SampleRate));
			Values[Index] = B * Envelope;
		}
		return Normalised(Values, FMath::Clamp(0.55f * Weight, 0.05f, 0.9f));
	}

	TArray<int16> KaChing(bool bAssist)
	{
		// A cash register: the "ka" of its drawer's catch (a short clack of mid noise and a low knock), then the "ching"
		// of its bell, struck twice in quick succession, inharmonic partials ringing down over half a second, with a
		// rattle of coins. An assist is the bell alone, once, higher and shorter.
		const float Seconds = bAssist ? 0.35f : 0.7f;
		const int32 Count = FMath::RoundToInt(Seconds * SampleRate);
		TArray<float> Values;
		Values.SetNumZeroed(Count);
		uint32 Seed = bAssist ? 31337u : 7331u;
		const float BellAt = bAssist ? 0.0f : 0.06f;
		const float Base = bAssist ? 2640.0f : 2093.0f;
		const float Partials[] = {1.0f, 2.32f, 4.25f, 6.63f};
		const float Shares[] = {1.0f, 0.55f, 0.3f, 0.18f};
		const float Rings[] = {0.32f, 0.18f, 0.1f, 0.06f};
		float A = 0.0f;
		float B = 0.0f;
		const float ClackHigh = PoleFor(3500.0f);
		const float ClackLow = PoleFor(900.0f);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			const float White = Noise(Seed);
			float Value = 0.0f;
			if (!bAssist)
			{
				// The clack: band-limited noise over in 15 ms, and a knock at 180 Hz.
				A += (White - A) * ClackHigh;
				B += (A - B) * ClackLow;
				Value += (A - B) * 2.2f * FMath::Exp(-Time / 0.006f);
				Value += FMath::Sin(2.0f * PI * 180.0f * Time) * 0.35f * FMath::Exp(-Time / 0.012f);
			}
			// The bell, struck at BellAt and again 45 ms later, a little softer.
			for (int32 Strike = 0; Strike < (bAssist ? 1 : 2); ++Strike)
			{
				const float Since = Time - BellAt - Strike * 0.045f;
				if (Since < 0.0f)
				{
					continue;
				}
				const float Struck = (Strike == 0 ? 1.0f : 0.7f) * FMath::Min(1.0f, Since / 0.0015f);
				for (int32 Partial = 0; Partial < UE_ARRAY_COUNT(Partials); ++Partial)
				{
					const float Ring = Rings[Partial] * (bAssist ? 0.6f : 1.0f);
					Value += FMath::Sin(2.0f * PI * Base * Partials[Partial] * Since) * Shares[Partial] * Struck * FMath::Exp(-Since / Ring) * 0.35f;
				}
			}
			// Coins: sparse ticks for a quarter of a second after the bell.
			if (!bAssist && Time > BellAt && Time < BellAt + 0.3f && FMath::Abs(Noise(Seed)) > 0.993f)
			{
				Value += Noise(Seed) * 0.4f * FMath::Exp(-(Time - BellAt) / 0.15f);
			}
			// Out over the last tenth.
			Values[Index] = Value * FMath::Min(1.0f, (Count - Index) / (0.1f * Count));
		}
		return Normalised(Values, bAssist ? 0.5f : 0.8f);
	}

	TArray<int16> JetPass(float Seconds)
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate));
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Seed = 8675309u;
		float A = 0.0f, B = 0.0f, RumbleA = 0.0f, RumbleB = 0.0f;
		double Whine = 0.0, Spool = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Along = static_cast<float>(Index) / Count;
			// Loud as it comes, loudest as it passes at 45%, falling away behind: a bell, not quite even.
			const float Near = FMath::Exp(-FMath::Square((Along - 0.45f) / (Along < 0.45f ? 0.2f : 0.28f)));
			const float White = Noise(Seed);
			// The roar: noise through two low-pass poles whose cutoff climbs with nearness, so that it brightens as the jet arrives.
			const float Pole = PoleFor(350.0f + 2300.0f * Near);
			A += (White - A) * Pole;
			B += (A - B) * Pole;
			const float Roar = B * (3.0f + 5.0f * Near);
			// The low rumble of the engines, heard all the way.
			RumbleA += (White - RumbleA) * PoleFor(140.0f);
			RumbleB += (RumbleA - RumbleB) * PoleFor(140.0f);
			const float Rumble = RumbleB * 6.0f;
			// The turbine's whine, its pitch dropping as the jet passes (the Doppler shift: higher coming, lower going), and the engines' low spool.
			const float Shift = FMath::Lerp(1.18f, 0.82f, FMath::Clamp((Along - 0.35f) / 0.2f, 0.0f, 1.0f));
			Whine += 2.0 * PI * 1250.0 * Shift / SampleRate;
			Spool += 2.0 * PI * 95.0 * Shift / SampleRate;
			const float Tone = 0.07f * FMath::Sin(Whine) + 0.05f * FMath::Sin(2.0 * Whine) + 0.12f * FMath::Sin(Spool);
			const float Fade = FMath::Min(1.0f, Along * Seconds / 0.3f) * FMath::Min(1.0f, (1.0f - Along) * Seconds / 0.5f);
			Values[Index] = (Roar + Tone * (0.4f + Near)) * (0.15f + 0.85f * Near) * Fade + Rumble * (0.2f + 0.5f * Near) * Fade;
		}
		return Normalised(Values, 0.85f);
	}

	TArray<int16> Explosion(float Seconds, int32 Seed)
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate));
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Random = 24680u + static_cast<uint32>(FMath::Abs(Seed)) * 104729u;
		float A = 0.0f, B = 0.0f;
		double Phase = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			const float White = Noise(Random);
			// The crack: a hard burst of noise, over in a few hundredths.
			const float Crack = White * FMath::Exp(-Time / 0.025f);
			// The boom: a low tone falling from 70 Hz to 28, heavy and slow to die.
			Phase += 2.0 * PI * (28.0f + 42.0f * FMath::Exp(-Time / 0.25f)) / SampleRate;
			const float Boom = static_cast<float>(FMath::Sin(Phase)) * FMath::Min(1.0f, Time / 0.006f) * FMath::Exp(-Time / 0.7f) * 1.3f;
			// The rumble: noise whose cutoff falls from 700 Hz to 70 as the blast rolls away.
			const float Pole = PoleFor(FMath::Lerp(700.0f, 70.0f, FMath::Min(1.0f, Time / (Seconds * 0.7f))));
			A += (White - A) * Pole;
			B += (A - B) * Pole;
			const float Roll = B * 7.0f * FMath::Min(1.0f, Time / 0.02f) * FMath::Exp(-Time / 1.1f);
			// Debris falling after: sparse clicks, fewer as time goes.
			float Debris = 0.0f;
			if (Time > 0.15f && FMath::Abs(Noise(Random)) > 0.9985f - 0.001f * FMath::Exp(-(Time - 0.15f) / 0.8f))
			{
				Debris = Noise(Random) * 0.5f * FMath::Exp(-Time / 1.2f);
			}
			Values[Index] = FMath::Tanh((Crack * 0.9f + Boom + Roll) * 1.4f) + Debris;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Out over the last fifth.
			Values[Index] *= FMath::Min(1.0f, static_cast<float>(Count - Index) / (Count * 0.2f));
		}
		return Normalised(Values, 0.95f);
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
