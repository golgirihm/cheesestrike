// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeGrenades.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGrenade;
class ACheeseStrikeWeapon;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FPredictProjectilePathResult;

/**
 * The cheese grenades a character carries: a count of its own, apart from
 * the weapons and their slots (UCheeseStrikeInventory), so that what is
 * picked up and changed to never touches it. A character comes in with
 * CheeseStrikeGrenades::SpawnCount; a mode may give it others (Gun Game
 * gives none).
 *
 * A player holds G (or the right bumper) to throw: the weapon in hand goes
 * down out of view and the grenade comes up in the right hand, the left let
 * go (the switch, CheeseStrikeGrenades::SwitchStep, posed in code by
 * FCheeseStrikeGrenadeHold on both bodies, so that others see it held too),
 * while on the player's own machine the arc it would fly is shown in the
 * world, a dot every so far along it and a mark where it comes down, worked
 * out as the grenade will fly (PredictPath, the same release, speed, gravity,
 * size and what it bounces off). Letting go throws it; changing weapons,
 * reloading, going down, dying or being held by a count puts it away, still
 * carried (CheeseStrikeGrenades::HeldThrow); either way the grenade hand goes
 * down and the weapon comes back up after.
 *
 * The throw itself (Throw, at once, as bots throw; EndHold, as the key is let
 * go) is shown on the player's own machine at once and tells the server which
 * way it went; the server, if a grenade is left and the hands are free, takes
 * one, has every other machine show the throw, and a moment later
 * (CheeseStrikeGrenades::ReleaseDelay, as the hand comes over) spawns the
 * grenade (ACheeseStrikeGrenade) where the right hand lets it go
 * (ReleasePoint) at CheeseStrikeGrenades::LaunchVelocity. Every body throws it
 * overhand with the right hand (CheeseStrikeGrenades::ThrowHand). While a
 * grenade is in hand or the weapon is coming back the weapon doesn't fire or
 * aim.
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
	/** How far a throw shown on this machine is (0 to 1), or below 0 for none. */
	float ThrowPhase() const;
	/** Whether a throw now would go: one left, the hands free, and the character able to act and not down. */
	bool CanThrow() const;
	/** Throws one along `Aim`: on the player's own machine (or a bot's, on the server), which shows it at once and asks the server. */
	void Throw(const FVector& Aim);

	/** The player's own machine: the key is held down; if a throw could go, the grenade is taken up and the arc shows. */
	void BeginHold();
	/** The key is let go: the throw held goes, along `Aim`. */
	void EndHold(const FVector& Aim);
	/** Puts a throw held away, the grenade still carried. */
	void CancelHold();
	/** Whether a throw is held on this machine. */
	bool IsHolding() const { return bHolding; }
	/** Whether a grenade is held up, as this machine knows: its own player's hold, or another's as the server tells it. */
	bool IsHoldShown() const { return bHolding || bHoldShown; }
	/** Whether the hands are busy with a grenade: a throw held or under way, or the weapon not yet back up after one. */
	bool IsBusy() const { return bHolding || IsThrowing() || Switch > 0.0f; }
	/** The switch from the weapon to the grenade and back (CheeseStrikeGrenades::SwitchStep): 0 the weapon up, 2 the grenade. */
	float GetSwitch() const { return Switch; }
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

	/** Where a grenade leaves the right hand: ahead of the eyes, a little right and down (CheeseStrikeGrenades::ReleaseAhead and the rest), or short of a wall right in front of them. */
	FVector ReleasePoint(const FVector& Aim) const;
	/** Where a grenade sits in `Body`'s right palm, in its hand bone's space: from its own knuckles and wrist, whichever way the bone is turned. False if it has no fingers to tell by. */
	static bool InPalm(const USkeletalMeshComponent* Body, FTransform& OutInHand);

	/** The grenade shown in the right hand, held or thrown, in its player's view and on the body others see; null before they are first shown. */
	const UStaticMeshComponent* GetHeldInView() const { return HeldInView; }
	const UStaticMeshComponent* GetInHandThird() const { return InHandThird; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerThrow(FVector_NetQuantizeNormal Aim);
	/** The server's side of a throw: takes a grenade, tells the others, and lets it go after the wind-up. */
	void ServerThrowNow(const FVector& Aim);
	/** Every machine but the thrower's own: shows the throw. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastThrow();
	/** Shows a throw on this machine: the right arm throws (ThrowPhase), the grenade in its hand until it goes. */
	void ShowThrow();
	/** Its own player's machine tells the server a grenade is held up, or no longer, for the others to see. */
	UFUNCTION(Server, Reliable)
	void ServerShowHold(bool bShow);
	void ShowHold(bool bShow);
	/** The grenade in the right hand of a body: made the first time it is wanted. */
	UStaticMeshComponent* MakeInHand(USkeletalMeshComponent* Body, bool bFirstPerson);
	/** Each frame: the grenade in the right hand while it is up or being thrown, until it goes. */
	void UpdateInHand();
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
	/** A grenade held up by its player, for every other machine (its own knows by bHolding). */
	UPROPERTY(Replicated)
	bool bHoldShown = false;
	/** This machine's world time the throw shown here ends, and the grenade in hand goes. */
	float ThrowingUntil = -1.0f;
	float InHandUntil = -1.0f;
	float LastThrowAt = -1000.0f;
	FTimerHandle ReleaseTimer;
	TArray<TWeakObjectPtr<ACheeseStrikeGrenade>> Thrown;
	/** The switch between the weapon and the grenade, on this machine (CheeseStrikeGrenades::SwitchStep). */
	float Switch = 0.0f;

	/** A throw held: the weapon in hand as it began (changing it puts the throw away). */
	bool bHolding = false;
	TWeakObjectPtr<ACheeseStrikeWeapon> HeldWith;
	/** The weapon put out of sight while a grenade is in the right hand, brought back after. */
	TWeakObjectPtr<ACheeseStrikeWeapon> AwayGun;
	void UpdateGunAway();
	/** The grenade in the right hand: in its player's view, and on the body others see. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeldInView;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InHandThird;
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
