// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CheeseStrikeHung.generated.h"

class ULightComponent;
class USceneComponent;

/**
 * What a map hangs in its cheese (a door in a wall of cheese, a lamp under a
 * roof of it, a sign on it: tagged by the map's script with the structure it
 * belongs to, CheeseStrikeMice::HungOnTag) goes with it: once every block of
 * that structure is eaten, it shrinks and falls away and is gone, its
 * collision too (CheeseStrikeMice::IsHungOnEatenCheese, VanishScale). Every
 * machine does this itself from the cheese it has (whose blocks go on every
 * machine as they are eaten), so nothing more is sent.
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
	};
	/** Finds the tagged actors (once the level's actors are there). */
	void Gather();
	/** Starts `Each` going: what it is made of stops moving by itself and can move. */
	void StartGoing(FHung& Each);
	/** Shrinks it, drops it and dims it, by how long it has been going; at the end hides it. */
	void ShowGoing(FHung& Each);

	TArray<FHung> Hung;
	bool bGathered = false;
	/** When the structures' cheese was last counted (the world's time). */
	float CountedAt = -1.0f;
};
