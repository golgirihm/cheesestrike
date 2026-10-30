// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeTarget.generated.h"

class UStaticMeshComponent;

/**
 * A cheese target on the Practice Range's wall (Rules/CheeseStrikeRange.h):
 * a wheel of cheese stood on its edge, its face toward the firing line
 * (the actor's forward), that shots break. Each hit takes the engine's
 * point damage off its health, its holes growing as the mice's bites grow
 * a block's (the cheese material's stage); at none it is broken, gone and
 * blocking nothing, and RegrowSeconds later it grows back from a seed to
 * whole. The server decides; the stage and when it grows back replicate,
 * so every machine shows it alike. Not an ACheeseStrikeCheese: the mice
 * never eat it, and it counts for none of the cheese a map has.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeTarget : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeTarget();

	float GetHealth() const { return Health; }
	/** Whether it is broken, waiting to grow back. */
	bool IsBroken() const { return BackAt > 0.0f; }
	/** How far broken, as it is drawn: 0 whole to 2 nearly broken. */
	int32 GetStage() const { return Stage; }

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;

protected:
	UFUNCTION()
	void OnRep_Look();
	/** Shows it as it is: its stage, and broken or growing back. */
	void ShowLook();
	float ServerNow() const;
	/** Its mesh's scale, whole. */
	FVector FullScale() const;

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Its size: across its face, and its thickness toward the line (cm). */
	UPROPERTY(EditAnywhere, Category = "Target")
	float Diameter = 60.0f;
	UPROPERTY(EditAnywhere, Category = "Target")
	float Thickness = 16.0f;

	/** Server: how much it has left. */
	float Health;
	UPROPERTY(ReplicatedUsing = OnRep_Look)
	uint8 Stage = 0;
	/** When it grows back, by the server's clock; 0 while it is there. */
	UPROPERTY(ReplicatedUsing = OnRep_Look)
	float BackAt = 0.0f;
	/** When it began to grow back, by the server's clock (each machine's own copy, for its growing); negative for whole. */
	float GrowingSince = -1.0f;
	/** As last shown: broken. */
	bool bShownBroken = false;
};
