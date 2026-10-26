// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Who gets the credit for a kill that others helped with, and for time on
 * King of the Hill, as plain functions (no world): an assist is a player who
 * hurt the victim shortly before they died, and the hill's time is given to
 * one of the players holding it at a time, in turn.
 */
namespace CheeseStrikeCredit
{
	/** A hit on a player: by whom (their player id), and when (server seconds). */
	struct FHit
	{
		int32 By = -1;
		float At = 0.0f;
	};

	/** How long before a death a hit still counts toward an assist. */
	constexpr float AssistWindow = 10.0f;

	/**
	 * The players who get an assist for `Victim`'s death at `Now`, by the
	 * killer `Killer` (-1 for nobody): each who hit the victim within
	 * AssistWindow, once each, never the killer nor the victim themself.
	 */
	TArray<int32> Assisters(TConstArrayView<FHit> Hits, int32 Killer, int32 Victim, float Now);

	/** How long each player holds the hill's time before it passes to the next of those holding it together. */
	constexpr float CreditRotation = 1.0f;

	/**
	 * Whose turn it is for the hill's time, with `Credited` seconds of time
	 * already given out, among `Count` players holding it together: 0 to
	 * Count - 1, in turn.
	 */
	int32 CreditIndex(float Credited, int32 Count);
}
