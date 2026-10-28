// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

/**
 * Sounds the streaks make that have no recording yet, made in code: a
 * falling bomb's whistle and an air-raid siren, a jet's fly-by and
 * heavy explosions and footsteps. Each is a few seconds of
 * 16-bit mono at SampleRate, played once by a procedural sound wave made
 * for that one playing. Recorded, licensed sounds may replace them, as
 * the design's "Audio" says.
 */
namespace CheeseStrikeSynth
{
	constexpr int32 SampleRate = 22050;

	/** A falling bomb's whistle: a tone gliding down from high to low over `Seconds`, growing louder as it comes. */
	TArray<int16> Whistle(float Seconds);
	/** An air-raid siren, rising and falling, for `Seconds`. */
	TArray<int16> Siren(float Seconds);
	/** A grunt when hurt: a voice-like sound of `Seconds`, its pitch times `Pitch`, with a breath of noise. */
	TArray<int16> Grunt(float Seconds, float Pitch);
	/** A soft thump like a smack on a pillow: about 60 ms of low-passed noise with a low body, `Weight` times as heavy. */
	TArray<int16> Thump(float Weight);
	/** A footstep on a hard floor: about 140 ms, a heel's dull thud with a scuff. `Variant` (any number) picks one of a few that differ a little. */
	TArray<int16> Footstep(int32 Variant);
	/** A jet passing low overhead in `Seconds`: a roar swelling to the fly-by and dropping away as it leaves, with a whine and a rumble under it. */
	TArray<int16> JetPass(float Seconds);
	/** A heavy explosion lasting `Seconds`: the crack, a falling boom, a rumble dying away and falling debris. `Seed` varies it. */
	TArray<int16> Explosion(float Seconds, int32 Seed);
	/** A sound, owned by `Outer`, that plays `Samples` once. */
	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples);
}
