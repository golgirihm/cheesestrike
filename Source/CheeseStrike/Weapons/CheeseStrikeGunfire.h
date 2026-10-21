// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"

class USoundAttenuation;
class USoundConcurrency;

/**
 * How gunfire carries: everyone's shots but the listener's own are heard
 * from where they were fired, at full volume close by, fading with
 * distance to nothing, and duller the further off they are (the design's
 * "sounds further off are duller"); and only so many play at once, the
 * furthest giving way. The listener's own shots are heard as from their own
 * hands, always whole.
 *
 * The weapons' sounds are Lyra's sound waves, which carry no attenuation of
 * their own: before this every shot anywhere was heard at full volume.
 */
namespace CheeseStrikeGunfire
{
	/** Full volume within this (cm), fading to silence FalloffDistance beyond it. */
	constexpr float FullWithin = 400.0f;
	constexpr float FalloffDistance = 6000.0f;
	/** How far down (dB) a shot is at the end of the falloff, just before it is gone. */
	constexpr float AttenuationAtEnd = -40.0f;
	/** Duller from this far off (cm), dullest at DullestAt, where nothing above DullestHertz is left. */
	constexpr float DullFrom = 1200.0f;
	constexpr float DullestAt = 5000.0f;
	constexpr float DullestHertz = 1800.0f;
	constexpr float BrightHertz = 20000.0f;
	/** The most shots heard at once: past this, the furthest stops for the new one. */
	constexpr int32 MostAtOnce = 10;

	/** The settings, as the engine takes them. */
	FSoundAttenuationSettings Attenuation();
	void SetConcurrency(FSoundConcurrencySettings& Settings);

	/** The share of its volume a shot `Distance` cm away is heard at, by the engine's own evaluation of Attenuation(). */
	float Volume(float Distance);
	/** The frequency (Hz) above which a shot `Distance` away is cut, as the engine maps it (log frequency). */
	float LowPass(float Distance);

	/** The settings as assets for playing sounds by, made once and kept for the game's life. */
	USoundAttenuation* AttenuationAsset();
	USoundConcurrency* ConcurrencyAsset();
}
