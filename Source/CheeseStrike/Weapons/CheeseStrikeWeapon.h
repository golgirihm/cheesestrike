// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeShot.h"
#include "CheeseStrikeWeapon.generated.h"

class ACheeseStrikeCharacter;
class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;

/**
 * A weapon in a character's hands. It has two meshes, one only its owner
 * sees in their first-person view and one everyone else sees on the body,
 * and fires by its FWeaponStats.
 *
 * A shot is fired on the shooter's machine, which shows it at once, kicks
 * the view and sends the server where it was aimed, with the seed its
 * spread comes from. The server checks the shot could have been fired,
 * puts every other player back to where the shooter saw them (FHitHistory),
 * decides what it hit, applies the damage, and tells the shooter whether it
 * hit. Ammunition is the server's count; the shooter's own count runs ahead
 * of it and is put right as it arrives.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeWeapon : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeWeapon();

	const FWeaponStats& GetStats() const { return *Stats; }
	int32 GetMagazine() const { return Magazine; }
	int32 GetReserve() const { return Reserve; }
	bool IsReloading() const;
	bool IsAimed() const { return bAimed; }
	bool IsFiring() const { return bTriggerHeld; }
	/** Which shot of the run of fire the next would be. */
	int32 NextShotIndex() const;
	/** The cone the next shot spreads over, in degrees. */
	float CurrentSpread() const;
	/** Shots fired by this machine's owner, and hits the server confirmed, for tests and the trace. */
	int32 ShotsFired = 0;
	int32 HitsConfirmed = 0;
	float LastHitDamage = 0.0f;
	bool bLastHitHead = false;
	/** Seconds since the server last confirmed a hit, for the HUD's marker. */
	float SinceHit() const;

	/** Puts the weapon in `Character`'s hands, on both meshes, and gives the body its animation. */
	void Hold(ACheeseStrikeCharacter* Character);

	void StartFiring();
	void StopFiring();
	void StartAiming();
	void StopAiming();
	void StartReload();

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

	//~ AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

protected:
	/** Fires one shot on the owner's machine, if the weapon can. */
	void FireOnce();
	/** Tells the server a shot was fired from `Origin` along `AimDirection` (before spread), shot `Index` of a run, spread from `Seed`, at the server's time `ClientTime` as the shooter had it. */
	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, int32 Seed, int32 Index, bool bWasAimed, float ClientTime);
	/** Tells the shooter the server found their shot hit, for `Damage`, on the head or not. */
	UFUNCTION(Client, Reliable)
	void ClientHit(float Damage, bool bHead);
	UFUNCTION(Server, Reliable)
	void ServerReload();
	void FinishReload();
	/** Plays the firing animation on the owner's first-person body. */
	void ShowShot();
	/** Whether the server can allow a shot now: the rate, and the magazine. */
	bool ServerAllowsShot(float Now);

	/** The weapon's numbers. */
	const FWeaponStats* Stats = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

	UPROPERTY()
	TObjectPtr<UAnimMontage> FiringMontage;
	UPROPERTY()
	TSubclassOf<UAnimInstance> FirstPersonAnimation;
	UPROPERTY()
	TSubclassOf<UAnimInstance> ThirdPersonAnimation;

	/** Rounds in the magazine and in reserve: the server's count, sent to the owner. */
	UPROPERTY(Replicated)
	int32 Magazine = 0;
	UPROPERTY(Replicated)
	int32 Reserve = 0;
	/** The server's time the reload under way ends, or 0. */
	UPROPERTY(Replicated)
	float ReloadEndsAt = 0.0f;

	/** The owner's side of firing. */
	bool bTriggerHeld = false;
	bool bAimed = false;
	FTimerHandle FireTimer;
	FTimerHandle ReloadTimer;
	/** When the last shot was fired here, in server time, and which of its run it was; -1 for none. */
	float LastShotAt = -1000.0f;
	int32 LastShotIndex = -1;
	float LastHitAt = -1000.0f;
	/** The server's side: when the shots it allowed were, for the rate; and the last run index it saw. */
	TArray<float> ServerShotTimes;

	UPROPERTY()
	TObjectPtr<ACheeseStrikeCharacter> Holder;
};
