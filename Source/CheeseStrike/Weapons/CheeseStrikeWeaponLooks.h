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
	const TCHAR* Materials[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	/** Which way its business end points in the model's space: a gun's muzzle (+Y), a blade's tip. */
	FVector Along = FVector(0.0f, 1.0f, 0.0f);
	/**
	 * Where the eye goes down the sights, in the model's space; none for the rifle's own (from its bounds and the console's
	 * sight variables). For two guns (FWeaponStats::bDual) a point between them: CheeseStrikeDual::AimHalfApart to the
	 * right gun's left, so that aimed, the two come up side by side either side of the eye.
	 */
	TOptional<FVector> Sight;
	/**
	 * Held in the fist (the knife, the Cheese Stick): its grip in the hand's own grip, as the hand's bones have it
	 * (CheeseStrikeArms::FistOfSkeleton, HeldInFist: its `Along` out of the top of the fist, its top, +Z, back from the
	 * fingers), rather than in the rifle's grip frame by `Model` (which a stand still shows it by).
	 */
	bool bInFist = false;
	/**
	 * Drawn in its own player's view by the animations' equip, the arms swung through it (the Curd Pistol and the two guns,
	 * whose draws look right); every other weapon comes up from below in code alone (wave 17: CheeseStrikeArms::RaiseLowered),
	 * as Lyra's rifle equip flung the arms about. The body others see plays the equip whichever.
	 */
	bool bDrawAnimation = false;
	/** Held in the fist, how far along it from its grip's origin the fist closes (cm, toward its business end). */
	float FistAlong = 0.0f;
	/** A melee weapon held and swung as a sledgehammer, in both hands (the Cheese Stick: CheeseStrikeArms::SledgeRest, SledgeSwing). */
	bool bSledge = false;
	/**
	 * Held by a handle in the right fist, every finger closed round it and the thumb over them (CheeseStrikeCloseFist), not
	 * by a gun's grip with the index on its trigger: the BabyBomb's haft (wave 17).
	 */
	bool bFistGrip = false;
	/**
	 * A part of the model that turns while it fires (the Gouda Gatling's barrels): a second plain model in the same frame,
	 * its materials slot by slot, and the axis it turns about (a direction and a point on it, in the model's space).
	 */
	const TCHAR* SpinningPart = nullptr;
	const TCHAR* SpinningMaterials[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	FVector SpinAxis = FVector(0.0f, 1.0f, 0.0f);
	FVector SpinCentre = FVector::ZeroVector;
	/** Where the muzzle is, and how big its flash (1 the rifle's). */
	FVector Muzzle = FVector(0.0f, 70.0f, 4.0f);
	float FlashSize = 1.0f;
	/**
	 * Where the other hand closes round it (the middle of its closed fingers: FCheeseStrikeGripIK; for a weapon held in
	 * the fist, the middle of the fist, round the handle), in the model's space: on a long gun's fore-end, under the
	 * barrel's line; up the Cheese Stick's handle. None for a weapon held in one hand, the other free (the knife, the
	 * Curd Pistol) or holding its own (two guns).
	 */
	TOptional<FVector> LeftHand;
	/**
	 * In its own player's view, where the other hand holds it instead, if not at LeftHand (wave 17: on a thick fore-end, the
	 * body others see, and the loadout page's figure, hold it under it, the fingers round its bottom; the view keeps the
	 * hand up on its near side, where it shows, as on the Roquefort).
	 */
	TOptional<FVector> ViewLeftHand;
	/**
	 * At the hip in its own player's view, moved this far over its hold's place (CheeseStrikeWeaponLooks::HipFromAimed; cm
	 * forward, right, up in the view): a long gun whose fore-end is far from its grip brought back and up (wave 17: the
	 * Cheddar Blaster and the Swiss Shotgun), so that the other arm reaches it bent, the elbow down, as on the Roquefort.
	 */
	FVector HipShift = FVector::ZeroVector;
	/**
	 * Its sounds: shots (one at random; a melee weapon's swings, or none for the knife's), the dry click, a reload's three,
	 * and how loud and how high its shots are (every shot is also kept down by CheeseStrikeGunfire::Loudness).
	 */
	const TCHAR* Shots[4] = {nullptr, nullptr, nullptr, nullptr};
	const TCHAR* DryFire = nullptr;
	const TCHAR* ReloadSounds[3] = {nullptr, nullptr, nullptr};
	float ShotVolume = 1.0f;
	float ShotPitch = 1.0f;
	/** A melee weapon's blow landing (none: the knife's stab). */
	const TCHAR* StrikeSound = nullptr;
	/** The colour of the streak of light each shot leaves (a tracer, M_Tracer); none where its alpha is 0. */
	FLinearColor Tracer = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/** The weapon whose HUD picture it borrows (tools/art/weapon_icons.py's), if not its own. */
	const TCHAR* IconOf = nullptr;
	/**
	 * A projectile weapon's round in flight (ACheeseStrikeProjectile): its model, its materials slot by slot, and its size
	 * and turn, its front along X; and how fast it spins about its flight (degrees a second).
	 */
	const TCHAR* ProjectileModel = nullptr;
	const TCHAR* ProjectileMaterials[2] = {nullptr, nullptr};
	FTransform ProjectileShape = FTransform::Identity;
	float ProjectileSpin = 0.0f;
	/** The colour of the light it glows with as it flies (none where its alpha is 0). */
	FLinearColor ProjectileGlow = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
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
	/** A tracer's material (tools/art/weapon_materials.py): unlit and additive, its Colour and Brightness set by the weapon. */
	extern const TCHAR* const TracerMaterial;
	/** How long a tracer is (cm), how fast it flies (cm/s), and how long it is seen (s). */
	constexpr float TracerLength = 500.0f;
	constexpr float TracerSpeed = 25000.0f;
	constexpr float TracerLife = 0.15f;
	constexpr float TracerThickness = 4.0f;
}
