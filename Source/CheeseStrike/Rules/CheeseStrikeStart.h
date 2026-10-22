// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * How a match and each of its rounds begin, as plain functions over plain
 * values: the count of seconds before anyone may move or fire (3, 2, 1,
 * FIGHT), and the protection of a fresh spawn, which lasts a few seconds
 * from the count's end or until its player attacks, and shows on every
 * machine as a ghostly look that fades as it ends. Times are the server's
 * (AGameStateBase::GetServerWorldTimeSeconds). ACheeseStrikeGameMode sets
 * when the count ends (ACheeseStrikeGameState::GetGoAt), the character
 * applies the rest.
 */
namespace CheeseStrikeStart
{
	/** Seconds counted before a match: everyone is in, and held where they stand until it ends. */
	constexpr float Countdown = 3.0f;
	/** Seconds counted before each later round of a mode played in rounds (Elimination, Execution): shorter, since a break came before it. */
	constexpr float RoundCountdown = 2.0f;
	/** The longest count a URL may ask for. */
	constexpr float MostCountdown = 10.0f;
	/** Seconds the word to go stays up once the count is over. */
	constexpr float GoShown = 0.8f;
	/** Seconds the ghostly look takes to fade once the protection ends. */
	constexpr float GhostFade = 0.5f;

	/** The count before a match with these URL `Options`: `?Countdown=` seconds (0 for none, as the play tests and tools/play.ps1 have it), else Countdown. */
	float CountdownFrom(const FString& Options);
	/** The count before a later round, in a match whose own count is `MatchCountdown`: RoundCountdown, never longer (none when the match has none). */
	float RoundCountdownFor(float MatchCountdown);

	/** Whether everyone is held: the count ends at `GoAt` (0 for no count) and it is `Now`. */
	bool IsFrozen(float Now, float GoAt);
	/** Seconds left on a round's clock that ends at `EndsAt`: it stands still while the count before the round runs. */
	float RoundLeft(float Now, float GoAt, float EndsAt);

	/** Until when a character that came in at `SpawnedAt` is protected: SpawnProtection seconds, counted from the end of a count still running. */
	float ProtectedUntil(float SpawnedAt, float GoAt);
	/** Its protection after it attacks at `Now` (fires, strikes with the knife, calls in a streak): over at once, if it wasn't already. */
	float AfterAttack(float Until, float Now);
	/** Whether a character protected until `Until` (0 for never) is protected `Now`. */
	bool IsProtected(float Now, float Until);
	/** How ghostly a living character protected until `Until` looks `Now`: 1 while protected, fading to 0 over GhostFade seconds after. */
	float GhostAmount(float Now, float Until);

	/** What the middle of the screen shows of the count: the second ("3", "2", "1") or the word to go, how large (1 as it settles), and how opaque. */
	struct FShown
	{
		FString Text;
		float Scale = 1.0f;
		float Alpha = 0.0f;
		bool bGo = false;
	};
	/** That, `Now`, for a count that ends at `GoAt` (0 for none): nothing outside the count and the GoShown seconds after. Each second comes in large and settles, and fades out at its end; the word fades out over its time. */
	FShown Shown(float Now, float GoAt);
}
