// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USoundWaveProcedural;

/**
 * Sounds that have no recording yet, made in code: a falling bomb's
 * whistle and an air-raid siren, a jet's fly-by, heavy explosions, a hit's
 * thud and a kill's ka-ching. Each is a few seconds of
 * 16-bit mono at SampleRate, played by a sound wave that ends with it
 * (UCheeseStrikeSynthWave), as often as it is played. Recorded, licensed
 * sounds may replace them, as the design's "Audio" says.
 */
namespace CheeseStrikeSynth
{
	constexpr int32 SampleRate = 22050;

	/** A falling bomb's whistle: a tone gliding down from high to low over `Seconds`, growing louder as it comes. */
	TArray<int16> Whistle(float Seconds);
	/**
	 * An air-raid siren heard across a town (the Gouda Overload's warning, wave 16: once a cartoon's warble): one slow wail
	 * over `Seconds`, rising as its rotors spin up, held and winding down (SirenPitch), two tones a minor third apart with
	 * their harmonics, the motor's low hum of 50 and 100 Hz under it, dulled by the distance, with an echo off far walls and
	 * a reverberant tail that runs SirenTail seconds past the wail; peaking at SirenPeak of full scale.
	 */
	TArray<int16> Siren(float Seconds);
	/** The siren's lower tone (Hz) `Time` seconds into a wail of `Seconds`: from SirenLowest of SirenTopHertz up to it over SirenRise of the wail, held for SirenHold, down to SirenWoundDown of it at the end; no warble. */
	float SirenPitch(float Time, float Seconds);
	constexpr float SirenTopHertz = 470.0f;
	constexpr float SirenLowest = 0.18f;
	constexpr float SirenWoundDown = 0.35f;
	constexpr float SirenRise = 0.55f;
	constexpr float SirenHold = 0.12f;
	constexpr float SirenTail = 1.5f;
	constexpr float SirenPeak = 0.7f;
	/** A hit's thud, low and heavy like a smack on a pillow with weight behind it: about 110 ms of low-passed noise with a body of 80-150 Hz under it, no tone, peaking near -3 dBFS, `Weight` times as loud (clamped under full scale). */
	TArray<int16> Thump(float Weight);
	/** A cash register's "ka-ching" for a kill (about 0.7 s: the drawer's clack, its bell struck twice, coins), or for an assist the bell alone, softer. */
	TArray<int16> KaChing(bool bAssist);
	/** A jet passing low overhead in `Seconds`: a roar swelling to the fly-by and dropping away as it leaves, with a whine and a rumble under it. */
	TArray<int16> JetPass(float Seconds);
	/** A heavy explosion lasting `Seconds`: the crack, a falling boom, a rumble dying away and falling debris. `Seed` varies it. */
	TArray<int16> Explosion(float Seconds, int32 Seed);
	/** A soft hiss for `Seconds` (the Smoked Gouda's cloud): band-limited noise, in over a third of a second, wavering gently, and out over the last fifth. Peaks at a quarter of full scale. */
	TArray<int16> Hiss(float Seconds);
	/**
	 * The menus' sounds: under the mouse a bright, short tick (about 20 ms, its energy over 3 kHz), and on a press a tap
	 * on stone (80 ms, Cave Aged's). Their peaks (MenuHoverPeak, MenuClickPeak) are near full scale, the hover's a little
	 * under the press's, so that both are heard plainly over the music at its usual 60% (wave 17; the hover was 0.4 and
	 * the press 0.6 of full scale).
	 */
	constexpr float MenuHoverPeak = 0.8f;
	constexpr float MenuClickPeak = 0.9f;
	TArray<int16> MenuHover();
	TArray<int16> MenuClick();
	/** A sound, owned by `Outer`, that plays `Samples` each time it is played, each playing ending with them (UCheeseStrikeSynthWave). */
	USoundWaveProcedural* MakeSound(UObject* Outer, const TArray<int16>& Samples);
}
