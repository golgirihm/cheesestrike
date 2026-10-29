// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "GameFramework/Actor.h"
#include "Rules/CheeseStrikeMenuMouseRules.h"
#include "CheeseStrikeMenuMouse.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Plays the menu mouse's walk or sniff, crossfading between them, and turns its head (FCheeseStrikeMenuMouseProxy). */
struct FCheeseStrikeMenuMouseProxy : public FAnimInstanceProxy
{
	FCheeseStrikeMenuMouseProxy() = default;
	explicit FCheeseStrikeMenuMouseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void CacheBones() override;

protected:
	FAnimNode_SequencePlayer_Standalone First;
	FAnimNode_SequencePlayer_Standalone Second;
	FAnimNode_TwoWayBlend Blend;
	bool bLinked = false;
	/** What the instance asks for, copied in on the game thread. */
	TObjectPtr<UAnimSequence> Wanted;
	float WantedRate = 1.0f;
	float HeadYaw = 0.0f;
	/** Which player shows the sequence wanted (false: First), and how far the blend is toward Second. */
	bool bOnSecond = false;
	TObjectPtr<UAnimSequence> Playing;
	float Alpha = 0.0f;
	/** The bones the head turns by, from the skull up its neck. */
	TArray<FCompactPoseBoneIndex> HeadBones;
};

/** The menu mouse's animation: one sequence at a time (SetSequence), and the head turned about the up axis (SetHeadYaw). */
UCLASS(Transient)
class CHEESESTRIKE_API UCheeseStrikeMenuMouseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetSequence(UAnimSequence* InSequence, float InPlayRate) { Sequence = InSequence; PlayRate = InPlayRate; }
	void SetHeadYaw(float Degrees) { HeadYaw = Degrees; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FCheeseStrikeMenuMouseProxy;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Sequence;
	float PlayRate = 1.0f;
	float HeadYaw = 0.0f;
	FCheeseStrikeMenuMouseProxy Proxy;
};

/**
 * The mouse on the front end: now and then (CheeseStrikeMenuMouse's
 * schedule) the mice's Black Rat walks in from one side behind the figure
 * (ACheeseStrikeMenuFigure), stops, sniffs, turns its head toward a wheel of
 * cheese put out for it, looks a moment and walks off the other side. Made
 * on each machine with the figure for its own menu, never replicated, never
 * in a match. Its model and animations are loaded by path in BeginPlay, as
 * the mice's are (ACheeseStrikeMouse). `CheeseStrikeMenuMouse now` on the
 * console brings it at once.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeMenuMouse : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeMenuMouse();

	/** Where it walks, from the figure's pedestal (its actor's place), centimetres: across behind it, from the view's right to its left. */
	static const FVector PathFrom;
	static const FVector PathStop;
	static const FVector PathTo;
	/** Where its cheese sits on the floor, and how big the wheel is. */
	static const FVector CheeseAt;
	static constexpr float CheeseWidth = 34.0f;
	static constexpr float CheeseHeight = 14.0f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Starts a visit now (the console's `CheeseStrikeMenuMouse now`). */
	void ComeNow();
	/** What it is doing now. */
	CheeseStrikeMenuMouse::EPhase GetPhase() const { return Moment.Phase; }
	/** Seconds until the next visit, while away. */
	float GetNextIn() const { return NextAt - Clock; }

protected:
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Stage;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Cheese;

	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> Model;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Walk;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Sniff;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Stand;
	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> Wheel;
	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> WheelLook;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SniffAnimation;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> StandAnimation;

private:
	/** Puts the mouse where `Moment` has it. */
	void Place();

	/** Seconds since it began, the visit's start (negative while away), and when the next starts. */
	float Clock = 0.0f;
	float StartedAt = -1.0f;
	float NextAt = 0.0f;
	CheeseStrikeMenuMouse::FMoment Moment;
};
