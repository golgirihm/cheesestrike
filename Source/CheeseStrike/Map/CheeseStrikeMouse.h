// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeMouse.generated.h"

class ACheeseStrikeCheese;
class UAnimSequence;
class UBoxComponent;
class USkeletalMeshComponent;

/** What a mouse is doing, as every machine shows it. */
UENUM()
enum class EMouseDoing : uint8
{
	/** Coming over the edge of the map. */
	Entering,
	/** Going to cheese, or looking for some. */
	Walking,
	Eating,
	/** Scared off by shots. */
	Fleeing,
};

/**
 * A giant mouse (docs/unreal/design.md, "The mice"; the numbers in
 * Rules/CheeseStrikeMice.h): it comes over the edge of the map, goes for the
 * cheapest cheese (the nearest, low down, and not another mouse's), walks
 * the navigation mesh to it and eats it bite by bite; shot enough, it runs
 * off for a while and comes back; making no progress on a block, it leaves
 * it alone for a while. It hurts nobody and blocks nobody, but shots hit it.
 *
 * The server runs it; every machine sees it move (replicated movement) and
 * animates it by what it is doing. Its body is Nestaeric's Black Rat at
 * fifteen times a rat's size (Content/Fab/Nestaeric/BlackRat).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeMouse : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeMouse();

	/** Server only: comes in over the edge, from `From` (on top of the map's wall) down to the ground at `To`. */
	void Enter(const FVector& From, const FVector& To);

	EMouseDoing GetDoing() const { return Doing; }
	ACheeseStrikeCheese* GetTarget() const { return Target.Get(); }
	float GetFear() const { return Fear; }
	/** How many blocks it has finished. */
	int32 GetEaten() const { return Eaten; }

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	float Now() const;
	/** Chooses the cheapest cheese it can walk to, and the way there. */
	void ChooseCheese();
	/** A way on the navigation mesh to `To`, into Path; false if there is none. */
	bool FindWay(const FVector& To);
	/** Walks along Path at `Speed`, turning as it goes. True once at its end. */
	bool FollowPath(float Speed, float DeltaSeconds);
	/** Turns toward `Direction` (flat), no faster than a mouse turns. */
	void TurnToward(const FVector& Direction, float DeltaSeconds);
	/** Whether it can reach `Cheese` from where it is. */
	bool CanReach(const ACheeseStrikeCheese* Cheese) const;
	void RunFrom(const FVector& Danger);
	/** Keeps its feet on what is under it (rising a little, as up a ramp), falling when what it stood on is eaten. */
	void KeepOnGround(float DeltaSeconds);
	UFUNCTION()
	void OnRep_Doing();
	/** The animation for what it is doing, on this machine. */
	void ShowDoing();

	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> RunAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> EatAnimation;

	UPROPERTY(ReplicatedUsing=OnRep_Doing)
	EMouseDoing Doing = EMouseDoing::Entering;
	EMouseDoing Shown = EMouseDoing::Entering;
	bool bShownOnce = false;

	TWeakObjectPtr<ACheeseStrikeCheese> Target;
	/** Cheese it gave up on, and when it may try it again. */
	TMap<TWeakObjectPtr<ACheeseStrikeCheese>, float> GivenUp;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	FVector EnterFrom = FVector::ZeroVector;
	FVector EnterTo = FVector::ZeroVector;
	float EnteredFor = 0.0f;
	float Fear = 0.0f;
	float FleeUntil = 0.0f;
	float NextBiteAt = 0.0f;
	float NextChoiceAt = 0.0f;
	/** When it last made progress on its cheese (closer to it, or a bite), and how close it had come. */
	float ProgressAt = 0.0f;
	float Closest = 0.0f;
	int32 Eaten = 0;
	float FallSpeed = 0.0f;
};
