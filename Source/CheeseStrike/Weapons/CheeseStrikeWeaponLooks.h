// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CheeseStrikeShot.h"

/**
 * How each weapon looks, sounds and is held: its model and where it sits in
 * the hand, its cheese materials, its animations, its sounds, where its
 * sights and muzzle are and where the free hand holds it. Plain data, by the
 * weapon's id; ACheeseStrikeWeapon loads what it names as it becomes that
 * weapon (at run time, not in a constructor, so that the scripts in
 * tools/art can rebuild the materials).
 *
 * Every model lies along its Y axis, muzzle forward and top up, its grip at
 * the origin, as Lyra's rifle does: `Model` puts one that doesn't so (its
 * size, turn and place). Places are in the model's own space, as it was
 * made, before `Model`.
 */
struct FWeaponLook
{
	const TCHAR* Id = TEXT("");
	ECheeseStrikeHold Hold = ECheeseStrikeHold::Rifle;
	/** A skinned model (Lyra's), with its own firing and reloading animations, or a plain one. */
	const TCHAR* SkinnedModel = nullptr;
	const TCHAR* PlainModel = nullptr;
	const TCHAR* FireAnimation = nullptr;
	const TCHAR* ReloadAnimation = nullptr;
	/** How the model sits in the grip: its size, turn and place. */
	FTransform Model = FTransform::Identity;
	/** Its materials, slot by slot (none keeps the model's own). */
	const TCHAR* Materials[4] = {nullptr, nullptr, nullptr, nullptr};
	/** Which way its business end points in the model's space: a gun's muzzle (+Y), a blade's tip. */
	FVector Along = FVector(0.0f, 1.0f, 0.0f);
	/** Where the eye goes down the sights, in the model's space; none for the rifle's own (from its bounds and the console's sight variables). */
	TOptional<FVector> Sight;
	/** Where the muzzle is, and how big its flash (1 the rifle's). */
	FVector Muzzle = FVector(0.0f, 70.0f, 4.0f);
	float FlashSize = 1.0f;
	/**
	 * Where the other hand closes round it (the middle of its closed fingers: FCheeseStrikeGripIK), in the model's
	 * space: on a long gun's fore-end, under the barrel's line; on a pistol, round the hand on its grip. None for a
	 * weapon held in one hand, the other free (the knife).
	 */
	TOptional<FVector> LeftHand;
	/** Its sounds: shots (one at random), the dry click, a reload's three, and how loud and how high its shots are. */
	const TCHAR* Shots[4] = {nullptr, nullptr, nullptr, nullptr};
	const TCHAR* DryFire = nullptr;
	const TCHAR* ReloadSounds[3] = {nullptr, nullptr, nullptr};
	float ShotVolume = 1.0f;
	float ShotPitch = 1.0f;
};

namespace CheeseStrikeWeaponLooks
{
	/** The look of the weapon `Id`, or the Cheddar Blaster's. */
	const FWeaponLook& For(FName Id);
	/**
	 * Where a weapon held `Hold`'s way sits in its player's view at the hip:
	 * where it is aimed down its sights (its sight line through the eye),
	 * moved (cm: forward, right, up in the view) and turned (about the eye),
	 * so that every weapon of a hold sits alike whatever its shape
	 * (FCheeseStrikeViewHold keeps it there however the body leans).
	 */
	FTransform HipFromAimed(ECheeseStrikeHold Hold);
	/** Where the knife hand is on guard in view (cm from the eye: forward, right, up). */
	FVector KnifeHand();
	/** The knife's swoosh, its stroke landing, and drawing a weapon. */
	extern const TCHAR* const KnifeSwings[4];
	/** The knife's stab, as it lands (tools/art/stab_sound.py, from OpenGameArt): the sound of a blow, not a punch. */
	extern const TCHAR* const KnifeStab;
	extern const TCHAR* const DrawSounds[3];
	/** The muzzle flash's model and material, and the material of a flash's light. */
	extern const TCHAR* const FlashModel;
	extern const TCHAR* const FlashMaterial;
	extern const TCHAR* const FlashFrontMaterial;
}
