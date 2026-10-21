// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeAerialCheese.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Rules/CheeseStrikeStreaks.h"

namespace
{
	/** The drone's wheel, as the engine's 1 m cylinder is scaled: 3.2 m across, 1.1 m thick; its rotors 1.4 m across, out at its corners. */
	const FVector DroneWheelScale(3.2f, 3.2f, 1.1f);
	const FVector DroneRotorScale(1.4f, 1.4f, 0.05f);
	constexpr float DroneRotorOut = 200.0f;
}

ACheeseStrikeAerialCheese::ACheeseStrikeAerialCheese()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.0f);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Wheel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel"));
	Wheel->SetupAttachment(RootComponent);
	Wheel->SetStaticMesh(Cylinder.Object);
	Wheel->SetRelativeScale3D(DroneWheelScale);
	Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		UStaticMeshComponent* Rotor = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rotor%d"), Corner));
		Rotor->SetupAttachment(RootComponent);
		Rotor->SetStaticMesh(Cylinder.Object);
		Rotor->SetRelativeScale3D(DroneRotorScale);
		const float Angle = FMath::DegreesToRadians(45.0f + 90.0f * Corner);
		Rotor->SetRelativeLocation(FVector(FMath::Cos(Angle) * DroneRotorOut, FMath::Sin(Angle) * DroneRotorOut, 70.0f));
		Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rotors.Add(Rotor);
	}
	Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(RootComponent);
	Beacon->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
	Beacon->SetLightColor(FLinearColor(1.0f, 0.1f, 0.05f));
	Beacon->SetIntensityUnits(ELightUnits::Candelas);
	Beacon->SetIntensity(3000.0f);
	Beacon->SetAttenuationRadius(800.0f);
	Beacon->SetCastShadows(false);
}

void ACheeseStrikeAerialCheese::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeAerialCheese, Centre);
	DOREPLIFETIME(ACheeseStrikeAerialCheese, Radius);
	DOREPLIFETIME(ACheeseStrikeAerialCheese, StartAt);
	DOREPLIFETIME(ACheeseStrikeAerialCheese, EndAt);
}

void ACheeseStrikeAerialCheese::BeginPlay()
{
	Super::BeginPlay();
	// The cheese material, without its carved holes, which would swim over a thing in flight; loaded here, as a script rebuilds it.
	if (UMaterialInterface* Cheese = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese")))
	{
		UMaterialInstanceDynamic* Plain = UMaterialInstanceDynamic::Create(Cheese, this);
		Plain->SetScalarParameterValue(TEXT("Holes"), 0.0f);
		Wheel->SetMaterial(0, Plain);
	}
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Dark = UMaterialInstanceDynamic::Create(Shape, this);
		Dark->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.035f));
		for (UStaticMeshComponent* Rotor : Rotors)
		{
			Rotor->SetMaterial(0, Dark);
		}
	}
}

float ACheeseStrikeAerialCheese::Now() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ACheeseStrikeAerialCheese::Fly(const FVector& InCentre, float InRadius, float From, float Until)
{
	Centre = InCentre;
	Radius = InRadius;
	StartAt = From;
	EndAt = Until;
	SetActorLocation(WhereAt(From));
}

FVector ACheeseStrikeAerialCheese::WhereAt(float Time) const
{
	const float Angle = 2.0f * PI * (Time - StartAt) / CheeseStrikeStreaks::DroneCircle;
	return FVector(Centre) + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
}

void ACheeseStrikeAerialCheese::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Time = Now();
	if (HasAuthority() && EndAt > 0.0f && Time > EndAt)
	{
		Destroy();
		return;
	}
	// Round the circle, facing along it, banked a little into the turn.
	const FVector Here = WhereAt(Time);
	const FVector Ahead = WhereAt(Time + 0.5f);
	SetActorLocationAndRotation(Here, FRotator(0.0f, (Ahead - Here).Rotation().Yaw, -8.0f));
	for (int32 Corner = 0; Corner < Rotors.Num(); ++Corner)
	{
		Rotors[Corner]->AddLocalRotation(FRotator(0.0f, (Corner % 2 == 0 ? 1.0f : -1.0f) * 1440.0f * DeltaSeconds, 0.0f));
	}
	// Its light blinks once a second.
	Beacon->SetVisibility(FMath::Fmod(Time, 1.0f) < 0.25f);
}
