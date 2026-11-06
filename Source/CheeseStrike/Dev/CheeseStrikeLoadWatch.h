// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Watches the frame rate for a while after a level loads, and says in the log what the engine is busy with meanwhile, so
 * that a slow first half minute on a map can be told apart: pipeline states (PSOs) the renderer is still compiling for
 * the materials it meets for the first time in this run, shaders still compiling (in the editor's builds), the
 * navigation mesh being built (the maps keep none: map_kit.save_level), textures still streaming in. Once a second for
 * CheeseStrikeLoadWatch::Seconds (`CheeseStrike.LoadWatch` seconds, 0 for none):
 *
 *   Load watch: Lvl_Creamery 3 s: 27 fps (mean 37 ms, worst 92 ms); 140 PSOs compiling, navigation building (12 tiles), 3 textures streaming
 *
 * and at its end a line for the whole: frames, the mean, the worst, how many seconds were under 30 frames a second.
 */
namespace CheeseStrikeLoadWatch
{
	/** How long after a level loads it is watched, unless CheeseStrike.LoadWatch says otherwise (seconds). */
	constexpr float Seconds = 30.0f;
	/** A second slower than this is a slow one (frames a second). */
	constexpr float SlowFps = 30.0f;

	/**
	 * The longest the loading cover waits, after a level has loaded, for the renderer to finish compiling the pipeline
	 * states it precaches for the level's materials (seconds): those compiling, a first load runs at a few frames a
	 * second (a packaged build only: the editor's builds precache none), so the cover stays over it and the match
	 * waits for everyone's to be done (ACheeseStrikePlayerController::CanSayReady).
	 */
	constexpr float MostWarmingSeconds = 15.0f;

	/** Whether the view may be shown `SinceLoaded` seconds after a level loaded with `Psos` pipeline states compiling. */
	bool IsWarm(int32 Psos, float SinceLoaded);
	/** Whether the view may be shown now: the renderer's PSOs for the level last loaded compiled, or waited for long enough. */
	bool IsWarm();

	/** Starts watching `World` (the level just loaded): the log says how its frames go. Replaces any watch running. */
	void Start(UWorld* World);

	/** How many PSOs the renderer is compiling ahead of their first use, now (PipelineStateCache's precache requests). */
	int32 PsosCompiling();

	/** A second's line: `Frames` frame times (seconds) as the log writes it, and whether it was slow. */
	FString DescribeSecond(const TArray<float>& Frames, bool* bSlow = nullptr);
}
