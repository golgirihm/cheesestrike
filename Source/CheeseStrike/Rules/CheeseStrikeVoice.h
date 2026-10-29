// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeStreaks.h"

/**
 * The voice: a calm, serious man on the command net (Piper's "norman" voice,
 * public domain, through a radio's filter: tools/art/voice_lines.py, which
 * keeps the lines' table), heard by each player for what concerns them, as
 * plain rules (tested in CheeseStrike.Voice.*):
 *
 * - a streak called in, by their side ("UAC online", "Cheese Strike inbound",
 *   "Gouda Overload incoming") or the enemy's ("Enemy UAC online", ...);
 * - the vermin coming ("Vermin inbound"), as the HUD says they have;
 * - a teammate's kill ("Enemy down"), in a mode of sides, now and then;
 * - a round won or lost, the match won or lost ("Victory", "Defeat");
 * - a teammate within earshot reloading ("Reloading!", heard from them, never
 *   by the one reloading, at most once every few seconds each).
 */
namespace CheeseStrikeVoice
{
	enum class ELine : uint8
	{
		None,
		UacOnline,
		UacEnemy,
		StrikeInbound,
		StrikeEnemy,
		OverloadIncoming,
		OverloadEnemy,
		VerminInbound,
		Reloading,
		EnemyDown,
		RoundWon,
		RoundLost,
		Victory,
		Defeat,
	};

	/** The sound of a line (in /Game/CheeseStrike/Voice), empty for None. */
	FString PathOf(ELine Line);
	/** How loud a line is heard. */
	constexpr float Volume = 1.0f;

	/** A streak of `Kind` called in, by the listener's side (or the listener) or not. */
	ELine ForStreak(CheeseStrikeStreaks::EStreak Kind, bool bOurs);
	/** A round won by side `Winner` (NoTeam for a draw: nothing said), the listener on side `Ours`. */
	ELine ForRound(int32 Winner, int32 Ours);
	/** The match over: won by the listener (their side, or they themself alone) or not; a draw says nothing. */
	ELine ForMatch(bool bDraw, bool bWeWon);
	/** Whether the vermin's coming is said now: they came `SinceCame` seconds ago (negative: not yet), a moment ago at most, and it hasn't been said. */
	bool SaysVermin(float SinceCame, bool bSaid);
	/** "Enemy down" at most this often (seconds), and whether a kill says it: by a teammate (not the listener, whose own kill rings the till), of an enemy, in a mode of sides. */
	constexpr float EnemyDownEvery = 5.0f;
	bool SaysEnemyDown(bool bTeams, bool bByMe, bool bByTeammate, bool bOfEnemy, float SinceLast);

	/** A player calls out a reload at most this often (seconds), and teammates hear it within this (cm). */
	constexpr float ReloadEvery = 6.0f;
	constexpr float ReloadEarshot = 1800.0f;
	/** Server: whether a reload begun `SinceLast` seconds after its player's last call-out is called out. */
	bool CallsReload(float SinceLast);
	/** On a listener's machine: whether a reload called out is heard: by a teammate, not the reloader themself, within earshot. */
	bool HearsReload(bool bReloaderIsMe, bool bTeammate, float Distance);
}
