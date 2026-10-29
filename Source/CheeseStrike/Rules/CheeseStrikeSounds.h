// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What a player hears of being hurt, taking a weapon and scoring, as plain
 * rules (tested in CheeseStrike.Feedback.*):
 *
 * - Hurt, the one hurt hears a quiet breathy "oof" (OpenGameArt's "Hurt
 *   Sound Effects" by EZduzziteh, CC0, imported by tools/art/wave14_sounds.py),
 *   at most one for a burst of hits; the one who hit hears a muted thud
 *   (CheeseStrikeSynth::Thump), which is nothing like it.
 * - Taking a weapon from a stand or the floor sounds Lyra's weapon swap there.
 * - A kill sounds a cash register's ka-ching on the killer's machine with the
 *   kill pop, an assist a softer ching (CheeseStrikeSynth::KaChing).
 */
namespace CheeseStrikeSounds
{
	/** The hurt "oof"s, and the path of number `Variant` (any number). */
	constexpr int32 Oofs = 5;
	FString OofPath(int32 Variant);
	/** How loud an "oof" is: quiet, under the shots. */
	constexpr float OofVolume = 0.45f;
	/** At most one "oof" this often (seconds): a burst of hits is one. */
	constexpr float OofEvery = 0.35f;
	/** Whether losing `Lost` health, `SinceLast` seconds after the last "oof", says another. */
	bool ShouldOof(float Lost, float SinceLast);

	/** Lyra's weapon swaps, and the path of number `Variant` (any number). */
	constexpr int32 Pickups = 3;
	FString PickupPath(int32 Variant);

	/** The sound a change in the player's own kills and assists makes. */
	enum class EScore : uint8
	{
		None,
		Kill,
		Assist,
	};
	/** From the kills and assists heard before to those now: a kill rings over an assist; fewer (a new match) is nothing. */
	EScore ScoreSound(int32 KillsBefore, int32 Kills, int32 AssistsBefore, int32 Assists);
}
