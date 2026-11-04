// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeMenuFigure.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTexture;

/**
 * The figure on the front end: the player's hero standing with the Cheddar
 * Blaster in hand (placed as a character holds it, CheeseStrikeWeaponLooks)
 * on a low round pedestal in the middle of the view, turning slowly. Made on
 * each machine for its own menu, never replicated. Every hero's meshes,
 * materials and textures are held and kept resident while it exists, so that
 * a hero switched to is whole and sharp from its first frame; the switch
 * itself shows nothing out of place (ShowHero).
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeMenuFigure : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeMenuFigure();

	/** Degrees a second it turns. */
	static constexpr float TurnRate = 18.0f;
	/** The pedestal's width and height, in centimetres. */
	static constexpr float PedestalWidth = 130.0f;
	static constexpr float PedestalHeight = 8.0f;
	/** The longest the figure waits for its textures to stream in before it counts as ready anyway (IsReady), in seconds. */
	static constexpr float TexturesWaitSeconds = 3.0f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Wears hero `Hero` of CheeseStrikeBodies::All (any other the default), in its own colours, at once, still turning.
	 * Nothing of the old body or the new, nor the weapon, shows out of place for a frame: the body is hidden through the
	 * swap, its built-in guns hidden and the weapon put in its hand, posed once, and shown again on the next tick.
	 */
	void ShowHero(int32 Hero);
	/** The hero it wears. */
	int32 GetHero() const { return Shown; }
	/** Whether the body shows (not hidden through a change of hero). */
	bool IsShown() const;
	/** The body, and the weapon in its right hand. */
	const USkeletalMeshComponent* GetBody() const { return Body; }
	const UStaticMeshComponent* GetWeapon() const { return Weapon; }
	/** Whether every texture of the hero shown is fully streamed in. */
	bool AreTexturesResident() const;
	/** Whether the figure is ready to be looked at: its textures resident, or TexturesWaitSeconds gone by waiting for them. The loading cover waits on it (ACheeseStrikePlayerController::IsViewReady). */
	bool IsReady() const;

protected:
	/** Every hero's mesh, in CheeseStrikeBodies::All's order. */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> Heroes;
	int32 Shown = 0;
	/** Hidden through a change of hero: how many ticks since; INDEX_NONE while shown. */
	int32 HiddenTicks = INDEX_NONE;
	/** When it began, by the PC's clock (IsReady's wait). */
	double BegunAt = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Pedestal;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Weapon;
	/**
	 * Every hero's body besides, tiny inside the pedestal where nothing sees it, drawn by the renderer all the same: so
	 * that the shaders and pipeline states of every hero's materials are made before one is switched to (a mesh first
	 * drawn after a switch showed grey, in the default material, until they were), and its textures asked for.
	 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> WarmBodies;

	/** Hides the hero shown's own guns (its weapon slots and bones) and puts the weapon in its hand at its grip. */
	void ArmHero();
	/** Makes WarmBodies. */
	void WarmUpHeroes();
	/** The textures of hero `Hero`'s materials. */
	TArray<UTexture*> TexturesOf(int32 Hero) const;
	/** Every hero's textures kept resident, every mip loaded (`bKeep`), or let go. */
	void KeepTexturesResident(bool bKeep);
};
