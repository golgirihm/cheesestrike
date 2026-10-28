// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/CheeseStrikeModes.h"
#include "Rules/CheeseStrikeExecution.h"
#include "CheeseStrikeLastStand.generated.h"

class ACameraActor;
class ACheeseStrikeCharacter;
class ACheeseStrikePlayerState;

/** An execution under way, as each of the two in it has it. */
USTRUCT()
struct FCheeseStrikeExecutionState
{
	GENERATED_BODY()

	/** The one executed, for the executor; the executor, for the one executed. */
	UPROPERTY()
	TObjectPtr<ACheeseStrikeCharacter> Other;
	/** CheeseStrikeExecution::EKind, as a number; 0 for none. */
	UPROPERTY()
	uint8 Kind = 0;
	/** Whether this one is the executor. */
	UPROPERTY()
	bool bExecutor = false;
	/** Which side of the one down the executor is on (1 right, -1 left). */
	UPROPERTY()
	int8 Side = 1;
	/** The server's time it began. */
	UPROPERTY()
	float StartedAt = 0.0f;
	/** For the executor: where it stands and faces to deal the blow. */
	UPROPERTY()
	FVector_NetQuantize Spot = FVector::ZeroVector;
	UPROPERTY()
	float Yaw = 0.0f;

	CheeseStrikeExecution::EKind GetKind() const { return CheeseStrikeExecution::KindAt(Kind); }
};

/**
 * A character's last stand, in Execution (CheeseStrikeModes): hurt enough
 * to die, it goes down instead (every time: up again, revived or by
 * itself, it can go down again). Down, it lies on its back,
 * propped on an elbow (FCheeseStrikeDownedPose; its capsule crouched, its
 * legs out ahead of it), aims only in a cone toward its feet, scoots slowly
 * and turns round slowly (CheeseStrikeDowned), can't sprint, jump or aim
 * down the sights, carries only its sidearm, which barely hurts anyone
 * (CheeseStrikeModes::DownedWeapon), and sees the world dark and blurred,
 * its view swaying. Nothing harms it but an enemy within reach
 * (CheeseStrikeModes::FinishRange), whose next hit finishes it. A press of
 * Interact (E) from an enemy over it executes it (CheeseStrikeExecution:
 * a knife stab or a head stomp, a couple of seconds long, which nothing
 * interrupts: neither of the two can be hurt while it plays); held Interact
 * from a teammate gets it back up. It stays down until one of those, until
 * SelfReviveTime has passed and it gets up by itself, or until the round
 * ends; a side with everyone down is still in.
 *
 * The server decides; every machine sees whether it is down, since when,
 * whom its player is working on and any execution it is in, for the HUD,
 * the pose and the views.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeLastStand : public UActorComponent
{
	GENERATED_BODY()

public:
	UCheeseStrikeLastStand();

	bool IsDowned() const { return bDowned; }
	/** The server's time it went down, and the seconds until it gets up by itself (CheeseStrikeModes::SelfReviveTime). */
	float GetDownedAt() const { return DownedAt; }
	float SelfReviveLeft() const;
	/** Whom its player is reviving just now, and how far through (0 to 1); null for nobody. */
	ACheeseStrikeCharacter* GetWorkingOn() const { return WorkingOn; }
	CheeseStrikeModes::EInteract GetWork() const { return static_cast<CheeseStrikeModes::EInteract>(Work); }
	float WorkDone() const;
	/** Who is working on this one, if anyone: the teammate reviving it. */
	ACheeseStrikeCharacter* WorkedOnBy() const;
	/** Whom Interact would work on now, and how; null for nobody (for the HUD's prompt and for bots). */
	ACheeseStrikeCharacter* InteractTarget(CheeseStrikeModes::EInteract& OutKind) const;

	/** The execution it is in, executing or executed: the other one, which kind, and how far in (seconds); none outside one. */
	const FCheeseStrikeExecutionState& GetExecution() const { return Execution; }
	CheeseStrikeExecution::EKind GetExecutionKind() const { return Execution.GetKind(); }
	float ExecutionElapsed() const;
	/** Whether it is in an execution still playing: as the executor, or as the one executed. */
	bool InExecution() const;
	bool IsExecuting() const { return InExecution() && Execution.bExecutor; }
	bool IsBeingExecuted() const { return InExecution() && !Execution.bExecutor; }
	/**
	 * Whether it was killed by an execution (on every machine, and still on
	 * its body once dead): for the HUD's "You got Cheesed".
	 */
	bool WasExecuted() const { return bExecuted; }

	/**
	 * Server: what becomes of `Damage` from `From` (or a streak of `By`'s)
	 * in a mode with a last stand. True if it is dealt with here, in
	 * `bOutHurt` whether it counted as a hit; false if it is to be taken as
	 * usual, `Damage` raised to finish a player down. Nothing hurts either
	 * of two in an execution.
	 */
	bool Intercept(float& Damage, const ACheeseStrikeCharacter* From, ACheeseStrikePlayerState* By, bool& bOutHurt);
	/** Its player presses Interact, or lets go: on its own machine, which tells the server; a bot's on the server. */
	void SetInteracting(bool bHeld);
	/** Server: gets up again, revived, with `Health`. */
	void Revive(float Health = CheeseStrikeModes::ReviveHealth);
	/** Server: executes `Victim`, an enemy down within reach, the `Kind` given or one at random. False if it can't. */
	bool Execute(ACheeseStrikeCharacter* Victim, CheeseStrikeExecution::EKind Kind = CheeseStrikeExecution::EKind::None);

	/** For the play tests: down now, as if hurt enough by `By`. */
	void GoDown(ACheeseStrikePlayerState* By);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION(Server, Reliable)
	void ServerInteract(bool bHeld);
	UFUNCTION()
	void OnRep_Downed();
	UFUNCTION()
	void OnRep_Execution();

	ACheeseStrikeCharacter* Body() const;
	float ServerNow() const;
	/** Whether the match has a last stand. */
	bool HasLastStand() const;
	/** Server: finishes it, as `By`'s kill (from their hand if they have one; from nowhere with nobody); by an execution if `bByExecution`. */
	void Finish(ACheeseStrikePlayerState* By, bool bByExecution = false);
	/** Server: what held Interact does this frame (reviving). */
	void DoWork(float DeltaTime);
	void StopWork();
	/** Server: the execution's course: the kill when it comes, the end. */
	void RunExecution();
	/** Server: done with it, this one and (if still in it) the other. */
	void EndExecution();
	/** Every machine its player is on: keeps it crouched, not sprinting and not aimed down the sights while down. */
	void KeepDown();
	/** Its own player's machine: the view, out to watch an execution it is in and back; down, dark, blurred and swaying. */
	void UpdateView(float DeltaTime);
	/**
	 * Every machine: the blows' sounds, and the stomp's burst of the head into cheese, once (played by the one executed's);
	 * and the cheese the wound bleeds (CheeseStrikeBlood): the stab's spurts and gush, the stomp's firehose and its pool.
	 */
	void UpdateBurst(float DeltaTime);
	void Bleed(float DeltaTime);
	/** Server, the executor: logs where its hands and knife are in the body of the one down (CheeseStrike.TraceExecution). */
	void TraceExecution() const;
	/** The executor (on the machine that moves it): where the execution has it stand. */
	void HoldExecutionSpot();

	UPROPERTY(ReplicatedUsing=OnRep_Downed)
	bool bDowned = false;
	UPROPERTY(Replicated)
	float DownedAt = 0.0f;
	UPROPERTY(Replicated)
	TObjectPtr<ACheeseStrikeCharacter> WorkingOn;
	UPROPERTY(Replicated)
	uint8 Work = 0;
	UPROPERTY(Replicated)
	float WorkStartedAt = 0.0f;
	UPROPERTY(ReplicatedUsing=OnRep_Execution)
	FCheeseStrikeExecutionState Execution;
	UPROPERTY(Replicated)
	bool bExecuted = false;

	/** Server: who put it down, whether Interact is held, whether a finishing blow is on its way, and whether the execution's kill is done. */
	TWeakObjectPtr<ACheeseStrikePlayerState> DownedBy;
	bool bInteractHeld = false;
	bool bFinishing = false;
	bool bExecutionKilled = false;
	/** Server: the executor's weapon before the knife came out for a stab. */
	uint8 SlotBefore = 0;
	bool bSlotBefore = false;

	/** This machine: the camera watching an execution, and when the view comes back from it; whether the head has burst here. */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ExecutionCamera;
	bool bViewOut = false;
	bool bViewToBlend = false;
	/** Where the one down lay, and which way, as the view went out: the camera keeps to it as their body falls. */
	FVector ViewFloor = FVector::ZeroVector;
	float ViewYaw = 0.0f;
	float ViewBackAt = -1.0f;
	bool bBurst = false;
	int32 BeatsDone = 0;
	/** The drops the wound owes (its rate times the time, the fraction carried on), and whether the stomp's pool is down. */
	float BleedOwed = 0.0f;
	bool bPooled = false;
	TWeakObjectPtr<class APlayerController> ViewPlayer;
	/** This machine: the sway last put on the view while down, and its clock. */
	FRotator SwayShown = FRotator::ZeroRotator;
	float SwayClock = 0.0f;
	/** This machine: how dark and blurred the view is, 0 to 1. */
	float HurtView = 0.0f;
};
