// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CheeseStrikeArtTools.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * Editor work the art scripts in tools/art/ ask for that the editor's Python
 * can't do itself (unreal.CheeseStrikeArtTools). Editor only: in a game they
 * do nothing.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeArtTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes a ragdoll for `Mesh` as the editor's Create Physics Asset does
	 * (a capsule for each bone of any length, joints with limits), saved as
	 * the asset `AssetPath`, and makes it the mesh's physics asset. The head
	 * is the last body: what hangs from it (the face and jaw) follows it as
	 * posed.
	 */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static UPhysicsAsset* MakeRagdoll(USkeletalMesh* Mesh, const FString& AssetPath);

	/** The bones of a ragdoll's bodies. */
	UFUNCTION(BlueprintCallable, Category = "CheeseStrike|Art")
	static TArray<FName> RagdollBodies(UPhysicsAsset* Ragdoll);
};
