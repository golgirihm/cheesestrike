// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The melted gouda on the floor under a foe the Gouda Overload has splatted
 * (wave 16): while they are slowed (CheeseStrikeStreaks::GooFactor under 1),
 * every machine that draws lays gooey orange-yellow puddles at their feet, a
 * big one as the gouda lands and then a fresh one every PuddleEvery cm they
 * go, or every PuddleRenew seconds where they stand, each spreading out from
 * a part of its size over PuddleGrowSeconds and lasting PuddleStays before it
 * fades over PuddleFades; the thinner the goo on them (GooWash), the smaller
 * each new one, so that the trail dwindles as they come back to speed and is
 * gone a few seconds after. Since wave 17 each puddle is a thick glob of
 * gouda with volume (SM_GooBlob1..3, tools/art/goo_blobs.py) lying in a
 * glossy wet film (the decal, FilmShare wider): it lands as a heap and
 * slumps out as it spreads (BlobHeightAt), and melts down flat as it fades.
 * Only for show (ACheeseStrikeGooSplats, Effects/),
 * as plain arithmetic tested without a world (CheeseStrike.Goo.*).
 *
 * Lengths are centimetres, times seconds.
 */
namespace CheeseStrikeGoo
{
	/** The puddle as the gouda lands: its radius. */
	constexpr float FirstRadius = 80.0f;
	/** A later puddle's radius, by how thick the goo still is (CheeseStrikeStreaks::GooWash: 1 just splatted, 0 gone). */
	constexpr float ThickRadius = 48.0f;
	constexpr float ThinRadius = 22.0f;
	float RadiusFor(float Wash);
	/** A fresh puddle is laid every PuddleEvery cm the foe goes, or every PuddleRenew seconds where they stand. */
	constexpr float PuddleEvery = 35.0f;
	constexpr float PuddleRenew = 1.2f;
	bool PuddleDue(float MovedSince, float SecondsSince);
	/** Each spreads from PuddleStart of its radius to all of it over PuddleGrowSeconds (quick, then slowing, as a liquid does). */
	constexpr float PuddleStart = 0.35f;
	constexpr float PuddleGrowSeconds = 1.4f;
	float RadiusAt(float Full, float Since);
	/** Each stays whole for PuddleStays, then fades over PuddleFades and is gone. */
	constexpr float PuddleStays = 3.5f;
	constexpr float PuddleFades = 2.0f;
	float OpacityAt(float Since);
	bool IsGone(float Since);
	/** The glob's mesh is BlobRadius across its middle (cm, about its edge's waves), as made by tools/art/goo_blobs.py. */
	constexpr float BlobRadius = 50.0f;
	/** How tall a glob is for its width: its height scaled BlobThick of its width's scale (the mesh is about 20 cm tall at BlobRadius: an 80 cm puddle's glob is about 18 cm at its humps). */
	constexpr float BlobThick = 0.55f;
	/** A glob lands as a heap BlobHeap times as tall and slumps to its height as it spreads (PuddleGrowSeconds), then melts flat as it fades (OpacityAt): its height's share at `Since`. */
	constexpr float BlobHeap = 1.6f;
	float BlobHeightAt(float Since);
	/** The wet film round a glob (the decal) is FilmShare of its radius. */
	constexpr float FilmShare = 1.2f;
	/** The most puddles on a machine at once (the oldest goes first). */
	constexpr int32 MostPuddles = 64;
	/** How far under the feet the floor may be for a puddle to be laid (cm below the capsule's bottom): none in the air. */
	constexpr float FloorReach = 40.0f;
}
