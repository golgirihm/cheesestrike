// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Where a character carries a weapon: one of each. */
enum class EWeaponSlot : uint8
{
	/** The main gun, chosen in the loadout. */
	Primary,
	/** The pistol. */
	Sidearm,
	/** The cheese knife. */
	Knife,
};
constexpr int32 NumWeaponSlots = 3;

/** How a weapon is held, which picks the body's animations: a long gun two-handed, a pistol, the shotgun (a long gun with its own firing and reloading), or the knife (the pistol's stance, the free hand up on guard). */
enum class ECheeseStrikeHold : uint8
{
	Rifle,
	Pistol,
	Shotgun,
	Knife,
};
constexpr int32 NumHolds = 4;

/**
 * What a weapon is and what a shot does, as arithmetic, apart from the
 * engine so that it can be tested alone. The weapon actor
 * (CheeseStrikeWeapon) fires by these; the server decides hits by them.
 *
 * The numbers are the Godot game's (docs/unreal/design.md, "Weapons"), a
 * starting point to tune from. Lengths are centimetres at life size (the
 * Godot game's, which was three times life size, divided by three); times
 * are seconds; angles are degrees. Every weapon is in CheeseStrikeArsenal.
 */
struct FWeaponStats
{
	FName Id;
	FText DisplayName;
	EWeaponSlot Slot = EWeaponSlot::Primary;
	/** Damage of one hit (one pellet's, for a shotgun) to the body, and how much more to the head. */
	float Damage = 20.0f;
	float HeadMultiplier = 1.75f;
	/** Seconds between shots (from the start of one burst to the next, for a weapon that fires in bursts), and whether holding the trigger keeps firing. */
	float FireInterval = 0.12f;
	bool bAutomatic = true;
	/** Shots each pull of the trigger fires, BurstInterval apart. */
	int32 Burst = 1;
	float BurstInterval = 0.07f;
	/** Rays each shot fires, each spread on its own over the cone. */
	int32 Pellets = 1;
	int32 MagazineSize = 24;
	float ReloadTime = 1.6f;
	/** Rounds carried besides the magazine at first, and the most that can be; below zero, there is no end to them. */
	int32 Reserve = 72;
	int32 ReserveMax = 120;
	/** How far a shot reaches at all. */
	float MaxRange = 5000.0f;
	/** The width of the cone shots spread over, fully bloomed: aimed down the sights, and from the hip. A shot goes up to half of it off the aim. */
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
	/** The field of view aimed down the sights, as a share of the usual, and whether that is through a scope. */
	float AimZoom = 0.6f;
	bool bScoped = false;
	/** Seconds it takes to come up when changed to, and the share of their usual speed a player moves at with it in hand. */
	float DrawTime = 0.35f;
	float CarrySpeed = 1.0f;

	/** The knife: how far from the eyes it reaches, how many degrees either side of the aim its swipe sweeps, the damage of a stroke in the back, and the seconds a stroke takes. */
	float Reach = 0.0f;
	float SweepDegrees = 0.0f;
	float BackstabDamage = 0.0f;
	float SwingTime = 0.0f;

	bool IsMelee() const { return Slot == EWeaponSlot::Knife; }
	/** Whether it never runs out of rounds to reload with. */
	bool HasEndlessReserve() const { return Reserve < 0; }
	/** Whether it fires rounds at all: the knife doesn't. */
	bool UsesRounds() const { return MagazineSize > 0; }
};

namespace CheeseStrikeWeapons
{
	/** The Cheddar Blaster: the automatic rifle, everyone's primary unless they choose another. */
	const FWeaponStats& Blaster();

	/** Aiming down the sights steadies the recoil to this share of it. */
	constexpr float AimRecoilFactor = 0.6f;
	/** How big a head is for a shot, and how big the body is: an upright capsule of the character's size. */
	constexpr float HeadRadius = 16.0f;
	/** Health everyone starts with. */
	constexpr float MaxHealth = 100.0f;
	/** Seconds a shot may come early after a weapon is drawn, for the network's jitter: the server allows it so much sooner than the draw time. */
	constexpr float DrawAllowance = 0.15f;
	/** A knife's swipe is tried along the aim and then this many degrees further to each side at a time, out to its sweep. */
	constexpr float SweepStep = 9.0f;

	/** What the HUD shows of a reserve beside the magazine: "/ 72", or "/ ∞" for rounds without end (below zero). */
	FString ReserveLabel(int32 Reserve);

	/** How much of `Weapon`'s damage a hit at `Distance` does, before the head. */
	float DamageAt(const FWeaponStats& Weapon, float Distance, bool bHead);

	/** Seconds from the first shot to shot number `Index` (0 the first), holding the trigger (or pulling it as fast as it allows). */
	float ShotTime(const FWeaponStats& Weapon, int32 Index);
	/** Seconds after a shot, number `InBurst` of its burst (0 the first), until the next may go. */
	float Cooldown(const FWeaponStats& Weapon, int32 InBurst);
	/** Shots a second, at most, over a while: what the server holds a shooter to. */
	float ShotsPerSecond(const FWeaponStats& Weapon);

	/** Seconds to kill full health with body shots from `Weapon` (every pellet hitting, close), holding the trigger; for the knife, strokes from the front. */
	float TimeToKill(const FWeaponStats& Weapon);
	/** Shots (or strokes) it takes to kill full health that way. */
	int32 ShotsToKill(const FWeaponStats& Weapon);

	/** The width of the cone, in degrees, shot number `Index` of a run (0 the first) spreads over, aimed or from the hip. */
	float SpreadFor(const FWeaponStats& Weapon, int32 Index, bool bAimed);

	/** Whether a weapon drawn at `DrawnAt` (server time) can fire at `Now`, allowing `Allowance` seconds early. */
	bool IsDrawn(const FWeaponStats& Weapon, float DrawnAt, float Now, float Allowance = 0.0f);

	/** The directions a knife stroke along `Direction` (normalized) is tried in: straight first, then fanned out to each side, a step at a time (SweepStep), all the way to the sweep. */
	TArray<FVector> SweepDirections(const FWeaponStats& Knife, const FVector& Direction);
	/** Whether a stroke along `Direction` lands in the back of someone facing `Yaw` degrees: from within a quarter turn of straight behind. */
	bool FromBehind(const FVector& Direction, float Yaw);
	/** The damage of a knife stroke along `Direction` on someone facing `Yaw`. */
	float StrokeDamage(const FWeaponStats& Knife, const FVector& Direction, float Yaw);

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

	/** A direction in a cone `ConeDegrees` wide about `Direction` (so at most half that off it), from `Random`, which the shooter and the server share. */
	FVector Spread(const FVector& Direction, float ConeDegrees, FRandomStream& Random);

	/**
	 * How far along the ray from `Origin` in `Direction` (normalized) it first
	 * touches an upright capsule whose feet are at `Feet`, or -1 if it misses.
	 * A ray that starts inside doesn't hit it.
	 */
	float RayHitsCapsule(const FVector& Origin, const FVector& Direction, const FVector& Feet, float Radius, float Height);

	/** How far along the ray it first touches a sphere, or -1. */
	float RayHitsSphere(const FVector& Origin, const FVector& Direction, const FVector& Centre, float Radius);

	/** How far along the ray it first touches a capsule lying any way, round the segment from `From` to `To`, or -1. A ray that starts inside doesn't hit it. */
	float RayHitsSegment(const FVector& Origin, const FVector& Direction, const FVector& From, const FVector& To, float Radius);
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
		/** Which way they faced, in degrees about the vertical, for a stroke in the back. */
		float Yaw = 0.0f;
		float Radius = 34.0f;
		float Height = 192.0f;
		/** Lying down (Execution's last stand): the body is then the torso from the hips to the shoulders and the legs from the hips to the feet (CheeseStrikeDowned's shape and radii), not the upright capsule. */
		bool bLying = false;
		FVector Hips = FVector::ZeroVector;
		FVector Shoulders = FVector::ZeroVector;
		FVector LegsTo = FVector::ZeroVector;
	};

	/** Seconds kept. */
	static constexpr float Kept = 1.0f;

	void Add(const FSample& Sample);
	/** The feet, head, facing and size at `Time`, between the samples either side of it; the latest for a time past the end, the earliest for one before the start. */
	FSample At(float Time) const;
	/** How far along a ray a body sampled as `Sample` is first touched, or -1: the upright capsule, or the torso and legs lying. */
	static float RayHitsBody(const FVector& Origin, const FVector& Direction, const FSample& Sample);
	int32 Num() const { return Samples.Num(); }

private:
	TArray<FSample> Samples;
};
