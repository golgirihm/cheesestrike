// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Rules/CheeseStrikeBots.h"
#include "Rules/CheeseStrikeHills.h"
#include "CheeseStrikeBotController.generated.h"

class ACheeseStrikeCharacter;

/**
 * A bot: a player state with a cheese name and a side, a character like any
 * player's, and this, which plays it on the server by the design's bots
 * (docs/unreal/design.md, "Bots"; the numbers in Rules/CheeseStrikeBots.h).
 *
 * Ten times a second it looks: an enemy is anyone alive on the other side
 * (anyone else in free-for-all) and not just come back, in its sight cone (or right beside it) with a clear line to
 * them. It fights the one it was already fighting, else whoever is shooting
 * it, else the nearest; with nobody in sight it turns to whoever shot it or
 * to a shot it heard, goes to where it last saw an enemy, and otherwise
 * roams the navigation mesh toward other players or to somewhere new. Every
 * frame it turns toward where it aims, no faster than its skill allows,
 * with an error that steadies the longer it tracks, and fires in bursts once
 * its reaction time is over and the aim is near enough. In a fight it keeps
 * to its range (MoveFor): it closes in, holds there stepping side to side,
 * steps back from anyone too close, or gets away when hurt. Stuck for a
 * moment, it jumps and goes somewhere else.
 *
 * It moves its character as a player would: path following and movement
 * input for the feet, its control rotation for the view, and the
 * character's own Do* actions for the trigger, sights, sprint, reload and
 * changing weapons. It comes in with a primary suited to its skill, and
 * changes to the sidearm or the knife when they serve better
 * (CheeseStrikeBots::ChooseWeapon): someone close, the magazine empty.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeBotController : public AAIController
{
	GENERATED_BODY()

public:
	ACheeseStrikeBotController();

	/** What it is doing, for tests and the log: "roaming", "searching", "fighting", "retreating", or "idle". */
	const TCHAR* Doing() const;
	/** The enemy it is after, if any, and whether it can see them now. */
	ACheeseStrikeCharacter* GetTarget() const { return Target.Get(); }
	bool SeesTarget() const { return bSeesTarget; }
	/** The skill it plays at (CheeseStrikeBots::Skills). */
	int32 GetSkill() const { return SkillIndex; }
	/** The seconds from first seeing an enemy to its first shot at them, the last time it shot at someone new; -1 before. */
	float GetLastReaction() const { return LastReaction; }

	/** One line on its state, for the log. */
	FString Describe() const;
	/** Turns its view to `Rotation` at once, as on coming in. */
	void FaceTo(const FRotator& Rotation);

	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	ACheeseStrikeCharacter* Body() const;
	float Now() const;
	/** Where it sees from: its view's place. */
	FVector EyesAt() const;
	/** Whether nothing of the world stands between its eyes and `Other`'s head or chest. */
	bool CanSeeInto(const ACheeseStrikeCharacter* Other) const;
	/** The ten-a-second decisions: what it sees, whom it fights, where it goes. */
	void Think();
	void Perceive();
	/** Whether `Other` is on the other side (anyone else in free-for-all): a bot fights, hears and hunts only its enemies. */
	bool IsEnemy(const ACheeseStrikeCharacter* Other) const;
	/**
	 * In Execution: an enemy down in reach it executes at once, with a
	 * press of Interact, whatever else is going on; it goes to execute one
	 * further off as CheeseStrikeBots::GoesToExecute has it (even while
	 * fighting, for one close), from outside their aim; and with no enemy in
	 * sight it goes to a teammate down near it and holds Interact over them
	 * to revive them. True while it is doing any of that, or is in an
	 * execution.
	 */
	bool ThinkAboutTheDown();
	/** How far it goes to revive a teammate down (cm). */
	static constexpr float TendRange = 2500.0f;
	/**
	 * In King of the Hill (CheeseStrikeHills::BotPlan): goes to the hill, or
	 * where it is about to go, fighting on the way at whoever it sees, and
	 * once there holds it, moving about inside it and fighting from it rather
	 * than chasing anyone out of it; only an enemy close by turns it aside
	 * to fight as it would anywhere. True while the hill has its feet.
	 */
	bool ThinkAboutTheHill();
	/** Calls in its next streak, as a player would: the drone and the Overload soon after earning them, the Cheese Strike on an enemy it has just seen. */
	void ThinkAboutStreaks();
	/**
	 * Once a second: whether to throw a grenade at its enemy, most likely at
	 * one just gone behind cover (CheeseStrikeGrenades::BotShouldThrow), and
	 * if so, the throw that lands nearest where they were, worked out along
	 * the grenade's own arc; it turns to that and throws (AimAndFire).
	 */
	void ThinkAboutGrenades();
	/** The way to throw from here for a grenade to come down near `Spot`, if one does (within CheeseStrikeGrenades::BotLandsWithin). */
	bool AimToLandAt(const FVector& Spot, FVector& OutAim) const;
	void ChooseWhereToGo();
	/** Every frame: where it looks, whether it fires, and its side steps. */
	void AimAndFire(float DeltaSeconds);
	void Step(float DeltaSeconds);
	/** Starts going to `Where` on the navigation mesh, if it can; `bSprint` to run; `bRightThere` to stop only right on it (not near it). */
	bool GoTo(const FVector& Where, bool bSprint, bool bRightThere = false);
	/** A spot on the navigation mesh to roam to: toward another player, or somewhere new well away from here. */
	bool PickRoamGoal(FVector& Out) const;
	/** A spot on the navigation mesh away from `From`, to get away to. */
	bool PickRetreatGoal(const FVector& From, FVector& Out) const;
	void LetGoOfTrigger();
	/** Changes to the weapon that serves best now. */
	void ChooseWeapon();
	void ForgetAll();

	int32 SkillIndex = CheeseStrikeBots::DefaultSkill;

	TWeakObjectPtr<ACheeseStrikeCharacter> Target;
	bool bSeesTarget = false;
	/** Whether its target is the one shooting it, lately: then it fires back from however far. */
	bool bShotByTarget = false;
	/** When it began tracking its target (its reaction and its aim's steadying count from here), when it last saw them, and where. */
	float TrackingSince = 0.0f;
	float LastSeenAt = -1000.0f;
	FVector LastSeenWhere = FVector::ZeroVector;
	/** Whether it has shot at its present target yet. */
	bool bShotAtTarget = false;
	float LastReaction = -1.0f;

	/** What its feet are doing in a fight, and where it is going. */
	CheeseStrikeBots::EMove Move = CheeseStrikeBots::EMove::Hold;
	bool bGoing = false;
	bool bSearching = false;
	bool bRetreating = false;
	FVector Goal = FVector::ZeroVector;
	float GoalSetAt = -1000.0f;
	/** When it last made progress, for being stuck. */
	float MovedAt = 0.0f;
	FVector MovedFrom = FVector::ZeroVector;
	/** Which way round it goes to finish someone down (CheeseStrikeBots::FinishApproach): changed when it gets stuck going one way. */
	float FlankSide = 1.0f;

	/** Where it wants to look; the view turns toward this each frame. */
	FRotator Wanted = FRotator::ZeroRotator;
	/** Its aim error's drift, so that the error wanders rather than jumps. */
	float Wobble = 0.0f;
	CheeseStrikeBots::FBursts Bursts;
	bool bTriggerHeld = false;
	float StepSide = 1.0f;
	float NextStepAt = 0.0f;
	float NextThinkAt = 0.0f;
	/** When it last changed weapons, so that it doesn't change back and forth. */
	float ChangedWeaponAt = -1000.0f;
	/** When it next thinks about a grenade, and a throw it has decided on: which way, and since when it has been turning to it. */
	float NextGrenadeThoughtAt = 0.0f;
	bool bThrowPending = false;
	FVector ThrowAim = FVector::ForwardVector;
	float ThrowPendingSince = 0.0f;
	/** When it began holding the streak it has ready; -1 with none. */
	float StreakHeldSince = -1.0f;
	/** What it is doing about the hill (CheeseStrikeHills::EBotPlan), and whether the hill has its feet. */
	CheeseStrikeHills::EBotPlan HillPlan = CheeseStrikeHills::EBotPlan::Fight;
	bool bHill = false;
	/** Whether it is tending someone down, and holding Interact. */
	bool bTending = false;
	bool bInteracting = false;
};
