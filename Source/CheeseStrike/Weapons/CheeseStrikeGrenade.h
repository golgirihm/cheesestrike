// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeGrenade.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikePlayerState;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UProjectileMovementComponent;
class USoundAttenuation;
class USoundBase;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A cheese grenade in flight: a frag grenade of yellow holed cheese with a
 * steel fuse, spoon and ring, sparking as it burns, thrown by a player (UCheeseStrikeGrenades). The server spawns it and
 * moves it, an arc under gravity that bounces off what it meets (but not
 * off players), and every machine sees it as the server has it, spinning
 * as it goes, the fuse sparking, a knock at each bounce. When its fuse has
 * burnt down (CheeseStrikeGrenades::Fuse) it bursts: every machine shows the
 * flash, a fireball swelling to where its harm stops, the bang and a
 * scorch, and the server harms whoever the burst can see within its radius
 * (a wall between them saves them), falling off with distance
 * (CheeseStrikeGrenades::DamageAt), as the thrower's kill, by the match's
 * rules for who may hurt whom; its thrower too, as their own doing.
 *
 * The Smoked Gouda (CheeseStrikeGrenades::EGrenadeKind::Smoke) is the same
 * round in a dark smoked rind, thrown, flown and bounced alike, with a
 * shorter fuse; at its end it harms nobody and leaves a cloud of smoke
 * where it lies (ACheeseStrikeSmokeCloud, spawned by the server), with a
 * puff rather than a flash.
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeGrenade : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeGrenade();

	/** Server only, as it is spawned: thrown by `Thrower` (whose player has the kill) at `Velocity`, a round of `Kind`. */
	void Launch(ACheeseStrikeCharacter* Thrower, const FVector& Velocity, CheeseStrikeGrenades::EGrenadeKind Kind = CheeseStrikeGrenades::EGrenadeKind::Cheese);
	/** The kind of round this is. */
	CheeseStrikeGrenades::EGrenadeKind GetKind() const { return CheeseStrikeGrenades::KindAt(Kind); }
	/**
	 * Dresses `Round` as a grenade of `Kind`: the frag's model (tools/art/weapon_models.py) in its cheese, or its dark
	 * smoked rind, and steel (tools/art/weapon_materials.py), loaded by path now so that the scripts can rebuild them;
	 * false, and nothing changed, without the model. The rounds in hand (UCheeseStrikeGrenades) wear the same.
	 */
	static bool DressRound(UStaticMeshComponent* Round, CheeseStrikeGrenades::EGrenadeKind Kind);

	/** The server's time it bursts. */
	float GetBurstsAt() const { return BurstsAt; }
	/** Whether it has burst (on the server; on the others, once they have been told). */
	bool HasBurst() const { return bBurst; }
	/** Server only: whom its burst harmed, and how many times it has bounced. */
	int32 GetHits() const { return Hits; }
	int32 GetBounces() const { return Bounces; }
	/** Server only: where it first came down (zero before). */
	FVector GetFirstBounceAt() const { return FirstBounceAt; }
	ACheeseStrikePlayerState* GetThrower() const { return Thrower.Get(); }

	/** The harm its burst would do `Victim` from `At`, seen or not: none behind a wall (server only, for the burst and the tests). */
	static float HarmAt(UWorld* World, const FVector& At, const ACheeseStrikeCharacter* Victim, float& OutDistance);
	/**
	 * Whether a burst at `At` can see any of `Victim` (their chest, head or feet: a wall, a floor or the cheese between
	 * hides them), and how far the nearest of those is (`OutDistance`): any burst's, a BabyBomb's too.
	 */
	static bool BurstSees(UWorld* World, const FVector& At, const ACheeseStrikeCharacter* Victim, float& OutDistance);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Another machine: its flight goes on from the server's speed as it arrives. */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

protected:
	/** Server only: the burst's harm. */
	void Burst();
	/** Every machine: the flash, the fireball, the bang and the scorch. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBurst(FVector_NetQuantize At);
	void ShowBurst(const FVector& At);
	UFUNCTION()
	void OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity);
	float Now() const;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProjectileMovementComponent> Movement;
	/** The grenade and its fuse's spark, under a spinning pivot. */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Round;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Spark;

	/** The server's time it bursts. */
	UPROPERTY(Replicated)
	float BurstsAt = 0.0f;
	/** CheeseStrikeGrenades::EGrenadeKind, as thrown. */
	UPROPERTY(Replicated)
	uint8 Kind = 0;
	/** The kind the round was last dressed as, so that one that arrives before its kind is dressed again. */
	uint8 DressedKind = 0;
	bool bBurst = false;
	float BurstShownAt = -1.0f;
	TWeakObjectPtr<ACheeseStrikePlayerState> Thrower;
	int32 Hits = 0;
	int32 Bounces = 0;
	FVector FirstBounceAt = FVector::ZeroVector;
	float LastKnockAt = -1.0f;
	/** How it spins in flight (degrees a second, about each axis). */
	FRotator Spin = FRotator::ZeroRotator;

	/** The burst's look, as it swells and fades. */
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Fireball;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> FireballMaterial;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Bangs;
	UPROPERTY()
	TObjectPtr<USoundBase> Rumble;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Knocks;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SparkMaterial;
};
