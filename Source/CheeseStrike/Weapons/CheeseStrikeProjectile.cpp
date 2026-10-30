// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeArsenal.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGrenade.h"
#include "CheeseStrikeGrenadeRules.h"
#include "CheeseStrikePlayerState.h"
#include "CheeseStrikeShot.h"
#include "CheeseStrikeWeapon.h"
#include "CheeseStrikeWeaponLooks.h"
#include "Map/CheeseStrikeMouse.h"
#include "Modes/CheeseStrikeLastStand.h"

namespace
{
	/** The burst's look: how long its flash and fireball last (s), how bright the flash is (candelas) and how far it reaches past the burst. */
	constexpr float ProjectileFlashTime = 0.45f;
	constexpr float ProjectileFlashBrightness = 2400.0f;
	constexpr float ProjectileFlashReach = 1600.0f;
	/** The scorch it leaves, how long (s), and the burst's own size and how long the actor stays after it. */
	constexpr float ProjectileScorchLife = 10.0f;
	constexpr float ProjectileLinger = 1.5f;
	/** A shard stays stuck in a player for so long, and in the arena for less. */
	constexpr float ProjectileStuckInBody = 1.0f;
	constexpr float ProjectileStuckInWall = 0.6f;
	const TCHAR* ProjectileScorchPath = TEXT("/Game/CheeseStrike/Materials/MI_Decal_OilStain.MI_Decal_OilStain");
	const FName ProjectileShellParameter(TEXT("Param"));

	FVector ProjectileFeetOf(const ACheeseStrikeCharacter* Character)
	{
		return Character->GetActorLocation() - FVector(0.0f, 0.0f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
}

ACheeseStrikeProjectile::ACheeseStrikeProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetNetCullDistanceSquared(FMath::Square(15000.0f));

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	RootComponent = Pivot;
	Round = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Round"));
	Round->SetupAttachment(Pivot);
	Round->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Round->SetCanEverAffectNavigation(false);
	Round->SetCastShadow(false);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Pivot);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(30.0f);
	Glow->SetAttenuationRadius(250.0f);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);

	// What it may look like (the engine's shapes, which its look shapes and colours) and its burst: the grenade's.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Explosion(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Projectiles/Materials/M_Explosion.M_Explosion"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch1(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_01.Explosions_Grenade_Punch-Close_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch2(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_02.Explosions_Grenade_Punch-Close_02"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch3(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_03.Explosions_Grenade_Punch-Close_03"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Tail(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_01.Explosions_Grenade_Noise-Exterior_01"));
	SphereMesh = Sphere.Object;
	CylinderMesh = Cylinder.Object;
	CubeMesh = Cube.Object;
	FireballMaterial = Explosion.Object;
	for (USoundBase* Sound : {Punch1.Object, Punch2.Object, Punch3.Object})
	{
		if (Sound != nullptr)
		{
			Bangs.Add(Sound);
		}
	}
	Rumble = Tail.Object;
	Round->SetStaticMesh(Sphere.Object);
	Round->SetRelativeScale3D(FVector(0.05f));
}

void ACheeseStrikeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACheeseStrikeProjectile, WeaponId, COND_InitialOnly);
	DOREPLIFETIME(ACheeseStrikeProjectile, Homing);
	DOREPLIFETIME(ACheeseStrikeProjectile, bDone);
}

float ACheeseStrikeProjectile::Now() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

void ACheeseStrikeProjectile::Launch(ACheeseStrikeWeapon* InWeapon, ACheeseStrikeCharacter* InShooter, const FVector& Velocity, ACheeseStrikeCharacter* InHoming)
{
	Weapon = InWeapon;
	WeaponId = InWeapon != nullptr ? InWeapon->GetWeaponId() : NAME_None;
	Stats = CheeseStrikeArsenal::Find(WeaponId);
	Shooter = InShooter;
	By = InShooter != nullptr ? InShooter->GetPlayerState<ACheeseStrikePlayerState>() : nullptr;
	Flight = Velocity;
	Homing = Stats != nullptr && Stats->HomingTurn > 0.0f ? InHoming : nullptr;
	FiredAt = Now();
	FiredFrom = GetActorLocation();
	RootComponent->ComponentVelocity = Flight;
}

void ACheeseStrikeProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (Stats == nullptr)
	{
		Stats = CheeseStrikeArsenal::Find(WeaponId);
	}
	if (Stats == nullptr)
	{
		// Nothing of the arsenal's: it shouldn't be.
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	ApplyLook();
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(500.0f);
	Attenuation->Attenuation.FalloffDistance = 7000.0f;
	if (HasAuthority())
	{
		// A round that never meets anything is gone some time after its life (which ends it in Fly).
		SetLifeSpan(Stats->ProjectileLife + 2.0f);
	}
}

void ACheeseStrikeProjectile::ApplyLook()
{
	const FWeaponLook& Look = CheeseStrikeWeaponLooks::For(WeaponId);
	UStaticMesh* Model = Look.ProjectileModel != nullptr ? LoadObject<UStaticMesh>(nullptr, Look.ProjectileModel) : nullptr;
	Round->SetStaticMesh(Model != nullptr ? Model : SphereMesh.Get());
	Round->SetRelativeTransform(Look.ProjectileShape);
	if (UMaterialInterface* Material = Look.ProjectileMaterial != nullptr ? LoadObject<UMaterialInterface>(nullptr, Look.ProjectileMaterial) : nullptr)
	{
		Round->SetMaterial(0, Material);
	}
	Spin = Look.ProjectileSpin;
	if (Look.ProjectileGlow.A > 0.0f && GetNetMode() != NM_DedicatedServer)
	{
		Glow->SetLightColor(Look.ProjectileGlow);
		Glow->SetVisibility(true);
	}
}

void ACheeseStrikeProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (!bDone)
	{
		Flight = NewVelocity;
	}
}

void ACheeseStrikeProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Stats == nullptr)
	{
		return;
	}
	if (!bDone)
	{
		if (HasAuthority())
		{
			Fly(DeltaSeconds);
		}
		else
		{
			// As the server flies it, between its updates.
			if (Homing != nullptr && Homing->IsAlive())
			{
				Flight = CheeseStrikeWeapons::TurnToward(Flight, Homing->ChestAt() - GetActorLocation(), Stats->HomingTurn * DeltaSeconds);
			}
			Flight.Z += GetWorld()->GetGravityZ() * Stats->GravityShare * DeltaSeconds;
			SetActorLocation(GetActorLocation() + Flight * DeltaSeconds);
		}
		if (!Flight.IsNearlyZero())
		{
			SetActorRotation(Flight.Rotation());
		}
		if (Spin != 0.0f)
		{
			Round->AddRelativeRotation(FRotator(0.0f, 0.0f, Spin * DeltaSeconds));
		}
	}
	else if (GetAttachParentActor() == nullptr && BurstShownAt < 0.0f)
	{
		Glow->SetVisibility(false);
	}
	if (BurstShownAt < 0.0f)
	{
		return;
	}
	// The flash fades and the fireball swells to where the burst's harm stops, breaking up as it goes.
	const float Along = (GetWorld()->GetTimeSeconds() - BurstShownAt) / ProjectileFlashTime;
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
		FlashLight->SetIntensity(ProjectileFlashBrightness * FMath::Square(1.0f - Along));
	}
	if (Fireball != nullptr)
	{
		// The engine's sphere is 1 m across.
		const float Across = FMath::Lerp(0.4f, Stats->SplashRadius / 50.0f * 0.7f, FMath::Sqrt(Along));
		Fireball->SetWorldScale3D(FVector(Across, Across, Across * 0.75f));
		if (UMaterialInstanceDynamic* Shell = Cast<UMaterialInstanceDynamic>(Fireball->GetMaterial(0)))
		{
			Shell->SetScalarParameterValue(ProjectileShellParameter, Along);
		}
	}
}

void ACheeseStrikeProjectile::Fly(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const FVector From = GetActorLocation();
	if (Homing != nullptr && Homing->IsAlive())
	{
		Flight = CheeseStrikeWeapons::TurnToward(Flight, Homing->ChestAt() - From, Stats->HomingTurn * DeltaSeconds);
	}
	Flight.Z += World->GetGravityZ() * Stats->GravityShare * DeltaSeconds;
	RootComponent->ComponentVelocity = Flight;
	const FVector To = From + Flight * DeltaSeconds;
	const float Length = FVector::Dist(From, To);
	if (Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Way = (To - From) / Length;

	// The arena, every player left out: they are tested where they are.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeProjectile), true, this);
	TArray<ACheeseStrikeCharacter*> Players;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Players.Add(*It);
		Params.AddIgnoredActor(*It);
	}
	FHitResult Wall;
	const bool bWall = World->LineTraceSingleByChannel(Wall, From, To, ECC_Visibility, Params);
	float Nearest = bWall ? Wall.Distance : Length;
	ACheeseStrikeCharacter* Victim = nullptr;
	bool bHead = false;
	const float Time = Now();
	for (ACheeseStrikeCharacter* Player : Players)
	{
		// Never its own shooter outright: only its burst.
		if (Player == Shooter.Get() || !Player->IsAlive())
		{
			continue;
		}
		bool bThisHead = false;
		const float Along = FHitHistory::RayHitsPlayer(From, Way, Player->SampleNow(Time), bThisHead);
		if (Along >= 0.0f && Along < Nearest)
		{
			Nearest = Along;
			Victim = Player;
			bHead = bThisHead;
		}
	}
	if (Victim != nullptr)
	{
		const FVector At = From + Way * Nearest;
		SetActorLocation(At);
		HitPlayer(Victim, At, bHead, Way);
		return;
	}
	if (bWall)
	{
		SetActorLocation(Wall.ImpactPoint - Way * 3.0f);
		// A mouse, or anything else of the engine's that takes damage, takes a hit's.
		AActor* Thing = Wall.GetActor();
		if (Thing != nullptr && (Thing->IsA<APawn>() || Thing->IsA<ACheeseStrikeMouse>()))
		{
			ACheeseStrikeCharacter* Firer = Shooter.Get();
			UGameplayStatics::ApplyPointDamage(Thing, Stats->Damage, Way, Wall, Firer != nullptr ? Firer->GetController() : nullptr, this, nullptr);
		}
		// Out of a wall, so that its burst sees the room it is in.
		End(Wall.ImpactPoint + Wall.ImpactNormal * 10.0f, true);
		return;
	}
	SetActorLocation(To);
	if (Time - FiredAt > Stats->ProjectileLife)
	{
		End(To, true);
	}
}

void ACheeseStrikeProjectile::HitPlayer(ACheeseStrikeCharacter* Victim, const FVector& At, bool bHead, const FVector& Way)
{
	Struck = Victim;
	ACheeseStrikeCharacter* Firer = Shooter.IsValid() && Shooter->IsAlive() ? Shooter.Get() : nullptr;
	ACheeseStrikeWeapon* Gun = Weapon.Get();
	const float Damage = Stats->Damage * (bHead ? Stats->HeadMultiplier : 1.0f);
	bool bHurt = false;
	if (Firer != nullptr)
	{
		Victim->NoteHit(WeaponId, FVector::Dist(FiredFrom, At), false);
		bHurt = Victim->TakeShot(Damage, Firer, bHead);
	}
	else if (By.IsValid())
	{
		bHurt = Victim->TakeBlast(Damage, By.Get(), Stats->DisplayName.ToString());
	}
	if (bHurt)
	{
		if (Gun != nullptr)
		{
			Gun->ConfirmHit(Damage, bHead, !Victim->IsAlive());
		}
		Victim->MulticastBleed(At - ProjectileFeetOf(Victim), Way, static_cast<uint8>(FMath::Clamp(Damage, 1.0f, 255.0f)));
		UE_LOG(LogCheeseStrike, Display, TEXT("Projectile: %s's %s hit %s%s for %.0f (health %.0f)"), Firer != nullptr ? *Firer->DisplayName() : TEXT("someone gone"),
			*Stats->DisplayName.ToString(), *Victim->DisplayName(), bHead ? TEXT(" on the head") : TEXT(""), Damage, Victim->GetHealth());
		// The supercombine: enough shards in one player in a short while burst for a kill.
		if (Stats->SuperShards > 0 && Gun != nullptr && Victim->IsAlive() && Gun->AddShard(Victim, Now()))
		{
			bool bBurst = false;
			if (Firer != nullptr)
			{
				Victim->NoteHit(WeaponId, FVector::Dist(FiredFrom, At), false);
				bBurst = Victim->TakeShot(CheeseStrikeWeapons::SuperDamage, Firer, false);
			}
			else if (By.IsValid())
			{
				bBurst = Victim->TakeBlast(CheeseStrikeWeapons::SuperDamage, By.Get(), Stats->DisplayName.ToString());
			}
			if (bBurst)
			{
				Gun->ConfirmHit(CheeseStrikeWeapons::SuperDamage, false, !Victim->IsAlive());
				MulticastSuper(At);
				UE_LOG(LogCheeseStrike, Display, TEXT("Projectile: %s's shards burst in %s (health %.0f)"), Firer != nullptr ? *Firer->DisplayName() : TEXT("someone gone"), *Victim->DisplayName(), Victim->GetHealth());
			}
		}
	}
	if (Stats->SplashRadius > 0.0f)
	{
		End(At, true);
		return;
	}
	// A shard stays stuck in them a moment, for everyone to see.
	bDone = true;
	Flight = FVector::ZeroVector;
	RootComponent->ComponentVelocity = FVector::ZeroVector;
	AttachToActor(Victim, FAttachmentTransformRules::KeepWorldTransform);
	SetLifeSpan(ProjectileStuckInBody);
}

void ACheeseStrikeProjectile::End(const FVector& At, bool bBurst)
{
	bDone = true;
	Flight = FVector::ZeroVector;
	RootComponent->ComponentVelocity = FVector::ZeroVector;
	if (bBurst && Stats->SplashRadius > 0.0f)
	{
		const int32 Harmed = Splash(GetWorld(), *Stats, At, Shooter.Get(), By.Get(), Struck.Get(), Weapon.Get());
		UE_LOG(LogCheeseStrike, Display, TEXT("Projectile: a %s bursts at (%.0f, %.0f, %.0f), harming %d"), *Stats->DisplayName.ToString(), At.X, At.Y, At.Z, Harmed);
		MulticastBurst(At);
		SetLifeSpan(ProjectileLinger);
		return;
	}
	SetLifeSpan(ProjectileStuckInWall);
}

int32 ACheeseStrikeProjectile::Splash(UWorld* World, const FWeaponStats& Stats, const FVector& At, ACheeseStrikeCharacter* Shooter, ACheeseStrikePlayerState* By, const ACheeseStrikeCharacter* Spared, ACheeseStrikeWeapon* Weapon)
{
	if (World == nullptr)
	{
		return 0;
	}
	TArray<ACheeseStrikeCharacter*> Near;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Near.Add(*It);
	}
	ACheeseStrikeCharacter* Firer = Shooter != nullptr && Shooter->IsAlive() ? Shooter : nullptr;
	int32 Harmed = 0;
	for (ACheeseStrikeCharacter* Victim : Near)
	{
		float Distance = 0.0f;
		if (Victim == Spared || !ACheeseStrikeGrenade::BurstSees(World, At, Victim, Distance))
		{
			continue;
		}
		const float Damage = CheeseStrikeWeapons::SplashAt(Stats, Distance);
		if (Damage <= 0.0f)
		{
			continue;
		}
		// Its shooter it hurts as it would anyone; a death by it is their own doing.
		const bool bOwn = Victim == Shooter || (By != nullptr && Victim->GetPlayerState() == By);
		bool bHurt = false;
		if (bOwn)
		{
			bHurt = Victim->TakeOwnBlast(Damage, CheeseStrikeGrenades::OwnWords);
		}
		else if (Firer != nullptr)
		{
			Victim->NoteHit(Stats.Id, Distance, false);
			bHurt = Victim->TakeShot(Damage, Firer, false);
			if (bHurt && Weapon != nullptr)
			{
				Weapon->ConfirmHit(Damage, false, !Victim->IsAlive());
			}
		}
		else if (By != nullptr)
		{
			bHurt = Victim->TakeBlast(Damage, By, Stats.DisplayName.ToString());
		}
		if (bHurt)
		{
			++Harmed;
			UE_LOG(LogCheeseStrike, Display, TEXT("Projectile: the %s's burst hit %s at %.0f cm for %.0f (health %.0f)"), *Stats.DisplayName.ToString(), *Victim->DisplayName(), Distance, Damage, Victim->GetHealth());
		}
	}
	return Harmed;
}

void ACheeseStrikeProjectile::MulticastBurst_Implementation(FVector_NetQuantize At)
{
	bDone = true;
	Round->SetVisibility(false);
	Glow->SetVisibility(false);
	SetActorLocation(At);
	ShowBurst(At);
}

void ACheeseStrikeProjectile::MulticastSuper_Implementation(FVector_NetQuantize At)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// A wet bang, higher than a grenade's, and the cheese bursting out of them.
	if (Bangs.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Bangs[FMath::RandHelper(Bangs.Num())], At, FRotator::ZeroRotator, 0.7f, FMath::FRandRange(1.35f, 1.5f), 0.0f, Attenuation);
	}
	UCheeseStrikeLastStand::ShowCheeseBurst(GetWorld(), At);
}

void ACheeseStrikeProjectile::ShowBurst(const FVector& At)
{
	if (GetNetMode() == NM_DedicatedServer || BurstShownAt >= 0.0f || Stats == nullptr)
	{
		return;
	}
	BurstShownAt = GetWorld()->GetTimeSeconds();
	if (Bangs.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Bangs[FMath::RandHelper(Bangs.Num())], At, FRotator::ZeroRotator, 1.0f, FMath::FRandRange(0.85f, 0.95f), 0.0f, Attenuation);
	}
	if (Rumble != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rumble, At, FRotator::ZeroRotator, 0.8f, FMath::FRandRange(0.85f, 0.95f), 0.0f, Attenuation);
	}
	FlashLight = NewObject<UPointLightComponent>(this);
	FlashLight->SetMobility(EComponentMobility::Movable);
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(ProjectileFlashBrightness);
	FlashLight->SetAttenuationRadius(ProjectileFlashReach);
	FlashLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.22f));
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
			// The template's explosion, as the grenade's: an unlit shell that breaks up as its Param goes from 0 to 1.
			UMaterialInstanceDynamic* Shell = UMaterialInstanceDynamic::Create(FireballMaterial, this);
			Shell->SetScalarParameterValue(ProjectileShellParameter, 0.0f);
			Fireball->SetMaterial(0, Shell);
		}
		Fireball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fireball->SetCastShadow(false);
		Fireball->SetupAttachment(RootComponent);
		Fireball->RegisterComponent();
		Fireball->SetWorldLocation(At);
		Fireball->SetWorldRotation(FRotator::ZeroRotator);
		Fireball->SetWorldScale3D(FVector(0.4f));
	}
	if (UMaterialInterface* Stain = LoadObject<UMaterialInterface>(nullptr, ProjectileScorchPath))
	{
		// The grenade's scorch, as big as the burst's inner reach.
		UMaterialInstanceDynamic* Scorch = UMaterialInstanceDynamic::Create(Stain, this);
		Scorch->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.05f, 0.04f, 0.03f));
		Scorch->SetScalarParameterValue(TEXT("Desaturate"), 1.0f);
		Scorch->SetScalarParameterValue(TEXT("Brightness"), 0.35f);
		const float Size = FMath::Max(Stats->SplashInner * 0.7f, 120.0f);
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, Scorch, FVector(80.0f, Size, Size), At, FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), ProjectileScorchLife))
		{
			Decal->SetFadeOut(ProjectileScorchLife - 2.0f, 2.0f, false);
		}
	}
}
