// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * What is overhead on a map, for the streaks that come from above: a roof a
 * trace up meets (The Creamery's hall, The Fondue Floor's), and the ceilings
 * that block nothing (`kit.overhead` in the map scripts: The Labyrinth's
 * vaulting, the Grotto's rock, the Kitchen's plaster), which no trace meets,
 * found by their bounds: every static mesh without collision at least
 * MinAcross wide both ways. Found once for a level (Of), as it looks at the
 * whole of it; what doesn't move doesn't change.
 */
class FCheeseStrikeOverhead
{
public:
	explicit FCheeseStrikeOverhead(UWorld* InWorld);
	/** The one for `World`, found the first time it is asked for. */
	static const FCheeseStrikeOverhead& Of(UWorld* World);

	/** Things without collision narrower than this (cm) either way are lamps, cables and the like, not a roof. */
	static constexpr float MinAcross = 400.0f;

	/** The underside of what is over `At`, no more than `Within` above it; unset under open sky. */
	TOptional<float> RoofOver(const FVector& At, float Within) const;
	/** Whether a box of `HalfSize`, upright, goes from `From` to `To` without meeting anything solid (players and mice aside). */
	bool IsClear(const FVector& From, const FVector& To, const FVector& HalfSize) const;

private:
	UWorld* World = nullptr;
	/** The ceilings without collision. */
	TArray<FBox> Ceilings;
	/** Players and mice, which a trace for a roof goes through: those there now. */
	TArray<const AActor*> Movers() const;
};
