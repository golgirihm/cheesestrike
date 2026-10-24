// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
 */
UCLASS(NotPlaceable)
class CHEESESTRIKE_API ACheeseStrikeGrenade : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeGrenade();

	/** Server only, as it is spawned: thrown by `Thrower` (whose player has the kill) at `Velocity`. */
	void Launch(ACheeseStrikeCharacter* Thrower, const FVector& Velocity);

	/** The server's time it bursts. */
	float GetBurstsAt() const { return BurstsAt; }
	/** Whether it has burst (on the server; on the others, once they have been told). */
	bool HasBurst() const { return bBurst; }
	/** Server only: whom its burst harmed, and how many times it has bounced. */
	int32 GetHits() const { return Hits; }
	int32 GetBounces() const { return Bounces; }
	ACheeseStrikePlayerState* GetThrower() const { return Thrower.Get(); }

	/** The harm its burst would do `Victim` from `At`, seen or not: none behind a wall (server only, for the burst and the tests). */
	static float HarmAt(UWorld* World, const FVector& At, const ACheeseStrikeCharacter* Victim, float& OutDistance);

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
	bool bBurst = false;
	float BurstShownAt = -1.0f;
	TWeakObjectPtr<ACheeseStrikePlayerState> Thrower;
	int32 Hits = 0;
	int32 Bounces = 0;
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
