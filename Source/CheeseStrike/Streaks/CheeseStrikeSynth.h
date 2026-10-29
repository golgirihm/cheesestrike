// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

/**
 * Sounds that have no recording yet, made in code: a falling bomb's
 * whistle and an air-raid siren, a jet's fly-by, heavy explosions, a hit's
 * thud and a kill's ka-ching. Each is a few seconds of
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
	/** A hit's thud, muted and low like a smack on a pillow: about 80 ms of low-passed noise, no tone, `Weight` times as loud. */
	TArray<int16> Thump(float Weight);
	/** A cash register's "ka-ching" for a kill (about 0.7 s: the drawer's clack, its bell struck twice, coins), or for an assist the bell alone, softer. */
	TArray<int16> KaChing(bool bAssist);
	/** A jet passing low overhead in `Seconds`: a roar swelling to the fly-by and dropping away as it leaves, with a whine and a rumble under it. */
	TArray<int16> JetPass(float Seconds);
	/** A heavy explosion lasting `Seconds`: the crack, a falling boom, a rumble dying away and falling debris. `Seed` varies it. */
	TArray<int16> Explosion(float Seconds, int32 Seed);
	/** A sound, owned by `Outer`, that plays `Samples` once. */
	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples);
}
