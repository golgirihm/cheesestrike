// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The mouse that now and then crosses the front end behind the figure
 * (ACheeseStrikeMenuMouse), as plain rules: when it comes, and what it is
 * doing a given time into a visit. A visit: it walks in from one side,
 * stops, sniffs, turns its head toward the cheese put out for it, looks a
 * moment, and walks off the other side, its head turning back as it goes.
 */
namespace CheeseStrikeMenuMouse
{
	/** No visit in the front end's first seconds. */
	constexpr float NotBefore = 5.0f;
	/** The first visit comes between these (seconds after the front end shows). */
	constexpr float FirstFrom = 8.0f;
	constexpr float FirstTo = 20.0f;
	/** Later visits: one starts every so many seconds after the last one started. */
	constexpr float EveryFrom = 20.0f;
	constexpr float EveryTo = 40.0f;
	/** Its size: the Black Rat's model scaled up (the match's vermin are 15). */
	constexpr float Scale = 4.0f;
	/** How fast it walks, centimetres a second, and the walk's play rate at that pace. */
	constexpr float WalkSpeed = 100.0f;
	constexpr float WalkPlayRate = 1.0f;
	/** The phases' lengths, seconds. */
	constexpr float SniffSeconds = 2.6f;
	constexpr float TurnSeconds = 0.6f;
	constexpr float LookSeconds = 1.4f;
	/** Seconds its head takes to come back round as it walks off. */
	constexpr float TurnBackSeconds = 0.5f;
	/** The most its head turns from its body, degrees. */
	constexpr float MostHeadTurn = 60.0f;

	enum class EPhase : uint8
	{
		/** Not on the screen: waiting for the next visit. */
		Away,
		WalkIn,
		/** Stopped, sniffing. */
		Sniff,
		/** Turning its head toward the cheese. */
		Turn,
		/** Looking at it a moment. */
		Look,
		WalkOff,
	};

	/** Seconds until the next visit starts: the first (`bFirst`) or a later one; `Random` from 0 to 1 picks within the range. */
	float NextVisitIn(bool bFirst, float Random);

	/** Where a visit is `Since` seconds after it started, with `WalkIn` and `WalkOff` seconds of walking. */
	struct FMoment
	{
		EPhase Phase = EPhase::Away;
		/** Seconds into the phase. */
		float Into = 0.0f;
		/** How far along its walk (0 to 1) while walking in or off; 1 walked in while stopped. */
		float Along = 0.0f;
		/** How much of its head's turn toward the cheese it has (0 to 1). */
		float HeadShare = 0.0f;
	};
	FMoment At(float Since, float WalkIn, float WalkOff);
	/** How long a whole visit takes. */
	float VisitSeconds(float WalkIn, float WalkOff);
	/** The head's turn toward the cheese, degrees: from the way it faces (`FacingYaw`) toward `ToCheeseYaw`, no more than MostHeadTurn. */
	float HeadTurnToward(float FacingYaw, float ToCheeseYaw);
}
