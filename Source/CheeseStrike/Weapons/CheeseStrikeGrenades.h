// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikeGrenades.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeGrenade;
class UStaticMeshComponent;

/**
 * The cheese grenades a character carries: a count of its own, apart from
 * the weapons and their slots (UCheeseStrikeInventory), so that what is
 * picked up and changed to never touches it. A character comes in with
 * CheeseStrikeGrenades::SpawnCount; a mode may give it others (Gun Game
 * gives none).
 *
 * Throwing (G, or the right bumper) happens at once on the player's own
 * machine, which plays the throw on both its bodies and tells the server
 * which way it threw; the server, if a grenade is left and the hands are
 * free, takes one, has every other machine play the throw, and a moment
 * later (CheeseStrikeGrenades::ReleaseDelay, as the hand comes over) spawns
 * the grenade (ACheeseStrikeGrenade) from the hand at
 * CheeseStrikeGrenades::LaunchVelocity. While the throw goes on
 * (ThrowTime) the weapon in hand doesn't fire or aim.
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

	/** Server only: the grenades it has thrown, still flying, for tests. */
	const TArray<TWeakObjectPtr<ACheeseStrikeGrenade>>& GetThrown() const { return Thrown; }
	/** The server's time it last threw one (-1000 for never). */
	float GetLastThrowAt() const { return LastThrowAt; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
};
