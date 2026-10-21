// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeMouse.generated.h"

class ACheeseStrikeCheese;
class UAnimSequence;
class UAudioComponent;
class UBoxComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;

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
 * fifteen times a rat's size (Content/Fab/Nestaeric/BlackRat), and it squeaks,
 * gnaws and crunches, and screams when it runs, pitched down for its size
 * (Content/OpenGameArt/Mice).
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
	/** Whether it found all its sounds. */
	bool HasSounds() const;
	/** Whether it has its body, animations and every material of its body. */
	bool HasBody() const;

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
	/** A way to somewhere beside `Cheese` it can eat it from (not on top of it), into Path; false if there is none. */
	bool FindWayBeside(const ACheeseStrikeCheese* Cheese);
	/** Whether it can reach `Cheese` from where it is. */
	bool CanReach(const ACheeseStrikeCheese* Cheese) const;
	void RunFrom(const FVector& Danger);
	/** Keeps its feet on what is under it (rising a little, as up a ramp), falling when what it stood on is eaten. */
	void KeepOnGround(float DeltaSeconds);
	UFUNCTION()
	void OnRep_Doing();
	/** The animation and the sounds for what it is doing, on this machine. */
	void ShowDoing();
	/** Its squeaks now and then, and its crunches while it eats, on this machine. */
	void Sound(float DeltaSeconds);
	/** Plays `Sound` from `Speaker`, at `Volume` and a pitch around `Around`. */
	static void Say(UAudioComponent* Speaker, USoundBase* Sound, float Volume, float Around);

	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<UBoxComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	/**
	 * Its body and animations, loaded when it comes into play rather than
	 * found in the constructor: what a constructor loads at the editor's
	 * start is rooted for good, and the rat's materials could then never be
	 * rebuilt by tools/art/build_materials.py.
	 */
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> Model;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Walk;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Run;
	UPROPERTY()
	TSoftObjectPtr<UAnimSequence> Sniff;
	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> RunAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> EatAnimation;
	/** Its gnawing while it eats (looping), its crunches, and its voice. */
	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<UAudioComponent> Gnawing;
	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<UAudioComponent> Mouth;
	UPROPERTY(VisibleAnywhere, Category="Mouse")
	TObjectPtr<UAudioComponent> Voice;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Squeaks;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Crunches;
	UPROPERTY()
	TObjectPtr<USoundBase> Scream;

	UPROPERTY(ReplicatedUsing=OnRep_Doing)
	EMouseDoing Doing = EMouseDoing::Entering;
	EMouseDoing Shown = EMouseDoing::Entering;
	bool bShownOnce = false;
	/** When it next squeaks, and next crunches while it eats, on this machine. */
	float NextSqueakAt = 0.0f;
	float NextCrunchAt = 0.0f;

	TWeakObjectPtr<ACheeseStrikeCheese> Target;
	/** Cheese it gave up on, and when it may try it again. */
	TMap<TWeakObjectPtr<ACheeseStrikeCheese>, float> GivenUp;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	/** When it found its way. */
	float PathFoundAt = 0.0f;
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
