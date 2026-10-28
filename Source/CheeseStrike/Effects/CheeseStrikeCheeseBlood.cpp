// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCheeseBlood.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rules/CheeseStrikeBlood.h"

namespace
{
	const TCHAR* const CheeseDropMeshPath = TEXT("/Game/CheeseStrike/Effects/SM_CheeseDrop.SM_CheeseDrop");
	const TCHAR* const CheeseDropLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseBlood.M_CheeseBlood");
	const TCHAR* const CheeseDecalLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseBloodDecal.M_CheeseBloodDecal");
	/** What a drop could be without the script's assets: the engine's sphere (radius 50) in the cheese material. */
	const TCHAR* const FallbackDropMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	/** The drop mesh's radius at a scale of 1. */
	constexpr float DropMeshRadius = 50.0f;
	/** How deep a decal projects, through the surface it lies on. */
	constexpr float SplatDepth = 12.0f;
	constexpr float PoolDepth = 40.0f;
}

ACheeseStrikeCheeseBlood::ACheeseStrikeCheeseBlood()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetReplicates(false);
	DropMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	RootComponent = DropMeshes;
	DropMeshes->SetMobility(EComponentMobility::Movable);
	DropMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropMeshes->SetCanEverAffectNavigation(false);
	DropMeshes->SetCastShadow(false);
	DropMeshes->SetGenerateOverlapEvents(false);
	// Instances move every frame: never culled by stale bounds.
	DropMeshes->SetCullDistances(0, 0);
	DropMeshes->bUseAsOccluder = false;
}

ACheeseStrikeCheeseBlood* ACheeseStrikeCheeseBlood::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ACheeseStrikeCheeseBlood> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

ACheeseStrikeCheeseBlood* ACheeseStrikeCheeseBlood::For(UWorld* World)
{
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return nullptr;
	}
	if (ACheeseStrikeCheeseBlood* Found = Find(World))
	{
		return Found;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.ObjectFlags |= RF_Transient;
	return World->SpawnActor<ACheeseStrikeCheeseBlood>(FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
}

void ACheeseStrikeCheeseBlood::LoadLooks()
{
	if (bLooksLoaded)
	{
		return;
	}
	bLooksLoaded = true;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, CheeseDropMeshPath);
	if (Mesh == nullptr)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, FallbackDropMeshPath);
	}
	DropMeshes->SetStaticMesh(Mesh);
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, CheeseDropLookPath))
	{
		DropMeshes->SetMaterial(0, Look);
	}
	DecalLook = LoadObject<UMaterialInterface>(nullptr, CheeseDecalLookPath);
}

void ACheeseStrikeCheeseBlood::Spray(const FVector& At, const FVector& Way, int32 Count, float Speed, float SpeedJitter, float ConeDegrees, float Size)
{
	LoadLooks();
	const int32 Allowed = CheeseStrikeBlood::DropsAllowed(Count, Drops.Num());
	const FVector Along = Way.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const float Cone = FMath::DegreesToRadians(ConeDegrees);
	for (int32 Each = 0; Each < Allowed; ++Each)
	{
		FDrop Drop;
		Drop.At = At + FMath::VRand() * 1.5f;
		Drop.Velocity = FMath::VRandCone(Along, Cone) * Speed * FMath::FRandRange(1.0f - SpeedJitter, 1.0f + SpeedJitter);
		Drop.Size = Size * FMath::FRandRange(0.66f, 1.33f);
		// Some leave a moment later than others, so that a burst is a stream rather than a ball.
		Drop.Age = -FMath::FRandRange(0.0f, 0.03f);
		Drops.Add(Drop);
		++Sprayed;
	}
}

void ACheeseStrikeCheeseBlood::ShotSpray(const FVector& At, const FVector& Way, float Damage)
{
	const CheeseStrikeBlood::FSpray Made = CheeseStrikeBlood::ShotSpray(Damage);
	// Out of the far side along the shot, and a little back toward the shooter too, as a hit splashes.
	Spray(At, Way, Made.Drops * 3 / 4, Made.Speed, Made.SpeedJitter, Made.ConeDegrees, Made.Size);
	Spray(At, -Way + FVector(0.0f, 0.0f, 0.35f), Made.Drops - Made.Drops * 3 / 4, Made.Speed * 0.55f, Made.SpeedJitter, Made.ConeDegrees * 1.6f, Made.Size * 0.8f);
}

int32 ACheeseStrikeCheeseBlood::GetPoolCount() const
{
	int32 Count = 0;
	for (const FSplat& Each : Splats)
	{
		Count += Each.bPool ? 1 : 0;
	}
	return Count;
}

UDecalComponent* ACheeseStrikeCheeseBlood::MakeDecal(const FVector& At, const FVector& Normal, float Radius, float Depth)
{
	LoadLooks();
	if (DecalLook == nullptr)
	{
		return nullptr;
	}
	UDecalComponent* Decal = NewObject<UDecalComponent>(this);
	// Projected into the surface, turned at random about it.
	FRotator Facing = (-Normal).Rotation();
	Facing.Roll = FMath::FRandRange(0.0f, 360.0f);
	Decal->SetWorldLocationAndRotation(At, Facing);
	Decal->DecalSize = FVector(Depth, Radius, Radius);
	Decal->SetDecalMaterial(DecalLook);
	Decal->RegisterComponent();
	if (UMaterialInstanceDynamic* Look = Decal->CreateDynamicMaterialInstance())
	{
		Look->SetScalarParameterValue(TEXT("Seed"), FMath::FRandRange(0.0f, 100.0f));
		Look->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	}
	return Decal;
}

void ACheeseStrikeCheeseBlood::Pool(const FVector& Above, const AActor* Ignore)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeBloodPool), false);
	if (Ignore != nullptr)
	{
		Params.AddIgnoredActor(Ignore);
	}
	FHitResult Floor;
	if (!World->LineTraceSingleByObjectType(Floor, Above, Above - FVector(0.0f, 0.0f, 150.0f), FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic)), Params))
	{
		return;
	}
	// The oldest pool makes way for a new one past the cap.
	if (GetPoolCount() >= CheeseStrikeBlood::MostPools)
	{
		for (int32 Index = 0; Index < Splats.Num(); ++Index)
		{
			if (Splats[Index].bPool)
			{
				if (Splats[Index].Decal != nullptr)
				{
					Splats[Index].Decal->DestroyComponent();
				}
				Splats.RemoveAt(Index);
				break;
			}
		}
	}
	FSplat Made;
	Made.Decal = MakeDecal(Floor.ImpactPoint, Floor.ImpactNormal, CheeseStrikeBlood::PoolStart, PoolDepth);
	if (Made.Decal == nullptr)
	{
		return;
	}
	// Under the splats that land on it.
	Made.Decal->SortOrder = -1;
	Made.Born = World->GetTimeSeconds();
	Made.Radius = CheeseStrikeBlood::PoolStart;
	Made.bPool = true;
	Splats.Add(Made);
}

void ACheeseStrikeCheeseBlood::Land(const FDrop& Drop, const FHitResult& Hit)
{
	if (!CheeseStrikeBlood::LeavesSplat(Landed++))
	{
		return;
	}
	// Past the cap, the oldest splat (never a pool) goes.
	int32 Count = Splats.Num() - GetPoolCount();
	for (int32 Index = 0; Index < Splats.Num() && Count >= CheeseStrikeBlood::MostSplats; )
	{
		if (!Splats[Index].bPool)
		{
			if (Splats[Index].Decal != nullptr)
			{
				Splats[Index].Decal->DestroyComponent();
			}
			Splats.RemoveAt(Index);
			--Count;
		}
		else
		{
			++Index;
		}
	}
	FSplat Made;
	Made.Radius = CheeseStrikeBlood::SplatRadius(Drop.Size);
	Made.Decal = MakeDecal(Hit.ImpactPoint, Hit.ImpactNormal, Made.Radius, SplatDepth);
	if (Made.Decal == nullptr)
	{
		return;
	}
	Made.Born = GetWorld()->GetTimeSeconds();
	Splats.Add(Made);
}

void ACheeseStrikeCheeseBlood::ShowDrops()
{
	Shown.Reset(Drops.Num());
	for (const FDrop& Drop : Drops)
	{
		if (Drop.Age < 0.0f)
		{
			continue;
		}
		// Drawn out along the way it flies, the faster the longer; shrinking away at the end of its life.
		const float Speed = Drop.Velocity.Size();
		const float Stretch = 1.0f + FMath::Clamp(Speed / 700.0f, 0.0f, 1.2f);
		const float Shrink = FMath::Clamp((CheeseStrikeBlood::DropLife - Drop.Age) / 0.3f, 0.0f, 1.0f);
		const float Scale = Drop.Size * Shrink / DropMeshRadius;
		const FQuat Turn = Speed > 1.0f ? Drop.Velocity.ToOrientationQuat() : FQuat::Identity;
		Shown.Add(FTransform(Turn, Drop.At, FVector(Scale * Stretch, Scale * 0.85f, Scale * 0.85f)));
	}
	if (Shown.Num() != DropMeshes->GetInstanceCount())
	{
		DropMeshes->ClearInstances();
		if (Shown.Num() > 0)
		{
			DropMeshes->AddInstances(Shown, false, true, false);
		}
	}
	else if (Shown.Num() > 0)
	{
		DropMeshes->BatchUpdateInstancesTransforms(0, Shown, true, true, true);
	}
}

void ACheeseStrikeCheeseBlood::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// The drops fly, and land on whatever of the level they meet first (players are no object to them).
	const FCollisionObjectQueryParams Level(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeBloodDrop), false);
	Params.AddIgnoredActor(this);
	const float Step = FMath::Min(DeltaSeconds, 0.05f);
	for (int32 Index = Drops.Num() - 1; Index >= 0; --Index)
	{
		FDrop& Drop = Drops[Index];
		Drop.Age += Step;
		if (Drop.Age < 0.0f)
		{
			continue;
		}
		if (Drop.Age > CheeseStrikeBlood::DropLife)
		{
			Drops.RemoveAtSwap(Index);
			continue;
		}
		// A little drag, so that a fast spray slows and falls.
		Drop.Velocity *= FMath::Max(0.0f, 1.0f - 0.6f * Step);
		Drop.Velocity.Z -= CheeseStrikeBlood::Gravity * Step;
		const FVector To = Drop.At + Drop.Velocity * Step;
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, Drop.At, To, Level, Params))
		{
			Land(Drop, Hit);
			Drops.RemoveAtSwap(Index);
			continue;
		}
		Drop.At = To;
	}
	ShowDrops();

	// The splats fade in their time, and the pools spread, stay and fade.
	const float Now = World->GetTimeSeconds();
	for (int32 Index = Splats.Num() - 1; Index >= 0; --Index)
	{
		FSplat& Splat = Splats[Index];
		const float Since = Now - Splat.Born;
		const bool bGone = Splat.Decal == nullptr || (Splat.bPool ? CheeseStrikeBlood::IsPoolGone(Since) : CheeseStrikeBlood::SplatOpacityAt(Since) <= 0.0f);
		if (bGone)
		{
			if (Splat.Decal != nullptr)
			{
				Splat.Decal->DestroyComponent();
			}
			Splats.RemoveAt(Index);
			continue;
		}
		const float Opacity = Splat.bPool ? CheeseStrikeBlood::PoolOpacityAt(Since) : CheeseStrikeBlood::SplatOpacityAt(Since);
		if (Splat.bPool)
		{
			const float Radius = CheeseStrikeBlood::PoolRadiusAt(Since);
			if (!FMath::IsNearlyEqual(Radius, Splat.Radius, 0.25f))
			{
				Splat.Radius = Radius;
				Splat.Decal->DecalSize = FVector(PoolDepth, Radius, Radius);
				Splat.Decal->MarkRenderStateDirty();
			}
		}
		if (Opacity < 1.0f)
		{
			if (UMaterialInstanceDynamic* Look = Cast<UMaterialInstanceDynamic>(Splat.Decal->GetDecalMaterial()))
			{
				Look->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			}
		}
	}
}
