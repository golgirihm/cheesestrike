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
 * round won, the hill captured or moving), and the Overload's warning to
 * those it falls on; King of the Hill's marker and strip
 * (CheeseStrikeHillHUD); the streak
 * ready to call in, and word when one is earned (the run of kills is the
 * HUD's streak panel, and choosing a Cheese Strike's spot its strike map);
 * during an Unmanned Aerial Cheese,
 * where its last sweep saw each enemy, and its time under the minimap; and,
 * out of a round, that the player waits for the next. A state a mode puts a
 * player in (a last stand) says so the way being out does (a notice in the
 * middle of the screen, ModeHudNotice).
 */
namespace CheeseStrikeModeHUD
{
	void Draw(AHUD* Hud, UCanvas* Canvas);
	/**
	 * The marks on the view over things in the world: during an Unmanned Aerial Cheese, a red diamond over each enemy, where they are
	 * now, followed every frame (the minimap's marks are the ones kept a second at a time); one off the screen pinned to its edge the
	 * way they are (CheeseStrikeHUD::MarkInView). The HUD draws them first, before its panels, so that those cover them
	 * (CheeseStrikeHUD::ShowsMarksInView).
	 */
	void DrawMarksInView(AHUD* Hud, UCanvas* Canvas);
	/** Whether the local player of `Hud` is out of the round, when the HUD's own death notice (with its time until coming back) is not for them. */
	bool IsOut(const AHUD* Hud);
}
