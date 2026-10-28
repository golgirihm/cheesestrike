// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeCheeseBlood.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * The cheese players bleed (Rules/CheeseStrikeBlood.h): one of these in a
 * world that draws, made the first time it is asked for (For), never on a
 * dedicated server, never replicated. It flies drops of molten cheese, all
 * one instanced mesh, by simple ballistics, each traced against the level as
 * it goes (never players), and leaves a splat (a decal) where one in a few
 * lands; and it spreads pools. Its looks (M_CheeseBlood, M_CheeseBloodDecal
 * and SM_CheeseDrop in /Game/CheeseStrike/Effects, made by
 * tools/art/cheese_blood.py) are loaded by path, so the script can rebuild them.
 */
UCLASS(NotPlaceable, Transient)
class ACheeseStrikeCheeseBlood : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeCheeseBlood();

	/** The world's, made now if there is none yet; none on a dedicated server or a world that isn't a game's. */
	static ACheeseStrikeCheeseBlood* For(UWorld* World);
	/** The world's if it has one yet, never made. */
	static ACheeseStrikeCheeseBlood* Find(const UWorld* World);

	/**
	 * Throws `Count` drops from `At`, along `Way` within a cone of `ConeDegrees`, at `Speed` (each faster or slower by up to
	 * `SpeedJitter` of it), of radius `Size` (each from two thirds to four thirds of it), as many as the cap allows.
	 */
	void Spray(const FVector& At, const FVector& Way, int32 Count, float Speed, float SpeedJitter, float ConeDegrees, float Size);
	/** A hit's spray (CheeseStrikeBlood::ShotSpray) at `At` along `Way`. */
	void ShotSpray(const FVector& At, const FVector& Way, float Damage);
	/** Starts a pool on the floor under `Above` (traced down to a metre and a half), spreading as CheeseStrikeBlood's pools do. */
	void Pool(const FVector& Above, const AActor* Ignore = nullptr);

	/** How many drops it has thrown since it was made (for the tests: the cheese reached this machine). */
	int32 GetSprayed() const { return Sprayed; }
	int32 GetDropCount() const { return Drops.Num(); }
	int32 GetSplatCount() const { return Splats.Num(); }
	int32 GetPoolCount() const;

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FDrop
	{
		FVector At = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Size = 1.0f;
		float Age = 0.0f;
	};
	struct FSplat
	{
		TObjectPtr<UDecalComponent> Decal;
		float Born = 0.0f;
		float Radius = 0.0f;
		bool bPool = false;
	};

	void LoadLooks();
	void Land(const FDrop& Drop, const FHitResult& Hit);
	UDecalComponent* MakeDecal(const FVector& At, const FVector& Normal, float Radius, float Depth);
	void ShowDrops();

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DropMeshes;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DecalLook;

	TArray<FDrop> Drops;
	TArray<FSplat> Splats;
	TArray<FTransform> Shown;
	int32 Landed = 0;
	int32 Sprayed = 0;
	bool bLooksLoaded = false;
};
