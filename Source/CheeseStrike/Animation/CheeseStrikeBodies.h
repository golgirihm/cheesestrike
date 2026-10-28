// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CheeseStrikeRules.h"

/**
 * The bodies a character can have, and which one each player gets, as plain
 * rules that a test can call without a world. Each is a Paragon hero's
 * default look (Content/LICENSES.md), animated by Lyra's rifle set
 * retargeted to its skeleton in the Lyra project (tools/art/lyra_hero.py),
 * with a ragdoll of its own (tools/art/ragdolls.py). In a match with sides
 * each side has heroes of its own, whose palettes go together (the Gouda
 * Guys dark and warm, the Mozza Radicals pale and cool), dressed in the
 * side's colour (CheeseStrikeModes::TeamColour) on their armour, so that the
 * sides are told apart at a glance and at range; without sides, each player
 * has one of the heroes in turn, in its own colours.
 * ACheeseStrikeCharacter applies what these say, on every machine, from the
 * players' replicated sides and ids.
 */
namespace CheeseStrikeBodies
{
	/** A body. */
	struct FBody
	{
		/** Who it is, for the logs and the tests. */
		const TCHAR* Name;
		/** The skeletal mesh (whose physics asset is its ragdoll), as an object path. */
		const TCHAR* Mesh;
		/** The package folder, ending in '/', of Lyra's rifle set retargeted to it, by Lyra's names. */
		const TCHAR* Animations;
		/** The material slots of the weapons it was modelled holding, hidden: only the game's own show. */
		const TCHAR* const* WeaponSlots;
		int32 NumWeaponSlots;
		/** How the rifle sits on its hand_r (the rifle is modelled along its Y axis): measured in the game. */
		FTransform HandGrip;
		/** The material slots its own player doesn't see, besides its head's: a helmet or chest that, as the arms come up to aim, would come before the eyes. */
		const TCHAR* const* FirstPersonHiddenSlots;
		int32 NumFirstPersonHiddenSlots;
		/** The folder ('/'-ended) of its material instances in each side's colours (tools/art/team_looks.py), MI_<slot>_<side>, and the slots they dress. */
		const TCHAR* TeamLooks;
		const TCHAR* const* TeamSlots;
		int32 NumTeamSlots;
		/**
		 * Its head for shots: a capsule round the segment from HeadLow to HeadHigh, HeadRadius thick, in the space of its `head`
		 * bone (X up the head), from the neck to the top of the head, fitted to the vertices on the head and every bone under it
		 * (the face, the hair, a helmet; CheeseStrike.Animation.TheHeadShapeFitsEachHero measures them).
		 */
		FVector HeadLow;
		FVector HeadHigh;
		float HeadRadius;
	};

	/** Every body. */
	TConstArrayView<FBody> All();
	/** Which of All a body is, by its name; INDEX_NONE for none. */
	int32 IndexOf(const TCHAR* Name);
	/** Which of All has the mesh at `Path` (an object path); INDEX_NONE for none (the mannequin). */
	int32 IndexOfMesh(const FString& Path);
	/** How many heroes each side has. */
	constexpr int32 HeroesPerTeam = 2;
	/** A side's heroes, by `Rank` in turn: the Gouda Guys' (Murdock and Twinblast), the Mozza Radicals' (Lt. Belica and Wraith). */
	int32 ForTeam(int32 Team, int32 Rank);
	/**
	 * The body a player has, by their `Rank`: how many players are before them
	 * (by player id) on their side, or in the match without sides. On a side,
	 * one of its heroes in turn; without sides, one of every hero in turn.
	 */
	int32 For(bool bTeams, int32 Team, int32 Rank);
	/** Which side's heroes a body is (INDEX_NONE for none). */
	int32 TeamOf(int32 Body);
	/** The short name of a side, as its materials are named: "Gouda", "Mozza". */
	const TCHAR* TeamWord(int32 Team);
	/** The material instance that dresses `Slot` of body `Body` in side `Team`'s colours, as an object path. */
	FString TeamLook(int32 Body, int32 Team, const FString& Slot);
}
