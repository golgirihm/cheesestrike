// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCheeseBlood.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rules/CheeseStrikeBlood.h"

namespace
{
	const TCHAR* const CheeseDropMeshPath = TEXT("/Game/CheeseStrike/Effects/SM_CheeseDrop.SM_CheeseDrop");
	const TCHAR* const CheeseDropLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseBlood.M_CheeseBlood");
	const TCHAR* const CheeseDecalLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseBloodDecal.M_CheeseBloodDecal");
	const TCHAR* const CheeseMistLookPath = TEXT("/Game/CheeseStrike/Effects/M_CheeseMist.M_CheeseMist");
	/** What a drop could be without the script's assets: the engine's sphere (radius 50) in the cheese material. */
	const TCHAR* const FallbackDropMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	/** The mist's card: the engine's plane, 100 cm square, facing +Z. */
	const TCHAR* const MistCardPath = TEXT("/Engine/BasicShapes/Plane.Plane");
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
	MistMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mist"));
	MistMeshes->SetupAttachment(DropMeshes);
	MistMeshes->SetMobility(EComponentMobility::Movable);
	MistMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MistMeshes->SetCanEverAffectNavigation(false);
	MistMeshes->SetCastShadow(false);
	MistMeshes->SetGenerateOverlapEvents(false);
	MistMeshes->SetCullDistances(0, 0);
	MistMeshes->bUseAsOccluder = false;
	// Each puff's thickness, for the mist's material (PerInstanceCustomData 0).
	MistMeshes->NumCustomDataFloats = 1;
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
	if (Mesh != nullptr)
	{
		// Across the drop: its bounds' half width.
		DropMeshRadius = FMath::Max(0.01f, static_cast<float>(Mesh->GetBounds().BoxExtent.Y));
	}
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, CheeseDropLookPath))
	{
		DropMeshes->SetMaterial(0, Look);
	}
	if (UStaticMesh* Card = LoadObject<UStaticMesh>(nullptr, MistCardPath))
	{
		MistMeshes->SetStaticMesh(Card);
		MistCardHalf = FMath::Max(1.0f, static_cast<float>(Card->GetBounds().BoxExtent.X));
	}
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, CheeseMistLookPath))
	{
		MistMeshes->SetMaterial(0, Look);
	}
	DecalLook = LoadObject<UMaterialInterface>(nullptr, CheeseDecalLookPath);
}

void ACheeseStrikeCheeseBlood::Spray(const FVector& At, const FVector& Way, int32 Count, float Speed, float SpeedJitter, float ConeDegrees, float Size, float Life, float Over)
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
		Drop.Size = Size * FMath::FRandRange(1.0f - CheeseStrikeBlood::SizeJitter, 1.0f + CheeseStrikeBlood::SizeJitter);
		Drop.Life = Life;
		if (Over > 0.0f)
		{
			// Left at its own moment in the frame: as far on as it has flown since.
			const float Ahead = FMath::FRandRange(0.0f, Over);
			Drop.At += Drop.Velocity * Ahead - FVector(0.0f, 0.0f, 0.5f * CheeseStrikeBlood::Gravity * Ahead * Ahead);
			Drop.Velocity.Z -= CheeseStrikeBlood::Gravity * Ahead;
			Drop.Age = Ahead;
		}
		else
		{
			// Some leave a moment later than others, so that a burst is a stream rather than a ball.
			Drop.Age = -FMath::FRandRange(0.0f, 0.03f);
		}
		Drops.Add(Drop);
		++Sprayed;
	}
}

void ACheeseStrikeCheeseBlood::Puff(const FVector& At, const FVector& Drift, const CheeseStrikeBlood::FMist& Mist)
{
	LoadLooks();
	if (Puffs.Num() >= CheeseStrikeBlood::MostMist)
	{
		Puffs.RemoveAt(0);
	}
	FPuff Made;
	Made.At = At;
	Made.Drift = Drift;
	Made.Mist = Mist;
	Puffs.Add(Made);
}

void ACheeseStrikeCheeseBlood::ShotSpray(const FVector& At, const FVector& Way, float Damage)
{
	const CheeseStrikeBlood::FSpray Made = CheeseStrikeBlood::ShotSpray(Damage);
	// Out of the far side along the shot, and a little back toward the shooter too, as a hit splashes: fine drops, gone in
	// a moment, and soft puffs of mist.
	// `At` is where the shot went in; most of it leaves by the far side, ExitDepth on.
	const FVector Along = Way.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Exit = At + Along * CheeseStrikeBlood::ExitDepth;
	Spray(Exit, Way, Made.Drops * 3 / 4, Made.Speed, Made.SpeedJitter, Made.ConeDegrees, Made.Size, Made.Life);
	Spray(At, -Way + FVector(0.0f, 0.0f, 0.35f), Made.Drops - Made.Drops * 3 / 4, Made.Speed * 0.55f, Made.SpeedJitter, Made.ConeDegrees * 1.6f, Made.Size * 0.8f, Made.Life);
	// The puff where it went in, which the shooter sees, and for a harder hit one where it came out.
	const CheeseStrikeBlood::FMist Mist = CheeseStrikeBlood::ShotMist(Damage);
	Puff(At - Along * 8.0f, -Along * 90.0f + FVector(0.0f, 0.0f, 20.0f), Mist);
	if (CheeseStrikeBlood::ShotMistPuffs(Damage) > 1)
	{
		Puff(Exit + Along * 6.0f, Along * 160.0f, Mist);
	}
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
	LongestDrop = 0.0f;
	for (const FDrop& Drop : Drops)
	{
		if (Drop.Age < 0.0f)
		{
			continue;
		}
		// Drawn out along the way it flies, the faster the longer (CheeseStrikeBlood::DropStretch); shrinking away at the end
		// of its life.
		const float Speed = Drop.Velocity.Size();
		const float Stretch = CheeseStrikeBlood::DropStretch(Speed);
		const float Shrink = FMath::Clamp((Drop.Life - Drop.Age) / FMath::Min(0.3f, Drop.Life * 0.4f), 0.0f, 1.0f);
		const float Scale = Drop.Size * Shrink / DropMeshRadius;
		const FQuat Turn = Speed > 1.0f ? Drop.Velocity.ToOrientationQuat() : FQuat::Identity;
		Shown.Add(FTransform(Turn, Drop.At, FVector(Scale * Stretch, Scale, Scale)));
		LongestDrop = FMath::Max(LongestDrop, CheeseStrikeBlood::DropLength(Drop.Size * Shrink, Speed));
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

void ACheeseStrikeCheeseBlood::ShowMist(float DeltaSeconds)
{
	// Each puff drifts, slowing, grows and thins away; its card is turned to this machine's camera.
	FVector Camera = FVector::ZeroVector;
	bool bCamera = false;
	if (const APlayerController* Player = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr; Player != nullptr && Player->PlayerCameraManager != nullptr)
	{
		Camera = Player->PlayerCameraManager->GetCameraLocation();
		bCamera = true;
	}
	for (int32 Index = Puffs.Num() - 1; Index >= 0; --Index)
	{
		FPuff& Each = Puffs[Index];
		Each.Age += DeltaSeconds;
		if (Each.Age >= Each.Mist.Life)
		{
			Puffs.RemoveAt(Index);
			continue;
		}
		Each.At += Each.Drift * DeltaSeconds;
		Each.Drift *= FMath::Max(0.0f, 1.0f - 4.0f * DeltaSeconds);
	}
	MistShown.Reset(Puffs.Num());
	MistFades.Reset(Puffs.Num());
	for (const FPuff& Each : Puffs)
	{
		const FVector Facing = bCamera ? (Camera - Each.At).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector) : FVector::UpVector;
		const float Scale = CheeseStrikeBlood::MistRadiusAt(Each.Mist, Each.Age) / MistCardHalf;
		MistShown.Add(FTransform(FRotationMatrix::MakeFromZ(Facing).ToQuat(), Each.At, FVector(Scale, Scale, 1.0f)));
		MistFades.Add(CheeseStrikeBlood::MistOpacityAt(Each.Mist, Each.Age));
	}
	if (MistShown.Num() != MistMeshes->GetInstanceCount())
	{
		MistMeshes->ClearInstances();
		if (MistShown.Num() > 0)
		{
			MistMeshes->AddInstances(MistShown, false, true, false);
		}
	}
	else if (MistShown.Num() > 0)
	{
		MistMeshes->BatchUpdateInstancesTransforms(0, MistShown, true, true, true);
	}
	for (int32 Index = 0; Index < MistFades.Num(); ++Index)
	{
		MistMeshes->SetCustomDataValue(Index, 0, MistFades[Index], Index == MistFades.Num() - 1);
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
		if (Drop.Age > Drop.Life)
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
	ShowMist(DeltaSeconds);

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
