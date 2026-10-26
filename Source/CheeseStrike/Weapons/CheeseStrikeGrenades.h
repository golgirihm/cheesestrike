// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeGrenades.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGrenade;
class ACheeseStrikeWeapon;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FPredictProjectilePathResult;

/**
 * The cheese grenades a character carries: a count of its own, apart from
 * the weapons and their slots (UCheeseStrikeInventory), so that what is
 * picked up and changed to never touches it. A character comes in with
 * CheeseStrikeGrenades::SpawnCount; a mode may give it others (Gun Game
 * gives none).
 *
 * A player holds G (or the right bumper) to throw: on their own machine
 * the grenade is taken up in the free hand, low on the left of the view,
 * while the arc it would fly is shown in the world, a dot every
 * so far along it and a mark where it comes down, worked out as the
 * grenade will fly (PredictPath, the same release, speed, gravity, size and
 * what it bounces off). Letting go throws it; changing weapons, reloading,
 * going down, dying or being held by a count puts it away, still carried
 * (CheeseStrikeGrenades::HeldThrow). Nobody else sees a throw held.
 *
 * The throw itself (Throw, at once, as bots throw; EndHold, as the key is let go)
 * plays on both the player's bodies at once and tells the server which way
 * it went; the server, if a grenade is left and the hands are free, takes
 * one, has every other machine play the throw, and a moment later
 * (CheeseStrikeGrenades::ReleaseDelay, as the hand comes over) spawns the grenade (ACheeseStrikeGrenade) from
 * the hand at CheeseStrikeGrenades::LaunchVelocity. While a throw is held
 * or goes on (ThrowTime) the weapon in hand doesn't fire or aim.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeGrenades : public UActorComponent
{
	GENERATED_BODY()

public:
	UCheeseStrikeGrenades();

	/** Grenades carried: the server's count, sent to its player. */
	int32 GetCount() const { return Count; }
	/** Server only: carries `InCount` from now on (no more than CheeseStrikeGrenades::MostCarried). */
	void SetCount(int32 InCount);

	/** Whether a throw is under way on this machine: the weapon in hand waits for it. */
	bool IsThrowing() const;
	/** Whether a throw now would go: one left, the hands free, and the character able to act and not down. */
	bool CanThrow() const;
	/** Throws one along `Aim`: on the player's own machine (or a bot's, on the server), which shows it at once and asks the server. */
	void Throw(const FVector& Aim);

	/** The player's own machine: the key is held down; if a throw could go, the arm winds back and the arc shows. */
	void BeginHold();
	/** The key is let go: the throw held goes, along `Aim`. */
	void EndHold(const FVector& Aim);
	/** Puts a throw held away, the grenade still carried. */
	void CancelHold();
	/** Whether a throw is held on this machine. */
	bool IsHolding() const { return bHolding; }
	/** Whether the hands are busy with a grenade: a throw held or under way. */
	bool IsBusy() const { return bHolding || IsThrowing(); }
	/** The dots of the arc shown now, in the world (none unless a throw is held), and where it comes down. */
	const TArray<FVector>& GetPreview() const { return PreviewAt; }
	bool GetPreviewLanding(FVector& OutAt) const { OutAt = PreviewLanding; return bPreviewLands; }

	/**
	 * The flight of a grenade thrown now along `Aim` from this character,
	 * as ACheeseStrikeGrenade will fly it, up to where it first comes down:
	 * true if it does within its fuse. For the arc shown, and for bots
	 * working out a throw.
	 */
	bool PredictPath(const FVector& Aim, FPredictProjectilePathResult& OutResult, float SimFrequency = 30.0f) const;

	/** Server only: the grenades it has thrown, still flying, for tests. */
	const TArray<TWeakObjectPtr<ACheeseStrikeGrenade>>& GetThrown() const { return Thrown; }
	/** The server's time it last threw one (-1000 for never). */
	float GetLastThrowAt() const { return LastThrowAt; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Where a grenade leaves the hand: ahead of the eyes, a little right and down, or short of a wall right in front of them. */
	FVector ReleasePoint(const FVector& Aim) const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerThrow(FVector_NetQuantizeNormal Aim);
	/** The server's side of a throw: takes a grenade, tells the others, and lets it go after the wind-up. */
	void ServerThrowNow(const FVector& Aim);
	/** Every machine but the thrower's own: shows the throw. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastThrow();
	/** Shows a throw on this machine: both bodies play it (the set's Throw), the round in hand until it goes. */
	void ShowThrow();
	/** A throw held: the grenade up in its player's view, or put away. */
	void ShowHeldInView(bool bShow);
	/** Puts the round in the throwing hand on both bodies. */
	void ShowInHand();
	/** Each frame of a throw held: puts it away or throws it, and draws its arc. */
	void UpdateHold();
	void ShowPreview(bool bShow);
	/** Server: the grenade leaves the hand. */
	void Release(FVector Aim);
	ACheeseStrikeCharacter* Holder() const;
	float ServerNow() const;

	/** Everyone comes in with the usual; a mode may change it in its SetPlayerDefaults. */
	UPROPERTY(Replicated)
	int32 Count = CheeseStrikeGrenades::SpawnCount;
	/** This machine's world time the throw shown here ends, and the round in hand goes. */
	float ThrowingUntil = -1.0f;
	float InHandUntil = -1.0f;
	float LastThrowAt = -1000.0f;
	FTimerHandle ReleaseTimer;
	TArray<TWeakObjectPtr<ACheeseStrikeGrenade>> Thrown;
	/** The round shown in the throwing hand while the throw winds up, in its player's view and on the body. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InHandFirst;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InHandThird;

	/** A throw held: the weapon in hand as it began (changing it puts the throw away), and the grenade shown up in view. */
	bool bHolding = false;
	TWeakObjectPtr<ACheeseStrikeWeapon> HeldWith;
	/** The gun put away from view while a grenade is held or thrown, brought back after. */
	TWeakObjectPtr<ACheeseStrikeWeapon> AwayGun;
	void UpdateGunAway();
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldInView;
	/** The arc shown: its dots and the mark where it comes down, seen by its player alone. */
	TArray<FVector> PreviewAt;
	FVector PreviewLanding = FVector::ZeroVector;
	bool bPreviewLands = false;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PreviewDots;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PreviewMark;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewGlow;
};
