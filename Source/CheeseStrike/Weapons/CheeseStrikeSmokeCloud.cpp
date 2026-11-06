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
	/** The blood's mist card (a plane a metre across) and the smoke's own look for it (a puff of smoke, since wave 17). */
	const TCHAR* const SmokeCardPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* const SmokeLookPath = TEXT("/Game/CheeseStrike/Effects/M_SmokeMist.M_SmokeMist");
	/** How big a card is against the cloud's radius (its half width as a share), and how slowly the swirl turns (degrees a second, about). */
	constexpr float SmokeCardShare = 0.55f;
	constexpr float SmokeSwirlDegrees = 14.0f;
	/** How fast each card rolls over on the view (degrees a second, about), so that its billows turn as smoke's do. */
	constexpr float SmokeRollDegrees = 6.0f;
	/** The veil's lobes round its edge, which make its outline lumpy (how many, how far out as a share of its half size, and how big as a share of it). */
	constexpr int32 VeilLobes = 7;
	constexpr float VeilLobeOut = 0.78f;
	constexpr float VeilLobeSize = 0.42f;
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
	// Each card's thickness, how solid it is (the swirl's none, the veil's all) and its shade (lighter or darker).
	Cards->NumCustomDataFloats = 3;
	// Sorted by distance with the veil, which is nearer the eye: drawn after it, a card behind someone in the cloud would
	// be hidden by them where they stand, and so show their outline over the veil.
	Cards->SetAbsolute(false, true, true);

	// The veil, sorted by its own distance like everything else translucent, so that it is drawn over a ghost or any mist
	// behind it.
	Veil = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Veil"));
	Veil->SetupAttachment(Sight);
	Veil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Veil->SetCastShadow(false);
	Veil->SetCanEverAffectNavigation(false);
	Veil->NumCustomDataFloats = 3;
	Veil->SetAbsolute(false, true, true);
	Veil->SetCullDistances(0, 0);
	Veil->bUseAsOccluder = false;
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

int32 ACheeseStrikeSmokeCloud::GetVeilCount() const
{
	return Veil != nullptr ? Veil->GetInstanceCount() : 0;
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
		Veil->SetStaticMesh(Card);
		CardHalf = FMath::Max(1.0f, static_cast<float>(Card->GetBounds().BoxExtent.X));
	}
	if (UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, SmokeLookPath))
	{
		Cards->SetMaterial(0, Look);
		Veil->SetMaterial(0, Look);
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
		Card.Spin = FMath::DegreesToRadians(SmokeRollDegrees * Random.FRandRange(0.5f, 1.5f)) * (Random.FRand() < 0.5f ? -1.0f : 1.0f);
		Card.Shade = Random.FRandRange(0.72f, 1.25f);
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
		if (Veil->GetInstanceCount() > 0)
		{
			Veil->ClearInstances();
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
	if (bCamera)
	{
		ShowVeil(Camera, At, Opacity);
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
		const FQuat Turn = FRotationMatrix::MakeFromZ(Facing).ToQuat() * FQuat(FVector::UpVector, Card.Phase + Card.Spin * At);
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
		Cards->SetCustomDataValue(Index, 0, Opacity * Edge, false);
		Cards->SetCustomDataValue(Index, 1, 0.0f, false);
		Cards->SetCustomDataValue(Index, 2, Swirl[Index].Shade, Index == Shown.Num() - 1);
	}
}

void ACheeseStrikeSmokeCloud::ShowVeil(const FVector& Camera, float At, float Opacity)
{
	// Worked out from where it burst (the sphere's middle is CloudMiddleUp over it), for this machine's camera.
	const FVector Burst = Sight->GetComponentLocation() - FVector(0.0f, 0.0f, CheeseStrikeGrenades::CloudMiddleUp);
	const CheeseStrikeGrenades::FVeil Made = CheeseStrikeGrenades::CloudVeil(Camera - Burst, At);
	VeilShown.Reset(VeilLobes + 1);
	if (Made.HalfRight > 0.0f)
	{
		const FVector Facing = FVector::CrossProduct(Made.Right, Made.Up);
		const FQuat Turn = FRotationMatrix::MakeFromZY(-Facing, Made.Up).ToQuat();
		// Its whole middle over what it must hide: the card bigger by its whole share.
		const float Right = Made.HalfRight / (CheeseStrikeGrenades::CloudVeilWhole * CardHalf);
		const float Up = Made.HalfUp / (CheeseStrikeGrenades::CloudVeilWhole * CardHalf);
		const FVector Middle = Burst + Made.Middle;
		VeilShown.Add(FTransform(Turn, Middle, FVector(Right, Up, 1.0f)));
		if (!Made.bAtTheEye)
		{
			// Lobes round its edge, turning slowly, so that its outline is lumpy smoke rather than an ellipse; a little
			// nearer the eye, so that they lie over it.
			for (int32 Lobe = 0; Lobe < VeilLobes; ++Lobe)
			{
				const float Angle = 2.0f * PI * Lobe / VeilLobes + At * 0.05f * (Lobe % 2 == 0 ? 1.0f : -1.0f) + 0.4f * FMath::Sin(Lobe * 2.3f);
				const float Out = VeilLobeOut * (0.9f + 0.2f * FMath::Sin(Lobe * 1.7f + At * 0.3f));
				const FVector Place = Middle + Made.Right * FMath::Cos(Angle) * Made.HalfRight * Out + Made.Up * FMath::Sin(Angle) * Made.HalfUp * Out - Facing * 2.0f;
				const float Size = VeilLobeSize * (0.85f + 0.3f * FMath::Frac(Lobe * 0.618f));
				VeilShown.Add(FTransform(Turn, Place, FVector(Right * Size, Right * Size, 1.0f)));
			}
		}
	}
	if (VeilShown.Num() != Veil->GetInstanceCount())
	{
		Veil->ClearInstances();
		if (VeilShown.Num() > 0)
		{
			Veil->AddInstances(VeilShown, false, true, false);
		}
	}
	else if (VeilShown.Num() > 0)
	{
		Veil->BatchUpdateInstancesTransforms(0, VeilShown, true, true, true);
	}
	for (int32 Index = 0; Index < VeilShown.Num(); ++Index)
	{
		Veil->SetCustomDataValue(Index, 0, Opacity, false);
		Veil->SetCustomDataValue(Index, 1, 1.0f, false);
		Veil->SetCustomDataValue(Index, 2, 1.0f, Index == VeilShown.Num() - 1);
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
