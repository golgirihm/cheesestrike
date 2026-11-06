// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Virtual shadow maps are held off for the game's first moments. On an NVIDIA driver (610.88, an RTX 3070 Ti) the
 * GPU now and then faulted in their passes on the first frames a run drew, at a different garbage address each time
 * (Nanite's culling for them on the front end's first frame, twice; their projection while lighting, once, a map
 * loaded at the start of a run): DXGI_ERROR_DEVICE_HUNG, the game gone. Never later in a run. Players' drivers will
 * be older than ours, so the game starts with them off (`r.Shadow.Virtual.Enable` 0, set by code as the module
 * starts, the game only: the editor draws them from its start) and turns them on as the project wants them
 * (`WantedFromConfig`) once the run has drawn `Frames` frames over `Seconds`, under the loading cover; the log says
 * "Shadows: virtual shadow maps on after ..." (FCheeseStrikeModule).
 */
namespace CheeseStrikeShadows
{
	/** The run has to have drawn this many frames... */
	constexpr int32 Frames = 90;
	/** ...over at least this many seconds, before virtual shadow maps come on. */
	constexpr float Seconds = 3.0f;

	/** Whether virtual shadow maps may come on `FramesDrawn` frames and `SecondsSince` seconds into the run. */
	bool ShouldTurnOn(int32 FramesDrawn, float SecondsSince);

	/** Whether the project (and a player's own Engine.ini) wants them: the renderer's settings, `[SystemSettings]` over them; on if neither says. */
	bool WantedFromConfig();
}
