// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeGooSplats.generated.h"

class ACheeseStrikeCharacter;
class UDecalComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The melted gouda under the Gouda Overload's slowed foes (Rules/CheeseStrikeGoo.h):
 * one of these in a world that draws, made the first time a character there
 * is splatted (ACheeseStrikeCharacter::OnRep_Splat, For), never on a
 * dedicated server, never replicated. Each frame it looks at every character
 * the gouda slows on this machine (GooFactorNow under 1, from the replicated
 * SplatAt and GooUntil), and lays puddles on the floor at their feet as
 * CheeseStrikeGoo says: decals in the cheese blood's splat look
 * (M_CheeseBloodDecal, a blob with a wavy edge and a few drops round it,
 * glossy, paler and thicker in the middle) coloured as melted gouda, each
 * spreading, staying and fading in its time. Characters receive no decals.
 * Since wave 17 each puddle is a thick glob of gouda (one of SM_GooBlob1..3
 * in M_GooBlob, tools/art/goo_blobs.py: a wavy-edged mound with humps and a
 * few drops round it) lying in the decal's wet film: landing as a heap,
 * slumping out as it spreads, melting flat as it fades
 * (CheeseStrikeGoo::BlobHeightAt); it neither collides nor is traced.
 */
UCLASS(NotPlaceable, Transient)
class ACheeseStrikeGooSplats : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeGooSplats();

	/** The world's, made now if there is none yet; none on a dedicated server or a world that isn't a game's. */
	static ACheeseStrikeGooSplats* For(UWorld* World);
	/** The world's if it has one yet, never made. */
	static ACheeseStrikeGooSplats* Find(const UWorld* World);

	/** How many puddles lie now, and how many within `Reach` cm (across the floor) of `At` (for the tests). */
	int32 GetPuddleCount() const { return Puddles.Num(); }
	int32 CountNear(const FVector& At, float Reach) const;
	/** How many globs lie within `Reach` cm (across the floor) of `At`, and how tall the tallest of them is (cm). */
	int32 CountBlobsNear(const FVector& At, float Reach, float* OutTallest = nullptr) const;
	/** How many it has laid since it was made. */
	int32 GetLaid() const { return Laid; }

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPuddle
	{
		TObjectPtr<UDecalComponent> Decal;
		TObjectPtr<UStaticMeshComponent> Blob;
		float Born = 0.0f;
		float Full = 0.0f;
		float Radius = 0.0f;
		float Opacity = 1.0f;
	};
	/** Where and when each slowed character last had a puddle laid, and the splat it was for. */
	struct FTrail
	{
		FVector LastAt = FVector::ZeroVector;
		float LastTime = 0.0f;
		float SplatAt = 0.0f;
	};

	/** Lays a puddle of full radius `Full` on the floor under `Who`, if there is floor under its feet. */
	bool Lay(ACheeseStrikeCharacter* Who, float Full, float Now);

	/** Shows a puddle's glob as it is `Since` it was laid, at its radius now. */
	static void ShapeBlob(UStaticMeshComponent* Blob, float Radius, float Since);
	/** Takes a puddle off the floor. */
	static void Clear(FPuddle& Puddle);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Look;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> Blobs;

	TArray<FPuddle> Puddles;
	TMap<TWeakObjectPtr<ACheeseStrikeCharacter>, FTrail> Trails;
	int32 Laid = 0;
	bool bLookLoaded = false;
};
