// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeArtTools.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicsAssetUtils.h"
#include "UObject/Package.h"
#endif

UPhysicsAsset* UCheeseStrikeArtTools::MakeRagdoll(USkeletalMesh* Mesh, const FString& AssetPath)
{
#if WITH_EDITOR
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	UPackage* Package = CreatePackage(*AssetPath);
	UPhysicsAsset* Ragdoll = NewObject<UPhysicsAsset>(Package, FName(*FPaths::GetBaseFilename(AssetPath)), RF_Public | RF_Standalone | RF_Transactional);
	FPhysAssetCreateParams Params;
	FText Error;
	if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Ragdoll, Mesh, Params, Error, true, false))
	{
		UE_LOG(LogTemp, Error, TEXT("MakeRagdoll: %s"), *Error.ToString());
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Ragdoll);
	Ragdoll->MarkPackageDirty();
	Mesh->MarkPackageDirty();
	return Ragdoll;
#else
	return nullptr;
#endif
}
