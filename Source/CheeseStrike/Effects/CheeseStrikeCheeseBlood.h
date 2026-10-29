// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CheeseStrikeBlood.h"
#include "CheeseStrikeCheeseBlood.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * The cheese players bleed (Rules/CheeseStrikeBlood.h): one of these in a
 * world that draws, made the first time it is asked for (For), never on a
 * dedicated server, never replicated. It flies drops of molten cheese, all
 * one instanced mesh (a small teardrop, drawn out along its flight the faster
 * it goes), by simple ballistics, each traced against the level as it goes
 * (never players), and leaves a splat (a decal) where one in a few lands; it
 * puffs soft mist (another instanced mesh, flat cards turned to this
 * machine's camera, each faded by its custom data); and it spreads pools. Its
 * looks (M_CheeseBlood, M_CheeseMist, M_CheeseBloodDecal and SM_CheeseDrop in
 * /Game/CheeseStrike/Effects, made by tools/art/cheese_blood.py) are loaded
 * by path, so the script can rebuild them.
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
	 * `SpeedJitter` of it), of radius `Size` (each within CheeseStrikeBlood::SizeJitter of it), each gone after `Life` if it
	 * lands on nothing, as many as the cap allows. A stream made a frame at a time passes the frame's `Over` seconds: each
	 * drop is put as far along as it would be had it left at its own moment in that time, so that the stream is unbroken
	 * rather than clumps a frame apart.
	 */
	void Spray(const FVector& At, const FVector& Way, int32 Count, float Speed, float SpeedJitter, float ConeDegrees, float Size, float Life = CheeseStrikeBlood::DropLife, float Over = 0.0f);
	/** A puff of mist at `At`, drifting at `Drift`, growing and fading as `Mist` says (past the cap, the oldest goes). */
	void Puff(const FVector& At, const FVector& Drift, const CheeseStrikeBlood::FMist& Mist);
	/** A hit's misty spray (CheeseStrikeBlood::ShotSpray, ShotMist) at `At` along `Way`. */
	void ShotSpray(const FVector& At, const FVector& Way, float Damage);
	/** Starts a pool on the floor under `Above` (traced down to a metre and a half), spreading as CheeseStrikeBlood's pools do. */
	void Pool(const FVector& Above, const AActor* Ignore = nullptr);

	/** How many drops it has thrown since it was made (for the tests: the cheese reached this machine). */
	int32 GetSprayed() const { return Sprayed; }
	int32 GetDropCount() const { return Drops.Num(); }
	int32 GetMistCount() const { return Puffs.Num(); }
	/** The longest drop drawn last frame, nose to tail (cm), for the tests. */
	float GetLongestDrop() const { return LongestDrop; }
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
		float Life = CheeseStrikeBlood::DropLife;
	};
	struct FPuff
	{
		FVector At = FVector::ZeroVector;
		FVector Drift = FVector::ZeroVector;
		CheeseStrikeBlood::FMist Mist;
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
	void ShowMist(float DeltaSeconds);

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DropMeshes;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> MistMeshes;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DecalLook;

	TArray<FDrop> Drops;
	TArray<FSplat> Splats;
	TArray<FPuff> Puffs;
	TArray<FTransform> Shown;
	TArray<FTransform> MistShown;
	TArray<float> MistFades;
	/** The drop mesh's radius at a scale of 1 (from its bounds: the script's teardrop, or the engine's sphere without it), and half the mist card's width. */
	float DropMeshRadius = 1.0f;
	float MistCardHalf = 50.0f;
	float LongestDrop = 0.0f;
	int32 Landed = 0;
	int32 Sprayed = 0;
	bool bLooksLoaded = false;
};
