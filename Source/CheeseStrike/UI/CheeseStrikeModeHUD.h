// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;

/**
 * What the modes and the streaks put over the view, drawn into the HUD's
 * canvas each frame in the HUD's look (ACheeseStrikeHUD calls Draw; the HUD
 * itself shows a team mode's scores, round and time in its score summary,
 * the teammates' names over their heads, and the Unmanned Aerial Cheese's
 * sweeps on its minimap): the last few announcements (a streak called in, a
 * round won), and the Overload's warning to those it falls on; the streak
 * ready to call in, and word when one is earned; while marking a Cheese
 * Strike, where its wheels will land; during an Unmanned Aerial Cheese,
 * where its last sweep saw each enemy, and its time under the minimap; and,
 * out of a round, that the player waits for the next. A state a mode puts a
 * player in (a last stand) says so the way being out does (a notice in the
 * middle of the screen, ModeHudNotice).
 */
namespace CheeseStrikeModeHUD
{
	void Draw(AHUD* Hud, UCanvas* Canvas);
	/** Whether the local player of `Hud` is out of the round, when the HUD's own death notice (with its time until coming back) is not for them. */
	bool IsOut(const AHUD* Hud);
}
