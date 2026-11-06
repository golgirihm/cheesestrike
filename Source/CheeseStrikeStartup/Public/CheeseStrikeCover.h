// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

class SWidget;

/**
 * The loading cover's looks, made of Slate alone (nothing of the game), so
 * that the engine's loading screen can draw it on its own thread while the
 * game's is busy: the lobby's dark tile, the big spinner (the cheese wedge
 * turning, a fifth of the screen's height) in the middle and a few words
 * under it. The game's loading cover (UCheeseStrikeCurtainSubsystem) and the
 * startup cover (this module's, shown from the moment the game's window is
 * made until the front end's level has loaded) are both this.
 */
namespace CheeseStrikeCover
{
	/**
	 * The cover saying `Saying`, drawn at the scale `Target` (the window's scale times the game's UI scale), or, with
	 * `bTargetFromSize`, at the scale the game's UI would have at the size of the window it is drawn in (the startup cover's,
	 * made before there is a window). `bCountFrames` counts the frames it is drawn in (FramesDrawn).
	 */
	CHEESESTRIKESTARTUP_API TSharedRef<SWidget> Make(const TAttribute<FText>& Saying, float Target, bool bCountFrames = false, bool bTargetFromSize = false);
	/** How many times a cover made with bCountFrames has been drawn since ResetFramesDrawn (on any thread). */
	CHEESESTRIKESTARTUP_API int32 FramesDrawn();
	CHEESESTRIKESTARTUP_API void ResetFramesDrawn();
	/** When the startup cover was given to the engine's loading screen (FPlatformTime::Seconds), or 0 if it never was (the editor, a server, no movie player). */
	CHEESESTRIKESTARTUP_API double StartupCoverSince();
	/** Notes the startup cover going up now (the startup module's). */
	CHEESESTRIKESTARTUP_API void NoteStartupCover();
}
