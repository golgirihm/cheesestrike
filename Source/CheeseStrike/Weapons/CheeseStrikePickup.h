// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CheeseStrikePickups.h"
#include "CheeseStrikePickup.generated.h"

class ACheeseStrikeCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * A weapon's stand on a map (Rules/CheeseStrikePickups.h has its rules): a
 * low dark plinth with a pad on it that glows in the colour of what its
 * weapon is worth, and the weapon floating over it, bobbing and turning.
 * The maps' scripts place them (tools/art/map_kit.py's `pickup`), its
 * place being the middle of its floor, and name the weapon.
 *
 * The server owns it: it takes the weapon for anyone who walks over it
 * and may (UCheeseStrikeInventory::TakePickup), and for anyone who
 * presses Interact near it, asked through their inventory; taken, it is
 * empty for CheeseStrikePickups::RespawnSeconds, until the replicated
 * BackAt, and every machine shows its weapon as a ghost and its pad dim
 * meanwhile, the HUD the seconds over it. A new round (Elimination,
 * Execution) puts every weapon back. In Gun Game it is hidden and gives
 * nothing. It blocks nothing: players walk through it and shots pass it.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikePickup : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikePickup();

	/** The weapon it holds: a primary's id (CheeseStrikeArsenal). */
	FName GetWeapon() const { return Weapon; }
	/** Before it is spawned or placed: the weapon it holds. */
	void SetWeapon(FName Id);
	/** The weapon's name, as players read it. */
	FText GetWeaponName() const;

	/** Whether the match's mode has pickups (not Gun Game): otherwise it is hidden and gives nothing. */
	bool IsInPlay() const;
	/** Whether its weapon is there to take: in play and not taken, or back. */
	bool IsThere() const;
	/** Seconds until its weapon is back; 0 if it is there. */
	float SecondsLeft() const;
	/** Where its floor is: its place. */
	FVector FloorAt() const { return GetActorLocation(); }
	/** How `Character` would take its weapon now, by what it carries (none if it is not there, or `Character` may not). */
	CheeseStrikePickups::ETake HowFor(const ACheeseStrikeCharacter* Character) const;
	/** Whether `Character`'s feet are within `Across` cm of its floor (and near it in height). */
	bool IsInReach(const ACheeseStrikeCharacter* Character, float Across) const;
	/** The stand whose weapon `Character` would take by pressing Interact now: the nearest in reach, or null. */
	static ACheeseStrikePickup* OfferedTo(const ACheeseStrikeCharacter* Character);

	/** How it is shown on this machine, for the log (`CheeseStrikePickups`). */
	FString DescribeLook() const;

	/** Server: its weapon has been taken; it is back after RespawnSeconds. */
	void Take();
	/** Server: its weapon back at once. */
	void PutBack();

	//~ AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

protected:
	/** The weapon it holds, as the map names it. */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Weapon, Category = "Pickup")
	FName Weapon;
	/** When its weapon is back, by the server's clock; 0 while it is there. */
	UPROPERTY(ReplicatedUsing=OnRep_BackAt)
	float BackAt = 0.0f;

	UFUNCTION()
	void OnRep_Weapon();
	UFUNCTION()
	void OnRep_BackAt();

	float ServerNow() const;
	/** Puts its weapon's model over the pad, in its own materials. */
	void ApplyLook();
	/** Shows it as it is now: hidden out of play, ghostly and dim while empty; `bForce` even if it was already shown so. Taken, it is heard. */
	void UpdateLook(bool bForce = false);
	/** Server: whoever walks over it and may takes it. */
	void GiveToWhoeverWalksOver();

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> Floor;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Plinth;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Pad;
	/** A faint column of light in its colour from the pad up through the weapon. */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamGlow;
	/** A soft light in its colour from the pad, on the weapon over it. */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Light;
	/** What turns and bobs: the weapon's model, skinned or plain as its look has it. */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> Spinner;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USkeletalMeshComponent> SkinnedModel;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PlainModel;

	/** The sound of its weapon being taken, heard near it. */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> TakenSound;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> TakenReach;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PadGlow;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Ghost;
	/** The model's own materials, slot by slot, for when it is back. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OwnMaterials;

	/** As last shown: there. */
	bool bShownThere = true;
	bool bLookApplied = false;
	float Spin = 0.0f;
	float NextCheckAt = 0.0f;
	/** Server: the round it last saw, to put its weapon back at a new one. */
	int32 SeenRound = -1;
};
