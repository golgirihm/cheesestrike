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
	const TCHAR* Materials[3] = {nullptr, nullptr, nullptr};
	/** Where the eye goes down the sights, in the model's space; none for the rifle's own (from its bounds and the console's sight variables). */
	TOptional<FVector> Sight;
	/** Where the muzzle is, and how big its flash (1 the rifle's). */
	FVector Muzzle = FVector(0.0f, 70.0f, 4.0f);
	float FlashSize = 1.0f;
	/** Where the free hand holds it, in the model's space; none for where the animation has it (the rifle's fore-end, 26 cm out). */
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
	/** Where the left hand holds the rifle in its animations, in its space: what LeftHand is measured from. */
	const FVector RifleLeftHand(6.0f, 26.4f, 5.5f);
	/**
	 * Where the hand holding a weapon held `Hold`'s way sits in its player's
	 * view at the hip (cm from the eye: forward, right, up), as the standing
	 * idle has it: ACheeseStrikeCharacter keeps it about there however the
	 * body leans (crouched, moving).
	 */
	FVector HipHand(ECheeseStrikeHold Hold);
	/** And which way its model points there (its Y axis, in the view's axes), as a unit vector. */
	FVector HipAlong(ECheeseStrikeHold Hold);
	/** The knife's swoosh, its stroke landing, and drawing a weapon. */
	extern const TCHAR* const KnifeSwings[4];
	extern const TCHAR* const KnifeHits[3];
	extern const TCHAR* const DrawSounds[3];
	/** The muzzle flash's model and material, and the material of a flash's light. */
	extern const TCHAR* const FlashModel;
	extern const TCHAR* const FlashMaterial;
	extern const TCHAR* const FlashFrontMaterial;
}
