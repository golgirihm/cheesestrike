// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeCheese.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Rules/CheeseStrikeRules.h"

ACheeseStrikeCheese::ACheeseStrikeCheese()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cheese(TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(Cube.Object);
	if (Cheese.Object != nullptr)
	{
		Mesh->SetMaterial(0, Cheese.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetMobility(EComponentMobility::Static);
	Health = CheeseStrikeRules::CheeseBlockHealth;
}
