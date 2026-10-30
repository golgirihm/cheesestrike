// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeMenuFigure.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
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

	// The character as players see one another, animated as in the game, holding the rifle: the hero picked (ShowHero).
	for (const CheeseStrikeBodies::FBody& Each : CheeseStrikeBodies::All())
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> Hero(Each.Mesh);
		Heroes.Add(Hero.Object);
	}
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> RifleMesh(TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle"));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Pedestal);
	Body->SetSkeletalMeshAsset(Heroes.IsValidIndex(0) ? Heroes[0].Get() : nullptr);
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

void ACheeseStrikeMenuFigure::ShowHero(int32 Hero)
{
	const int32 Wanted = Heroes.IsValidIndex(Hero) ? Hero : 0;
	USkeletalMesh* const Model = Heroes.IsValidIndex(Wanted) ? Heroes[Wanted].Get() : nullptr;
	if (Model == nullptr || (Wanted == Shown && Body->GetSkeletalMeshAsset() == Model))
	{
		return;
	}
	Shown = Wanted;
	Body->SetSkeletalMeshAsset(Model);
	// A fresh animation, which plays the new skeleton's set; nothing the last hero hid stays hidden.
	Body->SetAnimInstanceClass(nullptr);
	Body->SetAnimInstanceClass(UCheeseStrikeHeroAnimInstance::StaticClass());
	Body->EmptyOverrideMaterials();
	for (int32 Level = 0; Level < Model->GetLODNum(); ++Level)
	{
		Body->ShowAllMaterialSections(Level);
	}
	for (int32 Bone = 0; Bone < Model->GetRefSkeleton().GetNum(); ++Bone)
	{
		if (Body->IsBoneHiddenByName(Model->GetRefSkeleton().GetBoneName(Bone)))
		{
			Body->UnHideBoneByName(Model->GetRefSkeleton().GetBoneName(Bone));
		}
	}
	// Only our rifle shows, at this hero's grip.
	const CheeseStrikeBodies::FBody& Look = CheeseStrikeBodies::All()[Wanted];
	for (int32 Slot = 0; Slot < Look.NumWeaponSlots; ++Slot)
	{
		const FName Name(Look.WeaponSlots[Slot]);
		const int32 Material = Model->GetMaterials().IndexOfByPredicate([&Name](const FSkeletalMaterial& Each) { return Each.MaterialSlotName == Name; });
		for (int32 Level = 0; Material != INDEX_NONE && Level < Model->GetLODNum(); ++Level)
		{
			Body->ShowMaterialSection(Material, INDEX_NONE, false, Level);
		}
	}
	ACheeseStrikeWeapon::HideBuiltInWeapons(Body);
	ACheeseStrikeWeapon::PutInHand(Rifle, Body);
}

void ACheeseStrikeMenuFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AddActorWorldRotation(FRotator(0.0f, TurnRate * DeltaSeconds, 0.0f));
}
