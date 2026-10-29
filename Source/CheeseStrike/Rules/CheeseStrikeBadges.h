// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A kill's badges: the extra tags a kill earns, shown on the killer's kill
 * pop, in the kill feed's line and on the killed's death screen, each a small
 * cheese-gold pill, and counted on the killer's player state (for the
 * achievements). Worked out on the server from what it knows of the kill
 * (FKillFacts) and carried with the kill feed's entry as a set of bits.
 * Tested in CheeseStrike.Badges.*.
 */
namespace CheeseStrikeBadges
{
	enum class EBadge : uint8
	{
		/** The blow that killed was to the head. */
		Headshot,
		/** The knife, from behind. */
		Assassination,
		/** Execution's finishing blow, a stab or a stomp. */
		Execution,
		/** The shotgun from PointBlankRange or nearer. */
		PointBlank,
		/** A gun's shot from LongshotRange or further. */
		Longshot,
		/** The killed was the last to have killed the killer. */
		Revenge,
		/** From full health to dead in one hit (not the Roquefort's headshot, which always is, nor a blade or an execution). */
		OneShot,
		Count
	};
	constexpr int32 NumBadges = static_cast<int32>(EBadge::Count);

	/** The shotgun within 2 m; any gun from 40 m. */
	constexpr float PointBlankRange = 200.0f;
	constexpr float LongshotRange = 4000.0f;

	/** What the server knows of a kill. */
	struct FKillFacts
	{
		/** The killer's weapon's id (CheeseStrikeArsenal), or none for a streak, a grenade, a fall or an execution. */
		FName Weapon;
		bool bHead = false;
		/** A stroke in the back (CheeseStrikeWeapons::FromBehind). */
		bool bFromBehind = false;
		bool bExecuted = false;
		/** From the shooter's eyes to the hit, in cm. */
		float Distance = 0.0f;
		/** The killed last killed the killer (and nobody has avenged it since). */
		bool bRevenge = false;
		/** The killed was at full health and one hit (one shot's pellets together, one stroke) killed them. */
		bool bOneHit = false;
		/** A kill of one's own (a suicide, a fall nobody caused) earns nothing. */
		bool bByAnother = true;
	};

	/** The badges `Facts` earn, as bits (1 << EBadge). */
	uint8 BadgesFor(const FKillFacts& Facts);
	inline bool Has(uint8 Badges, EBadge Badge) { return (Badges & (1 << static_cast<uint8>(Badge))) != 0; }
	/** The badges in `Badges`, in the order they are shown. */
	TArray<EBadge> List(uint8 Badges);
	/** The pill's word ("HEADSHOT"). */
	const TCHAR* Word(EBadge Badge);
}
