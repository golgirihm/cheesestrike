// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/CheeseStrikeModes.h"
#include "CheeseStrikeLastStand.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikePlayerState;

/**
 * A character's last stand, in Execution (CheeseStrikeModes): hurt enough
 * to die, once a life, it goes down instead. Down, it lies on its back,
 * propped on an elbow (FCheeseStrikeDownedPose; its capsule crouched, its
 * legs out ahead of it), aims only in a cone toward its feet, scoots slowly
 * and turns round slowly (CheeseStrikeDowned), can't sprint, jump or aim
 * down the sights, carries only its sidearm, and nothing harms it but an
 * enemy within reach (CheeseStrikeModes::FinishRange), whose next hit
 * finishes it; held Interact (E) finishes it too from an enemy, or from a
 * teammate gets it back up; left alone it bleeds out, the kill of whoever
 * put it down. Interact is this component's too: held near someone down,
 * it works on them while it is held and nobody moves out of reach.
 *
 * The server decides; every machine sees whether it is down, until when,
 * and whom its player is working on, for the HUD.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeLastStand : public UActorComponent
{
	GENERATED_BODY()

public:
	UCheeseStrikeLastStand();

	bool IsDowned() const { return bDowned; }
	/** Seconds left before bleeding out, while down; 0 otherwise. */
	float SecondsToBleedOut() const;
	/** Whom its player is reviving or finishing just now, and how far through (0 to 1); null for nobody. */
	ACheeseStrikeCharacter* GetWorkingOn() const { return WorkingOn; }
	CheeseStrikeModes::EInteract GetWork() const { return static_cast<CheeseStrikeModes::EInteract>(Work); }
	float WorkDone() const;
	/** Who is working on this one, if anyone: the teammate reviving it or the enemy finishing it. */
	ACheeseStrikeCharacter* WorkedOnBy() const;
	/** Whom Interact would work on now, and how; null for nobody (for the HUD's prompt and for bots). */
	ACheeseStrikeCharacter* InteractTarget(CheeseStrikeModes::EInteract& OutKind) const;

	/**
	 * Server: what becomes of `Damage` from `From` (or a streak of `By`'s)
	 * in a mode with a last stand. True if it is dealt with here, in
	 * `bOutHurt` whether it counted as a hit; false if it is to be taken as
	 * usual, `Damage` raised to finish a player down.
	 */
	bool Intercept(float& Damage, const ACheeseStrikeCharacter* From, ACheeseStrikePlayerState* By, bool& bOutHurt);
	/** Its player holds Interact, or lets go: on its own machine, which tells the server; a bot's on the server. */
	void SetInteracting(bool bHeld);
	/** Server: gets up again, revived. */
	void Revive();

	/** For the play tests: down now, as if hurt enough by `By`. */
	void GoDown(ACheeseStrikePlayerState* By);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerInteract(bool bHeld);
	UFUNCTION()
	void OnRep_Downed();

	ACheeseStrikeCharacter* Body() const;
	float ServerNow() const;
	/** Whether the match has a last stand. */
	bool HasLastStand() const;
	/** Server: finishes it, as `By`'s kill (from `From`'s hand if they have one), or bleeds out. */
	void Finish(ACheeseStrikePlayerState* By);
	/** Server: what Interact does this frame. */
	void DoWork(float DeltaTime);
	void StopWork();
	/** Every machine its player is on: keeps it crouched, not sprinting and not aimed down the sights while down. */
	void KeepDown();

	UPROPERTY(ReplicatedUsing=OnRep_Downed)
	bool bDowned = false;
	UPROPERTY(Replicated)
	float BleedOutAt = 0.0f;
	UPROPERTY(Replicated)
	TObjectPtr<ACheeseStrikeCharacter> WorkingOn;
	UPROPERTY(Replicated)
	uint8 Work = 0;
	UPROPERTY(Replicated)
	float WorkStartedAt = 0.0f;

	/** Server: whether the last stand has been used this life, who put it down, whether Interact is held, and whether a finishing blow is on its way. */
	bool bUsed = false;
	TWeakObjectPtr<ACheeseStrikePlayerState> DownedBy;
	bool bInteractHeld = false;
	bool bFinishing = false;
};
