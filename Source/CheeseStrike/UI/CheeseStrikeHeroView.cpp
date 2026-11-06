// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeHeroView.h"
#include "Rules/CheeseStrikeHeroTurn.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrikeMenuFigure.h"

namespace CheeseStrikeHeroView
{
	float VerticalFieldOfView(float Distance)
	{
		const float Half = HeroHeight / HeroShare / 2.0f;
		return FMath::RadiansToDegrees(2.0f * FMath::Atan(Half / FMath::Max(Distance, 1.0f)));
	}

	float HorizontalFieldOfView(float Vertical, float Aspect)
	{
		return FMath::RadiansToDegrees(2.0f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Vertical) / 2.0f) * Aspect));
	}
}

namespace
{
	/** The fog's sphere, big enough to hold the camera with room behind it (cm). */
	constexpr float HeroViewFogRadius = 1100.0f;
	/** Where the figure's feet are over its pedestal's middle (the pedestal is half its height over the middle, the body on it). */
	constexpr float HeroViewFeet = ACheeseStrikeMenuFigure::PedestalHeight / 2.0f;
}

const FVector ACheeseStrikeHeroView::StageAt(0.0f, 40000.0f, 45000.0f);

ACheeseStrikeHeroView::ACheeseStrikeHeroView()
{
	// Ticks while shown, for the figure's turn by itself to stop and come back about a turn by hand.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	Stage = CreateDefaultSubobject<USceneComponent>(TEXT("Stage"));
	RootComponent = Stage;

	// The fog: the engine's sphere (a metre across), scaled round everything, its inside drawn by M_HeroBackdrop (unlit,
	// two-sided; loaded in BeginPlay); it casts nothing, so the lights reach the figure inside it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Backdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Stage);
	Backdrop->SetStaticMesh(Sphere.Object);
	Backdrop->SetRelativeLocation(FVector(0.0f, 0.0f, HeroViewFeet + CheeseStrikeHeroView::HeroHeight / 2.0f));
	Backdrop->SetRelativeScale3D(FVector(HeroViewFogRadius / 50.0f));
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->SetCastShadow(false);
	Backdrop->bAffectDistanceFieldLighting = false;
	Backdrop->bAffectDynamicIndirectLighting = false;
	Backdrop->SetVisibleInRayTracing(false);

	// The camera in front of the figure (which faces -Y, as the front end's does), level with its middle.
	const FVector Middle(0.0f, 0.0f, HeroViewFeet + CheeseStrikeHeroView::HeroHeight * 0.5f);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Stage);
	Capture->SetRelativeLocation(Middle + FVector(0.0f, -CheeseStrikeHeroView::CameraDistance, 18.0f));
	Capture->SetRelativeRotation((Middle - (Middle + FVector(0.0f, -CheeseStrikeHeroView::CameraDistance, 18.0f))).Rotation());
	Capture->FOVAngle = CheeseStrikeHeroView::HorizontalFieldOfView(CheeseStrikeHeroView::VerticalFieldOfView(CheeseStrikeHeroView::CameraDistance), static_cast<float>(CheeseStrikeHeroView::PictureWidth) / CheeseStrikeHeroView::PictureHeight);
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	// Nothing of the sky or the level's fog: the sphere's own fog is the whole background.
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetMotionBlur(false);
	// Its own exposure (the level's adapts to its own light).
	Capture->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& Look = Capture->PostProcessSettings;
	Look.bOverride_AutoExposureMethod = true;
	Look.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Look.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Look.AutoExposureApplyPhysicalCameraExposure = false;
	Look.bOverride_AutoExposureBias = true;
	Look.AutoExposureBias = 0.0f;
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = 0.0f;
	Look.bOverride_BloomIntensity = true;
	Look.BloomIntensity = 0.0f;

	// The lights: a soft warm key from the front left above, a dim fill from the right, a cool rim from behind.
	Key = CreateDefaultSubobject<USpotLightComponent>(TEXT("Key"));
	Key->SetupAttachment(Stage);
	const FVector KeyFrom(-260.0f, -420.0f, 360.0f);
	Key->SetRelativeLocation(KeyFrom);
	Key->SetRelativeRotation((Middle - KeyFrom).Rotation());
	Key->SetIntensityUnits(ELightUnits::Candelas);
	Key->SetIntensity(900.0f);
	Key->SetAttenuationRadius(1600.0f);
	Key->SetInnerConeAngle(22.0f);
	Key->SetOuterConeAngle(40.0f);
	Key->SetSourceRadius(40.0f);
	Key->SetLightColor(FLinearColor(1.0f, 0.95f, 0.88f));
	Fill = CreateDefaultSubobject<UPointLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(Stage);
	Fill->SetRelativeLocation(FVector(320.0f, -380.0f, 160.0f));
	Fill->SetIntensityUnits(ELightUnits::Candelas);
	Fill->SetIntensity(140.0f);
	Fill->SetAttenuationRadius(1600.0f);
	Fill->SetCastShadows(false);
	Fill->SetLightColor(FLinearColor(0.9f, 0.94f, 1.0f));
	Rim = CreateDefaultSubobject<USpotLightComponent>(TEXT("Rim"));
	Rim->SetupAttachment(Stage);
	const FVector RimFrom(80.0f, 380.0f, 340.0f);
	Rim->SetRelativeLocation(RimFrom);
	Rim->SetRelativeRotation((Middle - RimFrom).Rotation());
	Rim->SetIntensityUnits(ELightUnits::Candelas);
	Rim->SetIntensity(700.0f);
	Rim->SetAttenuationRadius(1400.0f);
	Rim->SetInnerConeAngle(24.0f);
	Rim->SetOuterConeAngle(42.0f);
	Rim->SetCastShadows(false);
	Rim->SetLightColor(FLinearColor(0.82f, 0.9f, 1.0f));
}

void ACheeseStrikeHeroView::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Fog = LoadObject<UMaterialInterface>(nullptr, BackdropPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Backdrop->SetMaterial(0, Fog);
	}
	Picture = UKismetRenderingLibrary::CreateRenderTarget2D(this, CheeseStrikeHeroView::PictureWidth, CheeseStrikeHeroView::PictureHeight, ETextureRenderTargetFormat::RTF_RGBA8_SRGB, FLinearColor(0.86f, 0.86f, 0.86f, 1.0f));
	Capture->TextureTarget = Picture;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.Owner = this;
	// Facing the camera, a little turned (the front end's figure, spawned at -90, shows its side to its camera), and turning
	// slower than the front end's figure, so that the hero is seen mostly from the front.
	Figure = GetWorld()->SpawnActor<ACheeseStrikeMenuFigure>(ACheeseStrikeMenuFigure::StaticClass(), GetActorLocation(), FRotator(0.0f, CheeseStrikeHeroView::FacingYaw, 0.0f), Parameters);
	Capture->ShowOnlyComponent(Backdrop);
	if (Figure != nullptr)
	{
		Figure->SetTurnShare(CheeseStrikeHeroView::TurnShare);
		// The hero and what it holds, not its pedestal: no scene.
		Capture->ShowOnlyComponent(const_cast<USkeletalMeshComponent*>(Figure->GetBody()));
		Capture->ShowOnlyComponent(const_cast<UStaticMeshComponent*>(Figure->GetWeapon()));
		Capture->ShowOnlyComponent(const_cast<UStaticMeshComponent*>(Figure->GetOffHand()));
	}
	SetShown(false);
}

void ACheeseStrikeHeroView::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Figure != nullptr)
	{
		Figure->Destroy();
		Figure = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ACheeseStrikeHeroView::Show(int32 Hero, FName Id)
{
	if (Figure != nullptr)
	{
		Figure->ShowHero(Hero);
		Figure->ShowPrimary(Id);
	}
}

void ACheeseStrikeHeroView::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Figure != nullptr)
	{
		Figure->SetTurnShare(CheeseStrikeHeroView::TurnShare * CheeseStrikeHeroTurn::AutoShare(TurnedAt < 0.0 ? -1.0 : FPlatformTime::Seconds() - TurnedAt));
	}
}

void ACheeseStrikeHeroView::TurnBy(float Degrees)
{
	TurnedAt = FPlatformTime::Seconds();
	if (Figure != nullptr)
	{
		Figure->SetTurnShare(0.0f);
		Figure->AddActorWorldRotation(FRotator(0.0f, Degrees, 0.0f));
	}
}

float ACheeseStrikeHeroView::GetFigureYaw() const
{
	return Figure != nullptr ? Figure->GetActorRotation().Yaw : 0.0f;
}

void ACheeseStrikeHeroView::SetShown(bool bInShown)
{
	bShown = bInShown;
	SetActorTickEnabled(bShown);
	Capture->bCaptureEveryFrame = bShown;
	SetActorHiddenInGame(!bShown);
	if (Figure != nullptr)
	{
		Figure->SetActorHiddenInGame(!bShown);
		Figure->SetActorTickEnabled(bShown);
	}
}
