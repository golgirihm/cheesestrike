// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSynth.h"
#include "Sound/SoundWaveProcedural.h"
#include "CheeseStrikeSynthWave.h"

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
		// Wave 14: a pillow thud and nothing else, unlike the hurt grunt: noise through two low-pass poles at 260 Hz (so no
		// pitch to it), a soft start and a quick fall. Wave 15: much louder (its peak near -3 dBFS, where it was -5 and
		// played at 0.6) and heavier, 110 ms with a body of noise through two poles at 120 Hz under it, which rings on
		// a little longer in the 80-150 Hz band.
		const int32 Count = FMath::RoundToInt(0.11f * SampleRate);
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Seed = 987654u;
		const float Pole = PoleFor(260.0f);
		const float BodyPole = PoleFor(120.0f);
		const float BodyHigh = PoleFor(70.0f);
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		float D = 0.0f;
		float Under = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			const float White = Noise(Seed);
			A += (White - A) * Pole;
			B += (A - B) * Pole;
			C += (White - C) * BodyPole;
			D += (C - D) * BodyPole;
			// The body's rumble below 70 Hz taken off, so that it is a thud and not a hum.
			Under += (D - Under) * BodyHigh;
			const float Body = (D - Under) * 4.0f;
			const float End = FMath::Min(1.0f, (Count - Index) / (0.015f * SampleRate));
			const float Slap = B * FMath::Min(1.0f, Time / 0.006f) * FMath::Exp(-Time / 0.022f);
			const float Weighty = Body * FMath::Min(1.0f, Time / 0.008f) * FMath::Exp(-Time / 0.04f);
			Values[Index] = (Slap + Weighty) * End;
		}
		return Normalised(Values, FMath::Clamp(0.71f * Weight, 0.05f, 0.95f));
	}

	TArray<int16> MetalClack()
	{
		// Gun metal knocked: a burst of white noise in at once and over in a few hundredths, through three resonant
		// band-passes (two-pole, narrow) near 1.2, 2.4 and 3.6 kHz that ring a little past the burst, with a click of the
		// bare burst at the front; 70 ms in all.
		const int32 Count = FMath::RoundToInt(0.07f * SampleRate);
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Seed = 2468u;
		const float Hertz[] = {1200.0f, 2400.0f, 3600.0f};
		const float Shares[] = {1.0f, 0.7f, 0.45f};
		// A resonator each: y = x + r*2cos(w)*y1 - r*r*y2, the ring set by r.
		float Y1[3] = {0.0f, 0.0f, 0.0f};
		float Y2[3] = {0.0f, 0.0f, 0.0f};
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			const float Burst = Noise(Seed) * FMath::Min(1.0f, Time / 0.0004f) * FMath::Exp(-Time / 0.009f);
			float Value = Burst * 0.25f;
			for (int32 Band = 0; Band < 3; ++Band)
			{
				const float Ring = 0.985f;
				const float Cosine = FMath::Cos(2.0f * PI * Hertz[Band] / SampleRate);
				const float Y = Burst * (1.0f - Ring) + 2.0f * Ring * Cosine * Y1[Band] - Ring * Ring * Y2[Band];
				Y2[Band] = Y1[Band];
				Y1[Band] = Y;
				Value += Y * Shares[Band] * 3.0f;
			}
			Values[Index] = Value * FMath::Min(1.0f, (Count - Index) / (0.012f * SampleRate));
		}
		return Normalised(Values, 0.8f);
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

	TArray<int16> Hiss(float Seconds)
	{
		// Smoke escaping: white noise through a band (two low-pass poles at 2.6 kHz over a high-pass at 500 Hz), so that it
		// is neither a rumble nor a whistle, swelling in and wavering a little as it goes, gone over the last fifth.
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate));
		TArray<float> Values;
		Values.SetNumUninitialized(Count);
		uint32 Seed = 4242u;
		const float Low = PoleFor(2600.0f);
		const float High = PoleFor(500.0f);
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Time = static_cast<float>(Index) / SampleRate;
			const float White = Noise(Seed);
			A += (White - A) * Low;
			B += (A - B) * Low;
			C += (B - C) * High;
			const float Band = B - C;
			const float Waver = 1.0f + 0.12f * FMath::Sin(2.0f * PI * 0.7f * Time) + 0.06f * FMath::Sin(2.0f * PI * 1.9f * Time + 1.0f);
			const float Envelope = FMath::Min(1.0f, Time / 0.35f) * FMath::Min(1.0f, (Seconds - Time) / (Seconds * 0.2f));
			Values[Index] = Band * Waver * Envelope;
		}
		return Normalised(Values, 0.25f);
	}

	namespace
	{
		/** A two-pole resonator at `Hertz` ringing by `Ring` (0.9 dull to 0.999 long), fed `In` a sample at a time. */
		struct FResonator
		{
			float Cosine;
			float Ring;
			float Y1 = 0.0f;
			float Y2 = 0.0f;
			FResonator(float Hertz, float InRing) : Cosine(FMath::Cos(2.0f * PI * Hertz / SampleRate)), Ring(InRing) {}
			float Feed(float In)
			{
				const float Y = In * (1.0f - Ring) + 2.0f * Ring * Cosine * Y1 - Ring * Ring * Y2;
				Y2 = Y1;
				Y1 = Y;
				return Y;
			}
		};

		/** `Seconds` of silence to fill in. */
		TArray<float> Blank(float Seconds)
		{
			TArray<float> Values;
			Values.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * SampleRate)));
			return Values;
		}

		/** A short fade at the very end, so that nothing clicks as it stops. */
		void FadeOut(TArray<float>& Values, float Seconds)
		{
			const int32 Count = Values.Num();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Values[Index] *= FMath::Min(1.0f, (Count - Index) / (Seconds * SampleRate));
			}
		}

		// The Cheese Board: a fingertip on a wooden board (two short wood resonances, 30 ms), and a knuckle's knock on it
		// (deeper resonances with a soft slap, 90 ms).
		TArray<int16> WoodTick()
		{
			TArray<float> Values = Blank(0.03f);
			uint32 Seed = 1101u;
			FResonator High(1900.0f, 0.975f);
			FResonator Mid(950.0f, 0.98f);
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float Burst = Noise(Seed) * FMath::Exp(-Time / 0.0012f);
				Values[Index] = High.Feed(Burst) * 1.0f + Mid.Feed(Burst) * 0.7f;
			}
			FadeOut(Values, 0.006f);
			return Normalised(Values, 0.3f);
		}

		TArray<int16> WoodKnock()
		{
			TArray<float> Values = Blank(0.09f);
			uint32 Seed = 1102u;
			FResonator Low(190.0f, 0.994f);
			FResonator Body(420.0f, 0.99f);
			FResonator Top(1300.0f, 0.96f);
			const float SlapPole = PoleFor(700.0f);
			float A = 0.0f;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float White = Noise(Seed);
				const float Burst = White * FMath::Exp(-Time / 0.003f);
				A += (White - A) * SlapPole;
				const float Slap = A * 2.0f * FMath::Min(1.0f, Time / 0.001f) * FMath::Exp(-Time / 0.008f);
				Values[Index] = Low.Feed(Burst) * 1.0f + Body.Feed(Burst) * 0.6f + Top.Feed(Burst) * 0.25f + Slap;
			}
			FadeOut(Values, 0.012f);
			return Normalised(Values, 0.6f);
		}

		// The Deli Counter: a slip of paper flicked (a breath of high noise, 22 ms), and a ticket punched (a click, then
		// the punch's chunk 18 ms after it, 100 ms).
		TArray<int16> PaperTick()
		{
			TArray<float> Values = Blank(0.022f);
			uint32 Seed = 2201u;
			const float Pole = PoleFor(3000.0f);
			float Low = 0.0f;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float White = Noise(Seed);
				Low += (White - Low) * Pole;
				Values[Index] = (White - Low) * FMath::Min(1.0f, Time / 0.0008f) * FMath::Exp(-Time / 0.006f);
			}
			FadeOut(Values, 0.004f);
			return Normalised(Values, 0.25f);
		}

		TArray<int16> TicketPunch()
		{
			TArray<float> Values = Blank(0.1f);
			uint32 Seed = 2202u;
			const float ChunkPole = PoleFor(900.0f);
			float A = 0.0f;
			float B = 0.0f;
			FResonator Lever(2600.0f, 0.95f);
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float White = Noise(Seed);
				// The lever's click at the start.
				const float Click = Lever.Feed(White * FMath::Exp(-Time / 0.0015f)) * 1.2f;
				// The punch through the card, 18 ms on: low noise and a thud at 140 Hz.
				const float Since = Time - 0.018f;
				float Chunk = 0.0f;
				if (Since >= 0.0f)
				{
					A += (White - A) * ChunkPole;
					B += (A - B) * ChunkPole;
					Chunk = B * 3.0f * FMath::Min(1.0f, Since / 0.001f) * FMath::Exp(-Since / 0.02f) + FMath::Sin(2.0f * PI * 140.0f * Since) * 0.6f * FMath::Exp(-Since / 0.03f);
				}
				Values[Index] = Click + Chunk;
			}
			FadeOut(Values, 0.012f);
			return Normalised(Values, 0.65f);
		}

		// Cave Aged: a fingernail on a glass (three inharmonic partials ringing 320 ms), and a tap on stone (a damped low
		// knock with a little grit, 80 ms).
		TArray<int16> GlassPing()
		{
			TArray<float> Values = Blank(0.32f);
			const float Partials[] = {2600.0f, 5420.0f, 8150.0f};
			const float Shares[] = {1.0f, 0.4f, 0.15f};
			const float Rings[] = {0.09f, 0.05f, 0.03f};
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				float Value = 0.0f;
				for (int32 Partial = 0; Partial < UE_ARRAY_COUNT(Partials); ++Partial)
				{
					Value += FMath::Sin(2.0f * PI * Partials[Partial] * Time) * Shares[Partial] * FMath::Exp(-Time / Rings[Partial]);
				}
				Values[Index] = Value * FMath::Min(1.0f, Time / 0.001f);
			}
			FadeOut(Values, 0.03f);
			return Normalised(Values, 0.3f);
		}

		TArray<int16> StoneTap()
		{
			TArray<float> Values = Blank(0.08f);
			uint32 Seed = 3302u;
			const float GritPole = PoleFor(500.0f);
			float A = 0.0f;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float White = Noise(Seed);
				A += (White - A) * GritPole;
				const float Knock = FMath::Sin(2.0f * PI * (160.0f + 60.0f * FMath::Exp(-Time / 0.01f)) * Time) * FMath::Min(1.0f, Time / 0.0015f) * FMath::Exp(-Time / 0.025f);
				const float Grit = A * 2.5f * FMath::Exp(-Time / 0.01f);
				Values[Index] = Knock + Grit;
			}
			FadeOut(Values, 0.01f);
			return Normalised(Values, 0.6f);
		}

		// Cartoon Cheese: a pop (a tone dropping from 700 to 180 Hz in a few hundredths with a click at the front, 70 ms),
		// and a boing (a spring's pitch bouncing down and settling, 350 ms).
		TArray<int16> Pop()
		{
			TArray<float> Values = Blank(0.07f);
			uint32 Seed = 4401u;
			double Phase = 0.0;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float Frequency = 180.0f + 520.0f * FMath::Exp(-Time / 0.012f);
				Phase += 2.0 * PI * Frequency / SampleRate;
				const float Tone = static_cast<float>(FMath::Sin(Phase)) * FMath::Exp(-Time / 0.025f);
				const float Click = Noise(Seed) * 0.5f * FMath::Exp(-Time / 0.0008f);
				Values[Index] = Tone * FMath::Min(1.0f, Time / 0.0005f) + Click;
			}
			FadeOut(Values, 0.01f);
			return Normalised(Values, 0.45f);
		}

		TArray<int16> Boing()
		{
			TArray<float> Values = Blank(0.35f);
			double Phase = 0.0;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				// The spring: a pitch that falls fast, then wobbles about 240 Hz, the wobble dying away.
				const float Frequency = 240.0f + 320.0f * FMath::Exp(-Time / 0.06f) + 110.0f * FMath::Exp(-Time / 0.14f) * FMath::Sin(2.0f * PI * 11.0f * Time);
				Phase += 2.0 * PI * Frequency / SampleRate;
				const float Tone = static_cast<float>(FMath::Sin(Phase) + 0.35 * FMath::Sin(2.0 * Phase) + 0.12 * FMath::Sin(3.0 * Phase));
				Values[Index] = Tone * FMath::Min(1.0f, Time / 0.004f) * FMath::Exp(-Time / 0.11f);
			}
			FadeOut(Values, 0.03f);
			return Normalised(Values, 0.6f);
		}

		// Tactical: a switch's mechanical click (two ticks 9 ms apart through a metal resonance, 35 ms), and a radio's
		// blip (two short tones with a breath of static, 140 ms).
		TArray<int16> MechanicalClick()
		{
			TArray<float> Values = Blank(0.035f);
			uint32 Seed = 5501u;
			FResonator Metal(2800.0f, 0.955f);
			const float BodyPole = PoleFor(1200.0f);
			float A = 0.0f;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				const float White = Noise(Seed);
				const float Second = FMath::Max(0.0f, Time - 0.009f);
				const float Ticks = White * (FMath::Exp(-Time / 0.0009f) + (Time >= 0.009f ? 0.8f * FMath::Exp(-Second / 0.0012f) : 0.0f));
				A += (Ticks - A) * BodyPole;
				Values[Index] = Metal.Feed(Ticks) * 1.5f + A * 1.2f;
			}
			FadeOut(Values, 0.005f);
			return Normalised(Values, 0.4f);
		}

		TArray<int16> RadioBlip()
		{
			TArray<float> Values = Blank(0.14f);
			uint32 Seed = 5502u;
			double Phase = 0.0;
			const float Low = PoleFor(3000.0f);
			const float High = PoleFor(1500.0f);
			float A = 0.0f;
			float B = 0.0f;
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				const float Time = static_cast<float>(Index) / SampleRate;
				// Two tones, 45 ms each, gated with a 4 ms edge.
				const float Frequency = Time < 0.045f ? 1150.0f : 1500.0f;
				Phase += 2.0 * PI * Frequency / SampleRate;
				const float InTone = Time < 0.09f ? FMath::Min(1.0f, Time / 0.004f) * FMath::Min(1.0f, (0.09f - Time) / 0.004f) * (Time < 0.045f ? FMath::Min(1.0f, (0.045f - Time) / 0.004f) : FMath::Min(1.0f, (Time - 0.045f) / 0.004f)) : 0.0f;
				const float Tone = static_cast<float>(FMath::Sin(Phase) + 0.3 * FMath::Sin(3.0 * Phase)) * InTone;
				// Static under it, through a band of 1.5 to 3 kHz, fading over the whole blip.
				const float White = Noise(Seed);
				A += (White - A) * Low;
				B += (A - B) * High;
				const float Static = (A - B) * 0.9f * FMath::Exp(-Time / 0.06f);
				Values[Index] = Tone + Static;
			}
			FadeOut(Values, 0.02f);
			return Normalised(Values, 0.55f);
		}
	}

	TArray<int16> ThemeHover(int32 Theme)
	{
		switch (Theme)
		{
		case 1: return PaperTick();
		case 2: return GlassPing();
		case 3: return Pop();
		case 4: return MechanicalClick();
		default: return WoodTick();
		}
	}

	TArray<int16> ThemeClick(int32 Theme)
	{
		switch (Theme)
		{
		case 1: return TicketPunch();
		case 2: return StoneTap();
		case 3: return Boing();
		case 4: return RadioBlip();
		default: return WoodKnock();
		}
	}

	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples)
	{
		// A wave whose every playing ends with its samples (UCheeseStrikeSynthWave), never one fed through the procedural
		// queue, which the mixer never sees end: each would hold a voice playing silence for as long as it lived.
		UCheeseStrikeSynthWave* Wave = NewObject<UCheeseStrikeSynthWave>(Outer);
		Wave->SetSamples(Samples, SampleRate);
		return Wave;
	}
}
