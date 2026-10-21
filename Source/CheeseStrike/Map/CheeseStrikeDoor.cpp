// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeDoor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrikeCharacter.h"
#include "Map/CheeseStrikeMouse.h"

namespace
{
	/** Seconds the leaves take to open or shut. */
	constexpr float DoorSlideTime = 0.35f;
	/** How often the server looks for anyone near (seconds). */
	constexpr float DoorWatchEvery = 0.1f;
	/** How far above or below its bottom someone still opens it (cm): not someone on the floor above. */
	constexpr float DoorReachUp = 300.0f;
	/** A mouse's middle is this much further from the door than its nose (cm). */
	constexpr float DoorMouseReach = 200.0f;
	/** The leaves stop short of the doorway's top, so that what is built over it never rests on them (cm). */
	constexpr float DoorTopGap = 10.0f;
}

ACheeseStrikeDoor::ACheeseStrikeDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetNetUpdateFrequency(10.0f);
	NetDormancy = DORM_Initial;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);
	for (TObjectPtr<UStaticMeshComponent>* Leaf : {&LeafA, &LeafB})
	{
		*Leaf = CreateDefaultSubobject<UStaticMeshComponent>(Leaf == &LeafA ? TEXT("LeafA") : TEXT("LeafB"));
		(*Leaf)->SetupAttachment(Root);
		(*Leaf)->SetStaticMesh(Cube.Object);
		(*Leaf)->SetMobility(EComponentMobility::Movable);
		// It blocks people and shots, but the navigation mesh runs through it: bots and mice walk up to it and it opens.
		(*Leaf)->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		(*Leaf)->SetCanEverAffectNavigation(false);
	}
}

void ACheeseStrikeDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeDoor, bOpen);
}

void ACheeseStrikeDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PlaceLeaves();
}

void ACheeseStrikeDoor::BeginPlay()
{
	Super::BeginPlay();
	Openness = bOpen ? 1.0f : 0.0f;
	PlaceLeaves();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(WatchTimer, this, &ACheeseStrikeDoor::Watch, DoorWatchEvery, true, FMath::FRandRange(0.0f, DoorWatchEvery));
	}
}

bool ACheeseStrikeDoor::Opens(float Distance, float Height, float Range)
{
	return Distance <= Range && FMath::Abs(Height) <= DoorReachUp;
}

void ACheeseStrikeDoor::Watch()
{
	const FVector Middle = GetActorLocation();
	bool bSomeone = false;
	for (TActorIterator<APawn> It(GetWorld()); It && !bSomeone; ++It)
	{
		const ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(*It);
		if (Character != nullptr && !Character->IsAlive())
		{
			continue;
		}
		const FVector At = It->GetActorLocation();
		bSomeone = Opens(FVector::Dist2D(At, Middle), At.Z - Middle.Z - 100.0f, Range);
	}
	for (TActorIterator<ACheeseStrikeMouse> It(GetWorld()); It && !bSomeone; ++It)
	{
		const FVector At = It->GetActorLocation();
		bSomeone = Opens(FVector::Dist2D(At, Middle), At.Z - Middle.Z - 100.0f, Range + DoorMouseReach);
	}
	if (bSomeone != bOpen)
	{
		bOpen = bSomeone;
		FlushNetDormancy();
		OnRep_Open();
	}
}

void ACheeseStrikeDoor::OnRep_Open()
{
	SetActorTickEnabled(true);
}

void ACheeseStrikeDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Wanted = bOpen ? 1.0f : 0.0f;
	Openness = FMath::FInterpConstantTo(Openness, Wanted, DeltaSeconds, 1.0f / DoorSlideTime);
	PlaceLeaves();
	if (Openness == Wanted)
	{
		SetActorTickEnabled(false);
	}
}

void ACheeseStrikeDoor::PlaceLeaves()
{
	if (LeafA == nullptr || LeafB == nullptr)
	{
		return;
	}
	// The cube is a metre a side about its middle; the door's Y runs along the wall.
	const float Tall = FMath::Max(10.0f, Height - DoorTopGap);
	const float LeafWidth = bDouble ? Width * 0.5f : Width;
	const float Slide = LeafWidth * Openness;
	LeafA->SetRelativeScale3D(FVector(Thickness / 100.0f, LeafWidth / 100.0f, Tall / 100.0f));
	LeafA->SetRelativeLocation(FVector(0.0f, (bDouble ? -Width * 0.25f : 0.0f) - Slide, Tall * 0.5f));
	LeafB->SetVisibility(bDouble);
	LeafB->SetCollisionEnabled(bDouble ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	LeafB->SetRelativeScale3D(FVector(Thickness / 100.0f, LeafWidth / 100.0f, Tall / 100.0f));
	LeafB->SetRelativeLocation(FVector(0.0f, Width * 0.25f + Slide, Tall * 0.5f));
	if (LeafMaterial != nullptr)
	{
		LeafA->SetMaterial(0, LeafMaterial);
		LeafB->SetMaterial(0, LeafMaterial);
	}
}
