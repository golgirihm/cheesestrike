// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeArtTools.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
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
	// No bodies under the head: the face's bones (Belica's faceAttach and C_jaw) follow the head as posed. With bodies
	// of their own they swing on their joints as the body falls, the face sliding round the skull and the jaw hanging.
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 Head = Skeleton.FindBoneIndex(TEXT("head"));
	for (int32 Body = Ragdoll->SkeletalBodySetups.Num() - 1; Body >= 0; --Body)
	{
		const int32 Bone = Skeleton.FindBoneIndex(Ragdoll->SkeletalBodySetups[Body]->BoneName);
		const bool bUnderHead = Head != INDEX_NONE && Bone != Head && Bone != INDEX_NONE && Skeleton.BoneIsChildOf(Bone, Head);
		if (bUnderHead)
		{
			FPhysicsAssetUtils::DestroyBody(Ragdoll, Body);
		}
	}
	Ragdoll->UpdateBodySetupIndexMap();
	Ragdoll->UpdateBoundsBodiesArray();
	FAssetRegistryModule::AssetCreated(Ragdoll);
	Ragdoll->MarkPackageDirty();
	Mesh->MarkPackageDirty();
	return Ragdoll;
#else
	return nullptr;
#endif
}

TArray<FName> UCheeseStrikeArtTools::RagdollBodies(UPhysicsAsset* Ragdoll)
{
	TArray<FName> Bones;
	if (Ragdoll != nullptr)
	{
		for (const TObjectPtr<USkeletalBodySetup>& Body : Ragdoll->SkeletalBodySetups)
		{
			Bones.Add(Body->BoneName);
		}
	}
	return Bones;
}
