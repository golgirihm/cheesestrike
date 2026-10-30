// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeProjectile.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikePlayerState;
class ACheeseStrikeWeapon;
class UMaterialInterface;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
struct FWeaponStats;

/**
 * A round in flight from a projectile weapon (FWeaponStats::ProjectileSpeed: the BabyBomb's red-waxed wheel, the Cheese
 * Grater's shards). The server spawns it, flies it and decides what it meets; every machine sees it fly as the server
 * moves it, carrying on between updates by the same arithmetic.
 *
 * Each frame on the server it goes on along its way, falling at its weapon's share of gravity and, homing, turned
 * toward the enemy it was fired at (CheeseStrikeWeapons::TurnToward, at most HomingTurn degrees a second), and the
 * stretch it covered is tested against the arena (a line trace, the characters left out) and against every player
 * where they are (FHitHistory::RayHitsPlayer, the head as shots find it). A player met takes its Damage, as its
 * shooter's hit (or, the shooter gone, their blast); a weapon with a burst (SplashRadius) then bursts where it met
 * them or the arena, harming whoever the burst can see by CheeseStrikeWeapons::SplashAt, its shooter too, the one it
 * hit outright excepted. A shard stuck in a player stays there a second for everyone to see, and the shooter's weapon
 * counts them (ACheeseStrikeWeapon::AddShard): enough in one player in a short while burst for a kill, chunks of
 * cheese flying.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeProjectile();

	/**
	 * Server only, as it is spawned (deferred): fired by `Weapon` in `Shooter`'s hands at `Velocity`, homing on `Homing`
	 * if its weapon homes (null for nobody).
	 */
	void Launch(ACheeseStrikeWeapon* Weapon, ACheeseStrikeCharacter* Shooter, const FVector& Velocity, ACheeseStrikeCharacter* Homing);

	FName GetWeaponId() const { return WeaponId; }
	const FVector& GetFlight() const { return Flight; }
	/** Whether it has come to an end: burst, or stuck in something. */
	bool IsDone() const { return bDone; }
	/** Whom it is homing on, if anyone. */
	ACheeseStrikeCharacter* GetHoming() const { return Homing; }
	/** Server only: whom it struck outright, if anyone. */
	ACheeseStrikeCharacter* GetStruck() const { return Struck.Get(); }

	/**
	 * Server only: the burst of `Stats`'s round at `At`, fired by `Shooter` (their player `By`, who keeps the kill if they
	 * are gone), sparing `Spared` (the one it hit outright): whom it harmed.
	 */
	static int32 Splash(UWorld* World, const FWeaponStats& Stats, const FVector& At, ACheeseStrikeCharacter* Shooter, ACheeseStrikePlayerState* By, const ACheeseStrikeCharacter* Spared, ACheeseStrikeWeapon* Weapon);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Another machine: its flight goes on from the server's speed as it arrives. */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

protected:
	/** Server only: goes on for `DeltaSeconds`, meeting what it meets. */
	void Fly(float DeltaSeconds);
	/** Server only: it meets `Victim` at `At`, on the head or not, after flying `Distance`. */
	void HitPlayer(ACheeseStrikeCharacter* Victim, const FVector& At, bool bHead, const FVector& Way);
	/** Server only: it comes to an end at `At` (a burst there, if its weapon has one). */
	void End(const FVector& At, bool bBurst);
	/** Every machine: its burst's flash, fireball, bang and scorch at `At`. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBurst(FVector_NetQuantize At);
	/** Every machine: a supercombine's burst in someone at `At`. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSuper(FVector_NetQuantize At);
	void ShowBurst(const FVector& At);
	/** Puts its look on, from its weapon's (CheeseStrikeWeaponLooks). */
	void ApplyLook();
	float Now() const;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Round;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** Which weapon fired it: sent once, as it is made. */
	UPROPERTY(Replicated)
	FName WeaponId;
	/** Whom it homes on, if anyone. */
	UPROPERTY(Replicated)
	TObjectPtr<ACheeseStrikeCharacter> Homing;
	/** It has come to an end: it flies no more (stuck, or burst and hidden). */
	UPROPERTY(Replicated)
	bool bDone = false;
	const FWeaponStats* Stats = nullptr;
	/** Its velocity (cm/s), on every machine. */
	FVector Flight = FVector::ZeroVector;
	/** The server's time it was fired, where from, and who fired it with what. */
	float FiredAt = 0.0f;
	FVector FiredFrom = FVector::ZeroVector;
	TWeakObjectPtr<ACheeseStrikeCharacter> Shooter;
	TWeakObjectPtr<ACheeseStrikePlayerState> By;
	TWeakObjectPtr<ACheeseStrikeWeapon> Weapon;
	TWeakObjectPtr<ACheeseStrikeCharacter> Struck;
	/** How it spins in flight (degrees a second, about its way). */
	float Spin = 0.0f;
	/** The burst's look, as it swells and fades. */
	float BurstShownAt = -1.0f;
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Fireball;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FireballMaterial;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Bangs;
	UPROPERTY()
	TObjectPtr<USoundBase> Rumble;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
};
