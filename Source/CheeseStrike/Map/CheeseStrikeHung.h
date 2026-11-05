// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CheeseStrikeHung.generated.h"

class ACheeseStrikeCheese;
class ULightComponent;
class USceneComponent;

/**
 * What a map hangs in its cheese (a door in a wall of cheese, a lamp under a
 * roof of it, a sign on it: tagged by the map's script with the structure it
 * belongs to, CheeseStrikeMice::HungOnTag) goes with the cheese round it: as
 * the level starts it finds the blocks of that structure touching or within
 * CheeseStrikeMice::HungReach of the thing's bounds (IsRoundHung), and once
 * all of those are eaten or have fallen away (a thing with none round it,
 * once every block of the structure is), it shrinks and falls away and is
 * gone, its collision too (CheeseStrikeMice::IsHungOnEatenCheese,
 * VanishScale). Every machine does this itself from the cheese it has (whose
 * blocks go on every machine as they are eaten, and fall as replicated
 * movement has them), so nothing more is sent.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeHungSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Whether `Actor`, hung on a structure, has gone with it (or is going). */
	bool IsGone(const AActor* Actor) const;
	/** The things hung on cheese in this world, and the structure each is hung on. */
	TArray<TPair<TWeakObjectPtr<AActor>, FName>> GetHung() const;
	/** How many blocks of cheese were round `Actor` as the level started, and how many of those are left (for the tests and the log). */
	int32 GetRound(const AActor* Actor) const;
	int32 GetRoundLeft(const AActor* Actor) const;

private:
	/** A thing hung on cheese, and how it was before it began to go. */
	struct FHung
	{
		TWeakObjectPtr<AActor> Actor;
		FName Structure;
		/** When it began to go (the world's time), or below 0 while it stays. */
		float GoneAt = -1.0f;
		bool bDone = false;
		/** Its root's place and size before it began to go. */
		FTransform Was;
		TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> Lights;
		/** Its bounds as the level started, and the blocks of its structure round them then (CheeseStrikeMice::IsRoundHung). */
		FBox Bounds;
		TArray<TWeakObjectPtr<ACheeseStrikeCheese>> Round;
	};
	/** Finds the tagged actors (once the level's actors are there), and the cheese round each. */
	void Gather();
	/** How many of the blocks round `Each` are still there and within reach. */
	static int32 RoundLeft(const FHung& Each);
	/** Starts `Each` going: what it is made of stops moving by itself and can move. */
	void StartGoing(FHung& Each);
	/** Shrinks it, drops it and dims it, by how long it has been going; at the end hides it. */
	void ShowGoing(FHung& Each);

	TArray<FHung> Hung;
	bool bGathered = false;
	/** When the structures' cheese was last counted (the world's time). */
	float CountedAt = -1.0f;
};
