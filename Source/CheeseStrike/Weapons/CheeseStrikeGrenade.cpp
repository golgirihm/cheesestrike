// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeGrenade.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePlayerState.h"
#include "CheeseStrikeGrenadeRules.h"

namespace
{
	/**
	 * Its look, loaded as it begins rather than held by the class, as scripts rebuild them: the wheel of cheese the maps
	 * stack (tools/art/wheel_mesh.py; a metre across and high), here a round 9 cm across and 4.5 cm thick, in the red wax
	 * of the cheese the mice leave alone (tools/art/wax_material.py), and the glow its spark burns with.
	 */
	const TCHAR* GrenadeWheelPath = TEXT("/Game/CheeseStrike/Meshes/SM_CheeseWheel.SM_CheeseWheel");
	const TCHAR* GrenadeWaxPath = TEXT("/Game/CheeseStrike/Materials/M_CheeseWax.M_CheeseWax");
	const TCHAR* GrenadeGlowPath = TEXT("/Game/CheeseStrike/Materials/M_Glow.M_Glow");
	const TCHAR* GrenadeScorchPath = TEXT("/Game/CheeseStrike/Materials/MI_Decal_OilStain.MI_Decal_OilStain");
	/** Lyra's grenade knocking on the ground (Content/LICENSES.md). */
	const TCHAR* const GrenadeKnockPaths[] = {
		TEXT("/Game/Audio/Sounds/Grenade/grenade_bounce_01.grenade_bounce_01"), TEXT("/Game/Audio/Sounds/Grenade/grenade_bounce_02.grenade_bounce_02"),
		TEXT("/Game/Audio/Sounds/Grenade/grenade_bounce_03.grenade_bounce_03"), TEXT("/Game/Audio/Sounds/Grenade/grenade_bounce_04.grenade_bounce_04"),
		TEXT("/Game/Audio/Sounds/Grenade/grenade_bounce_05.grenade_bounce_05")};
	const FVector GrenadeRoundScale(0.09f, 0.09f, 0.045f);
	/** The fuse: a short cord out of the top of the round, its spark at the end. */
	const FVector GrenadeWickScale(0.006f, 0.006f, 0.03f);
	const FVector GrenadeWickAt(0.0f, 0.0f, 3.5f);
	const FVector GrenadeSparkAt(0.0f, 0.0f, 5.2f);
	constexpr float GrenadeSparkSize = 0.015f;
	/** The burst: how long its flash and fireball last (s), how bright the flash is (candelas) and how far it reaches. */
	constexpr float GrenadeFlashTime = 0.4f;
	constexpr float GrenadeFlashBrightness = 1200.0f;
	constexpr float GrenadeFlashReach = 1200.0f;
	/** The scorch it leaves, how long and how big (cm). */
	constexpr float GrenadeScorchLife = 10.0f;
	constexpr float GrenadeScorchSize = 140.0f;
	/** Seconds it stays after its burst, for the burst to reach every machine and its flash to fade. */
	constexpr float GrenadeLinger = 1.5f;
	/** The template explosion material's one parameter, which breaks its shell up from 0 to 1. */
	const FName GrenadeShellParameter(TEXT("Param"));
	/** A knock is heard only for a bounce at least this fast into what it meets (cm/s), and not twice within so long (s). */
	constexpr float GrenadeKnockSpeed = 120.0f;
	constexpr float GrenadeKnockGap = 0.12f;
}

ACheeseStrikeGrenade::ACheeseStrikeGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.0f);

	// It bounces off the world and the cheese, but passes players and the mice, and never stops a shot or a look.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CheeseStrikeGrenades::CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 4000.0f;
	Movement->bShouldBounce = true;
	Movement->Bounciness = CheeseStrikeGrenades::Bounciness;
	Movement->Friction = CheeseStrikeGrenades::Friction;
	Movement->ProjectileGravityScale = CheeseStrikeGrenades::GravityScale;
	Movement->bRotationFollowsVelocity = false;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->BounceVelocityStopSimulatingThreshold = 15.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Explosion(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Projectiles/Materials/M_Explosion.M_Explosion"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch1(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_01.Explosions_Grenade_Punch-Close_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch2(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_02.Explosions_Grenade_Punch-Close_02"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch3(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_03.Explosions_Grenade_Punch-Close_03"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Tail(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_01.Explosions_Grenade_Noise-Exterior_01"));
	SphereMesh = Sphere.Object;
	FireballMaterial = Explosion.Object;
	for (USoundBase* Sound : {Punch1.Object, Punch2.Object, Punch3.Object})
	{
		if (Sound != nullptr)
		{
			Bangs.Add(Sound);
		}
	}
	Rumble = Tail.Object;

	const auto Part = [this](const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		return Mesh;
	};
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Collision);
	Round = Part(TEXT("Round"), Pivot);
	Round->SetStaticMesh(Cylinder.Object);
	Round->SetRelativeScale3D(GrenadeRoundScale);
	Wick = Part(TEXT("Wick"), Pivot);
	Wick->SetStaticMesh(Cylinder.Object);
	Wick->SetRelativeLocation(GrenadeWickAt);
	Wick->SetRelativeScale3D(GrenadeWickScale);
	Wick->SetCastShadow(false);
	Spark = Part(TEXT("Spark"), Pivot);
	Spark->SetStaticMesh(Sphere.Object);
	Spark->SetRelativeLocation(GrenadeSparkAt);
	Spark->SetRelativeScale3D(FVector(GrenadeSparkSize));
	Spark->SetCastShadow(false);
}

void ACheeseStrikeGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeGrenade, BurstsAt);
}

void ACheeseStrikeGrenade::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (!bBurst)
	{
		Movement->Velocity = NewVelocity;
	}
}

float ACheeseStrikeGrenade::Now() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ACheeseStrikeGrenade::Launch(ACheeseStrikeCharacter* InThrower, const FVector& Velocity)
{
	Thrower = InThrower != nullptr ? InThrower->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	BurstsAt = Now() + CheeseStrikeGrenades::Fuse;
	Movement->Velocity = Velocity;
	Movement->UpdateComponentVelocity();
	UE_LOG(LogCheeseStrike, Display, TEXT("Grenade: %s throws one from (%.0f, %.0f, %.0f) at %.0f cm/s"), InThrower != nullptr ? *InThrower->DisplayName() : TEXT("nobody"),
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, Velocity.Size());
}

void ACheeseStrikeGrenade::BeginPlay()
{
	Super::BeginPlay();
	Movement->OnProjectileBounce.AddDynamic(this, &ACheeseStrikeGrenade::OnBounce);
	// Each spins its own way as it flies, mostly end over end.
	Spin = FRotator(FMath::FRandRange(-720.0f, -420.0f), FMath::FRandRange(-90.0f, 90.0f), FMath::FRandRange(-160.0f, 160.0f));
	if (UStaticMesh* Wheel = LoadObject<UStaticMesh>(nullptr, GrenadeWheelPath))
	{
		Round->SetStaticMesh(Wheel);
	}
	if (UMaterialInterface* Wax = LoadObject<UMaterialInterface>(nullptr, GrenadeWaxPath))
	{
		Round->SetMaterial(0, Wax);
	}
	if (UMaterialInterface* BasicShape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Cord = UMaterialInstanceDynamic::Create(BasicShape, this);
		Cord->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.06f, 0.04f, 0.025f));
		Wick->SetMaterial(0, Cord);
	}
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, GrenadeGlowPath))
	{
		SparkMaterial = UMaterialInstanceDynamic::Create(Glow, this);
		SparkMaterial->SetVectorParameterValue(TEXT("Colour"), FLinearColor(1.0f, 0.42f, 0.08f));
		SparkMaterial->SetScalarParameterValue(TEXT("Strength"), 3.0f);
		Spark->SetMaterial(0, SparkMaterial);
	}
	for (const TCHAR* Path : GrenadeKnockPaths)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
		{
			Knocks.Add(Sound);
		}
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(400.0f);
	Attenuation->Attenuation.FalloffDistance = 6000.0f;
}

void ACheeseStrikeGrenade::OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity)
{
	++Bounces;
	const float Into = FMath::Abs(FVector::DotProduct(ImpactVelocity, Hit.ImpactNormal));
	const float Time = GetWorld()->GetTimeSeconds();
	// Rolling to rest, it slows its spin to the speed it goes.
	Spin *= 0.6f;
	if (GetNetMode() == NM_DedicatedServer || Knocks.Num() == 0 || Into < GrenadeKnockSpeed || Time - LastKnockAt < GrenadeKnockGap)
	{
		return;
	}
	LastKnockAt = Time;
	const float Loud = FMath::Clamp(Into / 900.0f, 0.25f, 1.0f);
	UGameplayStatics::PlaySoundAtLocation(this, Knocks[FMath::RandHelper(Knocks.Num())], Hit.ImpactPoint, FRotator::ZeroRotator, Loud, FMath::FRandRange(0.95f, 1.1f), 0.0f, Attenuation);
}

float ACheeseStrikeGrenade::HarmAt(UWorld* World, const FVector& At, const ACheeseStrikeCharacter* Victim, float& OutDistance)
{
	OutDistance = TNumericLimits<float>::Max();
	if (World == nullptr || Victim == nullptr || !Victim->IsAlive())
	{
		return 0.0f;
	}
	const FVector Head = Victim->GetEyes()->GetComponentLocation();
	const FVector Chest = Victim->ChestAt();
	const FVector Feet = Victim->IsDowned() ? Victim->GetDownShape().Feet : Victim->GetActorLocation() - FVector(0.0f, 0.0f, 70.0f);
	OutDistance = FMath::Min3(FVector::Dist(At, Head), FVector::Dist(At, Chest), FVector::Dist(At, Feet));
	const float Damage = CheeseStrikeGrenades::DamageAt(OutDistance);
	if (Damage <= 0.0f)
	{
		return 0.0f;
	}
	// A wall, a floor or the cheese between them saves them: the burst must see some of them.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeGrenadeBurst), false);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	for (const FVector& To : {Chest, Head, Feet})
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, At, To, ECC_Visibility, Params))
		{
			return Damage;
		}
	}
	return 0.0f;
}

void ACheeseStrikeGrenade::Burst()
{
	bBurst = true;
	UWorld* World = GetWorld();
	ACheeseStrikePlayerState* By = Thrower.Get();
	// A little above where it lies, so that the floor it is on hides nobody.
	const FVector At = GetActorLocation() + FVector(0.0f, 0.0f, 15.0f);
	UE_LOG(LogCheeseStrike, Display, TEXT("Grenade: %s's bursts at (%.0f, %.0f, %.0f) after %d bounces"), By != nullptr ? *By->GetPlayerName() : TEXT("nobody"), At.X, At.Y, At.Z, Bounces);
	if (World == nullptr || By == nullptr)
	{
		return;
	}
	TArray<ACheeseStrikeCharacter*> Near;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Near.Add(*It);
	}
	for (ACheeseStrikeCharacter* Victim : Near)
	{
		float Distance = 0.0f;
		const float Damage = HarmAt(World, At, Victim, Distance);
		if (Damage > 0.0f && Victim->TakeBlast(Damage, By, CheeseStrikeGrenades::Name))
		{
			++Hits;
			UE_LOG(LogCheeseStrike, Display, TEXT("Grenade: %s's hit %s at %.0f cm for %.0f (health %.0f)"), *By->GetPlayerName(), *Victim->DisplayName(), Distance, Damage, Victim->GetHealth());
		}
	}
}

void ACheeseStrikeGrenade::MulticastBurst_Implementation(FVector_NetQuantize At)
{
	bBurst = true;
	Pivot->SetVisibility(false, true);
	Movement->StopMovementImmediately();
	ShowBurst(At);
}

void ACheeseStrikeGrenade::ShowBurst(const FVector& At)
{
	if (GetNetMode() == NM_DedicatedServer || BurstShownAt >= 0.0f)
	{
		return;
	}
	BurstShownAt = GetWorld()->GetTimeSeconds();
	if (Bangs.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Bangs[FMath::RandHelper(Bangs.Num())], At, FRotator::ZeroRotator, 1.0f, FMath::FRandRange(0.95f, 1.08f), 0.0f, Attenuation);
	}
	if (Rumble != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rumble, At, FRotator::ZeroRotator, 0.6f, FMath::FRandRange(0.9f, 1.05f), 0.0f, Attenuation);
	}
	FlashLight = NewObject<UPointLightComponent>(this);
	FlashLight->SetMobility(EComponentMobility::Movable);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(GrenadeFlashBrightness);
	FlashLight->SetAttenuationRadius(GrenadeFlashReach);
	FlashLight->SetLightColor(FLinearColor(1.0f, 0.62f, 0.25f));
	FlashLight->SetCastShadows(false);
	FlashLight->SetupAttachment(RootComponent);
	FlashLight->RegisterComponent();
	FlashLight->SetWorldLocation(At + FVector(0.0f, 0.0f, 80.0f));
	if (SphereMesh != nullptr)
	{
		Fireball = NewObject<UStaticMeshComponent>(this);
		Fireball->SetStaticMesh(SphereMesh);
		if (FireballMaterial != nullptr)
		{
			// The template's explosion: an unlit shell that breaks up as its Param goes from 0 to 1.
			UMaterialInstanceDynamic* Shell = UMaterialInstanceDynamic::Create(FireballMaterial, this);
			Shell->SetScalarParameterValue(GrenadeShellParameter, 0.0f);
			Fireball->SetMaterial(0, Shell);
		}
		Fireball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fireball->SetCastShadow(false);
		Fireball->SetupAttachment(RootComponent);
		Fireball->RegisterComponent();
		Fireball->SetWorldLocation(At);
		Fireball->SetWorldScale3D(FVector(0.3f));
	}
	if (UMaterialInterface* Stain = LoadObject<UMaterialInterface>(nullptr, GrenadeScorchPath))
	{
		// The Creamery's oil stain, darkened, facing down onto the ground, as the streaks' bursts leave.
		UMaterialInstanceDynamic* Scorch = UMaterialInstanceDynamic::Create(Stain, this);
		Scorch->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.05f, 0.04f, 0.03f));
		Scorch->SetScalarParameterValue(TEXT("Desaturate"), 1.0f);
		Scorch->SetScalarParameterValue(TEXT("Brightness"), 0.35f);
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, Scorch, FVector(60.0f, GrenadeScorchSize, GrenadeScorchSize), At, FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), GrenadeScorchLife))
		{
			Decal->SetFadeOut(GrenadeScorchLife - 2.0f, 2.0f, false);
		}
	}
}

void ACheeseStrikeGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBurst)
	{
		// Spinning as it flies; the spark flickering as the fuse burns down.
		const float Speed = Movement->Velocity.Size();
		Pivot->AddLocalRotation(Spin * DeltaSeconds * FMath::Clamp(Speed / 400.0f, 0.0f, 1.0f));
		// Rolling along the ground, it slows to a stop.
		if (!Movement->Velocity.IsNearlyZero() && Movement->UpdatedComponent != nullptr && Movement->Velocity.Z < 60.0f)
		{
			FCollisionObjectQueryParams Solid;
			Solid.AddObjectTypesToQuery(ECC_WorldStatic);
			Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeGrenadeGround), false, this);
			FHitResult Ground;
			const FVector From = GetActorLocation();
			if (GetWorld()->LineTraceSingleByObjectType(Ground, From, From - FVector(0.0f, 0.0f, CheeseStrikeGrenades::CollisionRadius + 3.0f), Solid, Params))
			{
				const float Was = Movement->Velocity.Size();
				const float Left = FMath::Max(0.0f, Was - CheeseStrikeGrenades::RollDrag * DeltaSeconds);
				if (Left < 15.0f)
				{
					Movement->StopSimulating(Ground);
				}
				else
				{
					Movement->Velocity *= Left / Was;
				}
			}
		}
		const float Flicker = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 47.0f) * FMath::Sin(GetWorld()->GetTimeSeconds() * 13.0f);
		Spark->SetRelativeScale3D(FVector(GrenadeSparkSize * (0.8f + 0.4f * Flicker)));
		if (SparkMaterial != nullptr)
		{
			SparkMaterial->SetScalarParameterValue(TEXT("Strength"), 1.0f + 3.0f * Flicker);
		}
		if (HasAuthority() && BurstsAt > 0.0f && Now() >= BurstsAt)
		{
			Burst();
			MulticastBurst(GetActorLocation() + FVector(0.0f, 0.0f, 15.0f));
			SetLifeSpan(GrenadeLinger);
		}
		return;
	}
	// The flash fades and the fireball swells to where the harm stops, breaking up as it goes.
	if (BurstShownAt < 0.0f)
	{
		return;
	}
	const float Along = (GetWorld()->GetTimeSeconds() - BurstShownAt) / GrenadeFlashTime;
	if (Along >= 1.0f)
	{
		if (FlashLight != nullptr)
		{
			FlashLight->DestroyComponent();
			FlashLight = nullptr;
		}
		if (Fireball != nullptr)
		{
			Fireball->DestroyComponent();
			Fireball = nullptr;
		}
		return;
	}
	if (FlashLight != nullptr)
	{
		FlashLight->SetIntensity(GrenadeFlashBrightness * FMath::Square(1.0f - Along));
	}
	if (Fireball != nullptr)
	{
		// The engine's sphere is 1 m across.
		const float Across = FMath::Lerp(0.3f, CheeseStrikeGrenades::Radius / 50.0f * 0.75f, FMath::Sqrt(Along));
		Fireball->SetWorldScale3D(FVector(Across, Across, Across * 0.7f));
		if (UMaterialInstanceDynamic* Shell = Cast<UMaterialInstanceDynamic>(Fireball->GetMaterial(0)))
		{
			Shell->SetScalarParameterValue(GrenadeShellParameter, Along);
		}
	}
}
