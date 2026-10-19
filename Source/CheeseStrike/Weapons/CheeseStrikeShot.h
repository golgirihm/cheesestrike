// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * What a weapon is and what a shot does, as arithmetic, apart from the
 * engine so that it can be tested alone. The weapon actor
 * (CheeseStrikeWeapon) fires by these; the server decides hits by them.
 *
 * The numbers are the Godot game's (docs/unreal/design.md, "Weapons"), a
 * starting point to tune from. Lengths are centimetres at life size (the
 * Godot game's, which was three times life size, divided by three); times
 * are seconds; angles are degrees.
 */
struct FWeaponStats
{
	FName Id;
	FText DisplayName;
	/** Damage of one hit to the body, and how much more to the head. */
	float Damage = 20.0f;
	float HeadMultiplier = 1.75f;
	/** Seconds between shots, and whether holding the trigger keeps firing. */
	float FireInterval = 0.12f;
	bool bAutomatic = true;
	int32 MagazineSize = 24;
	float ReloadTime = 1.6f;
	/** Rounds carried besides the magazine at first, and the most that can be. */
	int32 Reserve = 72;
	int32 ReserveMax = 120;
	/** How far a shot reaches at all. */
	float MaxRange = 5000.0f;
	/** The cone shots spread over, fully bloomed: aimed down the sights, and from the hip. */
	float SpreadDegrees = 0.3f;
	float HipSpreadDegrees = 9.0f;
	/** Shots in a run over which the spread grows from nothing to its full cone. */
	int32 BloomShots = 8;
	/** Damage falls off from FalloffStart to FalloffEnd, down to FalloffFloor of itself. */
	float FalloffStart = 1333.0f;
	float FalloffEnd = 3000.0f;
	float FalloffFloor = 0.6f;
	/** Recoil: degrees the view climbs a shot, capped at so many shots' worth, degrees it wanders to the side, and seconds a run of fire ends after. */
	float RecoilUp = 0.45f;
	float RecoilCap = 9.0f;
	float RecoilSide = 0.35f;
	float RecoilReset = 0.3f;
	/** The field of view aimed down the sights, as a share of the usual. */
	float AimZoom = 0.6f;
};

namespace CheeseStrikeWeapons
{
	/** The Cheddar Blaster: the automatic rifle everyone starts the port with. */
	const FWeaponStats& Blaster();

	/** Aiming down the sights steadies the recoil to this share of it. */
	constexpr float AimRecoilFactor = 0.6f;
	/** How big a head is for a shot, and how big the body is: an upright capsule of the character's size. */
	constexpr float HeadRadius = 16.0f;
	/** Health everyone starts with. */
	constexpr float MaxHealth = 100.0f;

	/** How much of `Weapon`'s damage a hit at `Distance` does, before the head. */
	float DamageAt(const FWeaponStats& Weapon, float Distance, bool bHead);

	/** Seconds to kill full health with body shots from `Weapon`, holding the trigger. */
	float TimeToKill(const FWeaponStats& Weapon);

	/** The cone, in degrees, shot number `Index` of a run (0 the first) spreads over, aimed or from the hip. */
	float SpreadFor(const FWeaponStats& Weapon, int32 Index, bool bAimed);

	/**
	 * How far shot number `Index` of a run of fire (0 for the first, which goes
	 * exactly where it is aimed) is thrown off, in degrees, as (to the right,
	 * upward): a climb that slows and never passes RecoilCap shots' worth,
	 * and a wander from side to side along a pattern a player can learn.
	 */
	FVector2D RecoilFor(const FWeaponStats& Weapon, int32 Index, bool bAimed);

	/** Which shot of a run the next is, `SinceLast` seconds after one that was number `Last` (-1 for none). */
	int32 NextInRun(const FWeaponStats& Weapon, int32 Last, float SinceLast);

	/** `Direction` turned by `Degrees` (right, up), with `Up` as up. */
	FVector Turned(const FVector& Direction, const FVector2D& Degrees, const FVector& Up = FVector::UpVector);

	/** A direction within `ConeDegrees` of `Direction`, from `Random`, which the shooter and the server share. */
	FVector Spread(const FVector& Direction, float ConeDegrees, FRandomStream& Random);

	/**
	 * How far along the ray from `Origin` in `Direction` (normalized) it first
	 * touches an upright capsule whose feet are at `Feet`, or -1 if it misses.
	 * A ray that starts inside doesn't hit it.
	 */
	float RayHitsCapsule(const FVector& Origin, const FVector& Direction, const FVector& Feet, float Radius, float Height);

	/** How far along the ray it first touches a sphere, or -1. */
	float RayHitsSphere(const FVector& Origin, const FVector& Direction, const FVector& Centre, float Radius);
}

/**
 * Where a player was lately, for the server to put them back to the moment
 * a shooter saw them: a few seconds of (time, feet, head) samples.
 */
struct FHitHistory
{
	struct FSample
	{
		float Time = 0.0f;
		FVector Feet = FVector::ZeroVector;
		FVector Head = FVector::ZeroVector;
		float Radius = 34.0f;
		float Height = 192.0f;
	};

	/** Seconds kept. */
	static constexpr float Kept = 1.0f;

	void Add(const FSample& Sample);
	/** The feet, head and size at `Time`, between the samples either side of it; the latest for a time past the end, the earliest for one before the start. */
	FSample At(float Time) const;
	int32 Num() const { return Samples.Num(); }

private:
	TArray<FSample> Samples;
};
