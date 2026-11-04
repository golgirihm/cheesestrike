// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeSmokeCloud.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "CheeseStrike.h"
#include "CheeseStrikeGrenadeRules.h"
#include "Streaks/CheeseStrikeSynth.h"

namespace
{
	/** The blood's mist card (a plane a metre across) and the smoke's own look for it. */
	const TCHAR* const SmokeCardPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* const SmokeLookPath = TEXT("/Game/CheeseStrike/Effects/M_SmokeMist.M_SmokeMist");
	/** How big a card is against the cloud's radius (its half width as a share), and how slowly the swirl turns (degrees a second, about). */
	constexpr float SmokeCardShare = 0.55f;
	constexpr float SmokeSwirlDegrees = 14.0f;
	/** How loud the hiss is, and how far it is heard (cm). */
	constexpr float SmokeHissVolume = 0.5f;
	constexpr float SmokeHissReach = 2500.0f;

	TAutoConsoleVariable<float> CVarSmokeSeconds(TEXT("CheeseStrike.SmokeSeconds"), 0.0f,
		TEXT("How long a smoke cloud lasts, in place of CheeseStrikeGrenades::CloudLife (12 s), for the play tests; 0 for the rules'."), ECVF_Cheat);
}

ACheeseStrikeSmokeCloud::ACheeseStrikeSmokeCloud()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5.0f);

	// The sphere stops only looking: the Visibility channel, which bots' eyes, name tags and shots trace by (the shots
	// leave it out); nothing bumps into it.
	Sight = CreateDefaultSubobject<USphereComponent>(TEXT("Sight"));
	Sight->InitSphereRadius(CheeseStrikeGrenades::CloudSightRadius);
	Sight->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sight->SetCollisionObjectType(ECC_WorldDynamic);
	Sight->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sight->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Sight->SetCanEverAffectNavigation(false);
	Sight->SetGenerateOverlapEvents(false);
	Sight->SetRelativeLocation(FVector(0.0f, 0.0f, CheeseStrikeGrenades::CloudMiddleUp));
	RootComponent = Sight;

	Cards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Cards"));
	Cards->SetupAttachment(Sight);
	Cards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cards->SetCastShadow(false);
	Cards->SetCanEverAffectNavigation(false);
	Cards->NumCustomDataFloats = 1;
	Cards->SetAbsolute(false, true, true);
}

void ACheeseStrikeSmokeCloud::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeSmokeCloud, BeganAt);
}

float ACheeseStrikeSmokeCloud::Life()
{
	const float Shortened = CVarSmokeSeconds.GetValueOnGameThread();
	return Shortened > 0.0f ? Shortened : CheeseStrikeGrenades::CloudLife;
}

void ACheeseStrikeSmokeCloud::IgnoreClouds(FCollisionQueryParams& Params, const UWorld* World)
{
	for (TActorIterator<ACheeseStrikeSmokeCloud> It(const_cast<UWorld*>(World)); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
}

void ACheeseStrikeSmokeCloud::Begin(float At)
{
	BeganAt = At;
	SetLifeSpan(Life() + 0.5f);
}

float ACheeseStrikeSmokeCloud::Age() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	const float Now = State != nullptr ? static_cast<float>(State->GetServerWorldTimeSeconds()) : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
	// Scaled to the rules' life, so that a shortened cloud grows and thins as the real one does, only sooner.
	return BeganAt > 0.0f ? (Now - BeganAt) * CheeseStrikeGrenades::CloudLife / Life() : 0.0f;
}

bool ACheeseStrikeSmokeCloud::BlocksSight() const
{
	return BeganAt > 0.0f && CheeseStrikeGrenades::CloudBlocksSightAt(Age());
}

int32 ACheeseStrikeSmokeCloud::GetCardCount() const
{
	return Cards != nullptr ? Cards->GetInstanceCount() : 0;
}

void ACheeseStrikeSmokeCloud::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	MakeCards();
	// A soft hiss for as long as it smokes, from where it lies.
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.0f);
	Attenuation->Attenuation.FalloffDistance = SmokeHissReach;
	if (USoundWaveProcedural* Wave = CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Hiss(Life())))
	{
		Hiss = UGameplayStatics::SpawnSoundAttached(Wave, Sight, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, SmokeHissVolume, 1.0f, 0.0f, Attenuation);
	}
}

void ACheeseStrikeSmokeCloud::MakeCards()
{
	if (bLooksLoaded)
	{
		return;
	}
	bLooksLoaded = true;
	if (UStaticMesh* Card = LoadObject<UStaticMesh>(nullptr, SmokeCardPath))
	{
		Cards->SetStaticMesh(Card);
		CardHalf = FMath::Max(1.0f, static_cast<float>(Card->GetBounds().BoxExtent.X));
	}
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, SmokeLookPath))
	{
		Cards->SetMaterial(0, Look);
	}
	// Each card its own place in the swirl, the same on every machine for the same cloud (seeded by where it is).
	FRandomStream Random(static_cast<int32>(GetActorLocation().X * 7.0f + GetActorLocation().Y * 13.0f));
	Swirl.Reset(CheeseStrikeGrenades::CloudCards);
	for (int32 Index = 0; Index < CheeseStrikeGrenades::CloudCards; ++Index)
	{
		FCard Card;
		// Spread through a flattened ball: more cards low and wide, as smoke that has rolled out along the ground.
		Card.Out = Random.GetUnitVector();
		Card.Out.Z *= 0.6f;
		Card.Out.Normalize();
		Card.Share = FMath::Sqrt(Random.FRandRange(0.05f, 1.0f));
		Card.Size = Random.FRandRange(0.7f, 1.3f);
		Card.Turn = FMath::DegreesToRadians(SmokeSwirlDegrees * Random.FRandRange(0.6f, 1.4f)) * (Random.FRand() < 0.5f ? -1.0f : 1.0f);
		Card.Phase = Random.FRandRange(0.0f, 2.0f * PI);
		Swirl.Add(Card);
	}
}

void ACheeseStrikeSmokeCloud::ShowCards()
{
	const float At = Age();
	const float Radius = CheeseStrikeGrenades::CloudRadiusAt(At);
	const float Opacity = CheeseStrikeGrenades::CloudOpacityAt(At);
	if (Radius <= 0.0f || Opacity <= 0.0f)
	{
		if (Cards->GetInstanceCount() > 0)
		{
			Cards->ClearInstances();
		}
		return;
	}
	FVector Camera = FVector::ZeroVector;
	bool bCamera = false;
	if (const APlayerController* Player = GetWorld()->GetFirstPlayerController(); Player != nullptr && Player->PlayerCameraManager != nullptr)
	{
		Camera = Player->PlayerCameraManager->GetCameraLocation();
		bCamera = true;
	}
	const FVector Middle = Sight->GetComponentLocation();
	const float CardScale = Radius * SmokeCardShare / CardHalf;
	Shown.Reset(Swirl.Num());
	for (const FCard& Card : Swirl)
	{
		// Turned slowly about the middle, and breathing a little in and out, each card turned to this machine's camera.
		const FVector Turned = Card.Out.RotateAngleAxis(FMath::RadiansToDegrees(Card.Turn * At), FVector::UpVector);
		const float Breath = 1.0f + 0.08f * FMath::Sin(At * 0.9f + Card.Phase);
		const FVector Place = Middle + Turned * Radius * Card.Share * Breath;
		const FVector Facing = bCamera ? (Camera - Place).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector) : FVector::UpVector;
		const FQuat Turn = FRotationMatrix::MakeFromZ(Facing).ToQuat() * FQuat(FVector::UpVector, Card.Phase);
		Shown.Add(FTransform(Turn, Place, FVector(CardScale * Card.Size, CardScale * Card.Size, 1.0f)));
	}
	if (Shown.Num() != Cards->GetInstanceCount())
	{
		Cards->ClearInstances();
		Cards->AddInstances(Shown, false, true, false);
	}
	else
	{
		Cards->BatchUpdateInstancesTransforms(0, Shown, true, true, true);
	}
	// Thinner toward the edge of the swirl, and all of it thinning away at the end.
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		const float Edge = 1.0f - 0.45f * FMath::Square(Swirl[Index].Share);
		Cards->SetCustomDataValue(Index, 0, Opacity * Edge, Index == Shown.Num() - 1);
	}
}

void ACheeseStrikeSmokeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The sphere stops sight while the smoke is thick; thinning, it lets it through again (on the server for the bots, here for the tags).
	const bool bBlocks = BlocksSight();
	if (bBlocks != bSightOn)
	{
		bSightOn = bBlocks;
		Sight->SetCollisionEnabled(bBlocks ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		ShowCards();
	}
}
