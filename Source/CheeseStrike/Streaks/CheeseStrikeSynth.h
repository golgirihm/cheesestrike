// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

/**
 * Sounds that have no recording yet, made in code: a falling bomb's
 * whistle and an air-raid siren, a jet's fly-by, heavy explosions, a hit's
 * thud, a kill's ka-ching and a taken weapon's clack. Each is a few seconds of
 * 16-bit mono at SampleRate, played by a sound wave that ends with it
 * (UCheeseStrikeSynthWave), as often as it is played. Recorded, licensed
 * sounds may replace them, as the design's "Audio" says.
 */
namespace CheeseStrikeSynth
{
	constexpr int32 SampleRate = 22050;

	/** A falling bomb's whistle: a tone gliding down from high to low over `Seconds`, growing louder as it comes. */
	TArray<int16> Whistle(float Seconds);
	/** An air-raid siren, rising and falling, for `Seconds`. */
	TArray<int16> Siren(float Seconds);
	/** A hit's thud, low and heavy like a smack on a pillow with weight behind it: about 110 ms of low-passed noise with a body of 80-150 Hz under it, no tone, peaking near -3 dBFS, `Weight` times as loud (clamped under full scale). */
	TArray<int16> Thump(float Weight);
	/** A short clack of gun metal, a weapon taken from a stand or the floor: 70 ms of a noise burst with a sharp attack through resonances near 1.2, 2.4 and 3.6 kHz. */
	TArray<int16> MetalClack();
	/** A cash register's "ka-ching" for a kill (about 0.7 s: the drawer's clack, its bell struck twice, coins), or for an assist the bell alone, softer. */
	TArray<int16> KaChing(bool bAssist);
	/** A jet passing low overhead in `Seconds`: a roar swelling to the fly-by and dropping away as it leaves, with a whine and a rumble under it. */
	TArray<int16> JetPass(float Seconds);
	/** A heavy explosion lasting `Seconds`: the crack, a falling boom, a rumble dying away and falling debris. `Seed` varies it. */
	TArray<int16> Explosion(float Seconds, int32 Seed);
	/** A soft hiss for `Seconds` (the Smoked Gouda's cloud): band-limited noise, in over a third of a second, wavering gently, and out over the last fifth. Peaks at a quarter of full scale. */
	TArray<int16> Hiss(float Seconds);
	/**
	 * The UI theme sampler's sounds (UI/CheeseStrikeThemes.h, temporary), a hover and a click a theme, each a few tenths of
	 * a second at most: the Cheese Board's soft wooden tick and wooden knock, the Deli Counter's paper tick and ticket
	 * punch, Cave Aged's glass ping and low stone tap, Cartoon Cheese's pop and boing, Tactical's mechanical click and radio
	 * blip. `Theme` is the theme's index (CheeseStrikeThemes::NumThemes); out of range, the first's.
	 */
	TArray<int16> ThemeHover(int32 Theme);
	TArray<int16> ThemeClick(int32 Theme);
	/** A sound, owned by `Outer`, that plays `Samples` each time it is played, each playing ending with them (UCheeseStrikeSynthWave). */
	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples);
}
