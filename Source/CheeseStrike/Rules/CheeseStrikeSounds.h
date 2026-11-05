// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What a player hears of being hurt, taking a weapon and scoring, as plain
 * rules (tested in CheeseStrike.Feedback.*):
 *
 * - Hurt, the one hurt hears a recorded grunt (ten of them: OpenGameArt's
 *   "Hurt Sound Effects" by EZduzziteh, CC0, and "11 male human pain/death
 *   sounds" by Michel Baradari, CC BY 3.0, imported by tools/art/wave14_sounds.py
 *   and wave15_sounds.py), plainly over the gunfire, once for every ten health
 *   lost since the last and at most one for a burst of hits, never the same one
 *   twice running; the one who hit hears a thud (CheeseStrikeSynth::Thump),
 *   which is nothing like it. A hero's grunts are in the hero's own voice
 *   (VoiceOf): Lt. Belica's ten a woman's (OpenGameArt's "Female Hurt Grunts &
 *   Groans" by Nocturnal_Vanguard and "RPG Voice Starter Pack" by cicifyre,
 *   both CC0, made by tools/art/wave16_sounds.py), the men's the ten above; a
 *   bot's, by the hero it wears, heard from where it is hurt.
 * - Taking a weapon from a stand or the floor sounds a real clack where it is
 *   taken, one syllable of a reload (PickupClackPath: a bolt-action rifle's
 *   click-clack from LFA's "equipment clicks III", CC0, with a low knock under
 *   it); drawing a weapon in hand keeps Lyra's weapon swap
 *   (CheeseStrikeWeaponLooks::DrawSounds).
 * - A kill sounds a cash register's ka-ching on the killer's machine with the
 *   kill pop, an assist a softer ching (CheeseStrikeSynth::KaChing).
 */
namespace CheeseStrikeSounds
{
	/** Whose voice a hero's grunts are in. */
	enum class EVoice : uint8
	{
		Male,
		Female,
	};
	/** The voice of the hero named `Hero` (CheeseStrikeBodies' names): a woman's for Lt. Belica, a man's for the rest and for a body unknown. */
	EVoice VoiceOf(const FString& Hero);
	/** The hurt grunts, ten in each voice, and the path of number `Variant` (any number) in `Voice`. */
	constexpr int32 Oofs = 10;
	FString OofPath(int32 Variant, EVoice Voice = EVoice::Male);
	/** A weapon taken from a stand or the floor: one syllable of a reload. */
	constexpr const TCHAR* PickupClackPath = TEXT("/Game/OpenGameArt/Clicks/SW_PickupClack.SW_PickupClack");
	/** How loud a grunt is: plainly heard over the shots (which CheeseStrikeGunfire::Loudness keeps down). */
	constexpr float OofVolume = 0.9f;
	/** At most one grunt this often (seconds): a burst of hits is one. */
	constexpr float OofEvery = 0.35f;
	/** Health lost since the last grunt that says another: a hit that takes this much is always heard; smaller ones add up to it. */
	constexpr float OofFrom = 10.0f;
	/** Whether losing `Lost` health since the last grunt, `SinceLast` seconds after it, says another. */
	bool ShouldOof(float Lost, float SinceLast);
	/** The grunt to say after `Last` (none: -1), given `Random` (any number): never the same one twice running. */
	int32 NextOof(int32 Last, int32 Random);

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
