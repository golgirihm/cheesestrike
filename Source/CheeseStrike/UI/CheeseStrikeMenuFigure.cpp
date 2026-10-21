// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenuFigure.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Weapons/CheeseStrikeWeapon.h"

ACheeseStrikeMenuFigure::ACheeseStrikeMenuFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	// A low round pedestal, from the engine's own shapes, in the level's grey.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grey(TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_Round.MI_PrototypeGrid_Gray_Round"));
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	RootComponent = Pedestal;
	Pedestal->SetStaticMesh(Cylinder.Object);
	if (Grey.Object != nullptr)
	{
		Pedestal->SetMaterial(0, Grey.Object);
	}
	// The engine's cylinder is a metre wide and tall, standing on its middle.
	Pedestal->SetRelativeScale3D(FVector(PedestalWidth / 100.0f, PedestalWidth / 100.0f, PedestalHeight / 100.0f));
	Pedestal->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The character as players see one another, animated as in the game, holding the rifle.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Belica(TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica.Belica"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle"));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Pedestal);
	Body->SetSkeletalMeshAsset(Belica.Object);
	Body->SetAnimInstanceClass(UCheeseStrikeHeroAnimInstance::StaticClass());
	// Undo the pedestal's scale, and stand on its top, facing the way the pedestal faces.
	Body->SetRelativeScale3D(FVector(100.0f / PedestalWidth, 100.0f / PedestalWidth, 100.0f / PedestalHeight));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Rifle = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rifle"));
	Rifle->SetupAttachment(Body);
	Rifle->SetSkeletalMeshAsset(RifleMesh.Object);
	Rifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACheeseStrikeMenuFigure::BeginPlay()
{
	Super::BeginPlay();
	// In the hand as the game holds it, with only our rifle showing.
	ACheeseStrikeWeapon::PutInHand(Rifle, Body);
	ACheeseStrikeWeapon::HideBuiltInWeapons(Body);
}

void ACheeseStrikeMenuFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AddActorWorldRotation(FRotator(0.0f, TurnRate * DeltaSeconds, 0.0f));
}
