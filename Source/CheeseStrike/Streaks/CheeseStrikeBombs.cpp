// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeBombs.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameState.h"
#include "CheeseStrikePlayerState.h"
#include "Streaks/CheeseStrikeOverhead.h"
#include "Streaks/CheeseStrikeSynth.h"

namespace
{
	/** A gouda wheel's size, as the engine's 1 m cylinder is scaled: 1.3 m across, 45 cm thick. */
	const FVector BombWheelScale(1.3f, 1.3f, 0.45f);
	/** How long a burst's flash and shockwave last (s); the shockwave swells to where the harm stops. */
	constexpr float BombFlashTime = 0.45f;
	/** How bright a burst's flash is at first (candelas), and how far it reaches (cm). */
	constexpr float BombFlashBrightness = 1500.0f;
	constexpr float BombFlashReach = 1500.0f;
	/** How long a burst's scorch stays on the ground, and how big it is (cm). */
	constexpr float BombScorchLife = 12.0f;
	constexpr float BombScorchSize = 220.0f;
	const TCHAR* BombScorchPath = TEXT("/Game/CheeseStrike/Materials/MI_Decal_OilStain.MI_Decal_OilStain");
	/** A burst shown more than this late (a machine that heard of it late) is skipped. */
	constexpr float BombTooLate = 0.6f;
	/** The cheese material's switch for its carved holes: a wheel in flight has none, which would swim over it (tools/art/cheese_material.py). */
	const FName BombHolesParameter(TEXT("Holes"));
	/** The template explosion material's one parameter, which breaks its shell up from 0 to 1. */
	const FName BombShellParameter(TEXT("Param"));
	const TCHAR* BombCheesePath = TEXT("/Game/CheeseStrike/Materials/M_Cheese.M_Cheese");
	/** How far above the mark's height a strike's wheel looks for a roof and down for the floor (cm): from about a player's middle. */
	constexpr float BombLookFrom = 100.0f;
	constexpr float BombFloorBelow = 1500.0f;
	/** A burst through a roof: smaller and dimmer than a landing's, and its scorch. */
	constexpr float BombRoofReach = 160.0f;
	constexpr float BombRoofBrightness = 40.0f;
	/** A landing's flash under a roof (candelas). */
	constexpr float BombIndoorBrightness = 120.0f;
	constexpr float BombRoofScorch = 150.0f;
}

float FCheeseStrikeDrop::FallHeight() const
{
	return Roof > 0.0f ? FMath::Max(0.0f, Roof - CheeseStrikeStreaks::RoofGap) : CheeseStrikeStreaks::DropHeight;
}

float FCheeseStrikeDrop::FallSpeed() const
{
	return Roof > 0.0f ? CheeseStrikeStreaks::IndoorDropSpeed : CheeseStrikeStreaks::DropSpeed;
}

float FCheeseStrikeDrop::FallTime() const
{
	return CheeseStrikeStreaks::FallTime(FallHeight(), FallSpeed());
}

ACheeseStrikeBombs::ACheeseStrikeBombs()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(20.0f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fireball(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Projectiles/Materials/M_Explosion.M_Explosion"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch1(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_01.Explosions_Grenade_Punch-Close_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch2(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_02.Explosions_Grenade_Punch-Close_02"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Punch3(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_03.Explosions_Grenade_Punch-Close_03"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Tail(TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_01.Explosions_Grenade_Noise-Exterior_01"));
	WheelMesh = Cylinder.Object;
	BallMesh = Sphere.Object;
	BallMaterial = Fireball.Object;
	for (USoundBase* Sound : {Punch1.Object, Punch2.Object, Punch3.Object})
	{
		if (Sound != nullptr)
		{
			Thumps.Add(Sound);
		}
	}
	Rumble = Tail.Object;
}

void ACheeseStrikeBombs::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeBombs, Kind);
	DOREPLIFETIME(ACheeseStrikeBombs, Drops);
	DOREPLIFETIME(ACheeseStrikeBombs, WarnAt);
	DOREPLIFETIME(ACheeseStrikeBombs, WarnWhere);
	DOREPLIFETIME(ACheeseStrikeBombs, RainFrom);
	DOREPLIFETIME(ACheeseStrikeBombs, RainUntil);
}

void ACheeseStrikeBombs::BeginPlay()
{
	Super::BeginPlay();
	// The cheese material, loaded here rather than held by the class, as a script rebuilds it.
	if (UMaterialInterface* Cheese = LoadObject<UMaterialInterface>(nullptr, BombCheesePath))
	{
		WheelMaterial = UMaterialInstanceDynamic::Create(Cheese, this);
		WheelMaterial->SetScalarParameterValue(BombHolesParameter, 0.0f);
	}
	// The Creamery's oil stain, darkened, as the scorch a burst leaves; loaded here, as a script makes it.
	if (UMaterialInterface* Stain = LoadObject<UMaterialInterface>(nullptr, BombScorchPath))
	{
		UMaterialInstanceDynamic* Scorch = UMaterialInstanceDynamic::Create(Stain, this);
		Scorch->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.05f, 0.04f, 0.03f));
		Scorch->SetScalarParameterValue(TEXT("Desaturate"), 1.0f);
		Scorch->SetScalarParameterValue(TEXT("Brightness"), 0.35f);
		ScorchMaterial = Scorch;
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(600.0f);
	Attenuation->Attenuation.FalloffDistance = 9000.0f;
}

float ACheeseStrikeBombs::Now() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

bool ACheeseStrikeBombs::Landing(UWorld* World, const FVector& Over, FVector& OutImpact)
{
	if (World == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeLanding), false);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	const FVector From = Over + FVector(0.0f, 0.0f, 6000.0f);
	if (!World->LineTraceSingleByChannel(Hit, From, Over - FVector(0.0f, 0.0f, 6000.0f), ECC_Visibility, Params))
	{
		return false;
	}
	OutImpact = Hit.ImpactPoint;
	return true;
}

bool ACheeseStrikeBombs::GroundNear(UWorld* World, const FVector2D& Where, float Height, FVector& OutGround)
{
	if (World == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeGroundNear), false);
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	// Down from a head's height over the feet: the floor they are on, or the one under them there; where that begins
	// inside something (a wall, a stack), or meets nothing, the first thing under the sky.
	const FVector From(Where.X, Where.Y, Height + 250.0f);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, From, FVector(Where.X, Where.Y, Height - CheeseStrikeStreaks::DropHeight), ECC_Visibility, Params) && !Hit.bStartPenetrating && Hit.Distance > 1.0f)
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return Landing(World, FVector(Where.X, Where.Y, Height), OutGround);
}

bool ACheeseStrikeBombs::StrikeLanding(UWorld* World, const FCheeseStrikeOverhead& Overhead, const FVector& Over, FVector& OutImpact, float& OutRoof)
{
	OutRoof = 0.0f;
	if (World == nullptr)
	{
		return false;
	}
	const FVector From = Over + FVector(0.0f, 0.0f, BombLookFrom);
	if (const TOptional<float> Roof = Overhead.RoofOver(From, CheeseStrikeStreaks::DropHeight))
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeFloor), false);
		for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, From - FVector(0.0f, 0.0f, BombFloorBelow), ECC_Visibility, Params) && Roof.GetValue() > Hit.ImpactPoint.Z)
		{
			OutImpact = Hit.ImpactPoint;
			OutRoof = Roof.GetValue() - Hit.ImpactPoint.Z;
			return true;
		}
	}
	return Landing(World, Over, OutImpact);
}

void ACheeseStrikeBombs::AddDrop(const FVector& Over, float LandsAt)
{
	FVector Impact;
	if (!Landing(GetWorld(), Over, Impact))
	{
		return;
	}
	FCheeseStrikeDrop Drop;
	Drop.Impact = Impact;
	Drop.LandsAt = LandsAt;
	Drops.Add(Drop);
	DoneAt = FMath::Max(DoneAt, LandsAt + 2.0f);
}

void ACheeseStrikeBombs::StartStrike(ACheeseStrikePlayerState* InCaller, const FVector& Spot, float Yaw)
{
	Caller = InCaller;
	Kind = static_cast<uint8>(CheeseStrikeStreaks::EStreak::CheeseStrike);
	const float Start = Now();
	WarnAt = Start;
	WarnWhere = Spot;
	const TArray<FVector> Line = CheeseStrikeStreaks::StrikeArea(Spot, Yaw);
	// Indoors, through the roof onto the floor under it, rather than onto the roof.
	const FCheeseStrikeOverhead& Overhead = FCheeseStrikeOverhead::Of(GetWorld());
	int32 ThroughRoofs = 0;
	for (int32 Index = 0; Index < Line.Num(); ++Index)
	{
		FCheeseStrikeDrop Drop;
		FVector Impact;
		if (!StrikeLanding(GetWorld(), Overhead, Line[Index], Impact, Drop.Roof))
		{
			continue;
		}
		Drop.Impact = Impact;
		Drop.LandsAt = Start + CheeseStrikeStreaks::StrikeWarning + Index * CheeseStrikeStreaks::StrikeInterval;
		Drops.Add(Drop);
		ThroughRoofs += Drop.Roof > 0.0f ? 1 : 0;
		DoneAt = FMath::Max(DoneAt, Drop.LandsAt + 2.0f);
	}
	DoneAt = FMath::Max(DoneAt, Start + CheeseStrikeStreaks::StrikeWarning + 2.0f);
	UE_LOG(LogCheeseStrike, Display, TEXT("Cheese Strike from %s: %d wheels over %.0f cm round (%.0f, %.0f, %.0f), %d through a roof"), InCaller != nullptr ? *InCaller->GetPlayerName() : TEXT("nobody"), Drops.Num(), CheeseStrikeStreaks::StrikeRadius, Spot.X, Spot.Y, Spot.Z, ThroughRoofs);
}

void ACheeseStrikeBombs::StartOverload(ACheeseStrikePlayerState* InCaller)
{
	Caller = InCaller;
	Kind = static_cast<uint8>(CheeseStrikeStreaks::EStreak::GoudaOverload);
	const float Start = Now();
	WarnAt = Start;
	// The first wheels land as the warning ends: they are let go a fall's time before.
	RainFrom = Start + CheeseStrikeStreaks::OverloadWarning - CheeseStrikeStreaks::FallTime();
	RainUntil = RainFrom + CheeseStrikeStreaks::OverloadRain;
	NextRainAt = RainFrom;
	DoneAt = RainUntil + CheeseStrikeStreaks::FallTime() + 2.0f;
	UE_LOG(LogCheeseStrike, Display, TEXT("Gouda Overload from %s: rain from %.1f s to %.1f s"), InCaller != nullptr ? *InCaller->GetPlayerName() : TEXT("nobody"), RainFrom - Start, RainUntil - Start);
}

void ACheeseStrikeBombs::Burst(const FCheeseStrikeDrop& Drop)
{
	++Landed;
	ACheeseStrikePlayerState* By = Caller.Get();
	UWorld* World = GetWorld();
	if (By == nullptr || World == nullptr)
	{
		return;
	}
	const CheeseStrikeStreaks::FBlast& Blast = GetKind() == CheeseStrikeStreaks::EStreak::GoudaOverload ? CheeseStrikeStreaks::OverloadBlast : CheeseStrikeStreaks::StrikeBlast;
	const FString How = CheeseStrikeStreaks::Info(GetKind()).Name;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeBurst), false);
	TArray<ACheeseStrikeCharacter*> Near;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
		if (It->IsAlive())
		{
			Near.Add(*It);
		}
	}
	// The burst is a little above where it landed, so that the ground itself hides nobody.
	const FVector From = FVector(Drop.Impact) + FVector(0.0f, 0.0f, 40.0f);
	for (ACheeseStrikeCharacter* Victim : Near)
	{
		const FVector Head = Victim->GetEyes()->GetComponentLocation();
		const FVector Chest = Victim->ChestAt();
		const FVector Feet = Victim->IsDowned() ? Victim->GetDownShape().Feet : Victim->GetActorLocation() - FVector(0.0f, 0.0f, 70.0f);
		const float Distance = FMath::Min3(FVector::Dist(From, Head), FVector::Dist(From, Chest), FVector::Dist(From, Feet));
		const float Damage = CheeseStrikeStreaks::BlastDamage(Blast, Distance);
		if (Damage <= 0.0f)
		{
			continue;
		}
		// Splash is checked for line of sight: a wall or a roof between them saves them.
		bool bSeen = false;
		FHitResult Hit;
		for (const FVector& To : {Chest, Head, Feet})
		{
			bSeen |= !World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
		}
		if (bSeen && Victim->TakeBlast(Damage, By, How))
		{
			++Hits;
			UE_LOG(LogCheeseStrike, Display, TEXT("%s: %s hit %s at %.0f cm for %.0f (health %.0f)"), *How, *By->GetPlayerName(), *Victim->DisplayName(), Distance, Damage, Victim->GetHealth());
		}
	}
}

void ACheeseStrikeBombs::ShowWarning()
{
	if (bWarned || WarnAt <= 0.0f || Now() < WarnAt || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bWarned = true;
	// A machine that hears of it well after it began (one just joined) is spared a warning out of time.
	if (Now() - WarnAt > 1.0f)
	{
		return;
	}
	if (GetKind() == CheeseStrikeStreaks::EStreak::GoudaOverload)
	{
		// The siren, for everyone, wherever they are.
		UGameplayStatics::SpawnSound2D(this, CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Siren(CheeseStrikeStreaks::OverloadWarning + 1.0f)), 0.8f);
	}
	else
	{
		// The whistle coming down on the mark.
		UGameplayStatics::SpawnSoundAtLocation(this, CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Whistle(CheeseStrikeStreaks::StrikeWarning)), FVector(WarnWhere) + FVector(0.0f, 0.0f, 1500.0f), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, Attenuation);
	}
}

void ACheeseStrikeBombs::ShowBurst(const FVector& At, bool bIndoors)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Thumps.Num() > 0)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Thumps[FMath::RandHelper(Thumps.Num())], At, FRotator::ZeroRotator, 1.0f, FMath::FRandRange(0.85f, 1.0f), 0.0f, Attenuation);
	}
	if (Rumble != nullptr)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Rumble, At, FRotator::ZeroRotator, 0.7f, FMath::FRandRange(0.8f, 0.95f), 0.0f, Attenuation);
	}
	FFlash Flash;
	Flash.At = Now();
	Flash.Reach = (GetKind() == CheeseStrikeStreaks::EStreak::GoudaOverload ? CheeseStrikeStreaks::OverloadBlast : CheeseStrikeStreaks::StrikeBlast).Radius;
	Flash.Brightness = bIndoors ? BombIndoorBrightness : BombFlashBrightness;
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Flash.Brightness);
	Light->SetAttenuationRadius(BombFlashReach);
	Light->SetLightColor(FLinearColor(1.0f, 0.62f, 0.25f));
	Light->SetCastShadows(false);
	Light->SetupAttachment(RootComponent);
	Light->RegisterComponent();
	Light->SetWorldLocation(At + FVector(0.0f, 0.0f, 150.0f));
	Flash.Light = Light;
	if (BallMesh != nullptr)
	{
		UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(this);
		Ball->SetStaticMesh(BallMesh);
		if (BallMaterial != nullptr)
		{
			// The template's explosion: an unlit shell that breaks up as its Param goes from 0 to 1.
			UMaterialInstanceDynamic* Shell = UMaterialInstanceDynamic::Create(BallMaterial, this);
			Shell->SetScalarParameterValue(BombShellParameter, 0.0f);
			Ball->SetMaterial(0, Shell);
		}
		Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ball->SetCastShadow(false);
		Ball->SetupAttachment(RootComponent);
		Ball->RegisterComponent();
		Ball->SetWorldLocation(At + FVector(0.0f, 0.0f, 20.0f));
		Ball->SetWorldScale3D(FVector(0.6f));
		Flash.Ball = Ball;
	}
	if (ScorchMaterial != nullptr)
	{
		// Facing down onto the ground, turned any way; its own, so that it outlasts this.
		if (UDecalComponent* Scorch = UGameplayStatics::SpawnDecalAtLocation(this, ScorchMaterial, FVector(80.0f, BombScorchSize, BombScorchSize), At, FRotator(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), BombScorchLife))
		{
			Scorch->SetFadeOut(BombScorchLife - 2.0f, 2.0f, false);
		}
	}
	Flashes.Add(Flash);
}

void ACheeseStrikeBombs::ShowThroughRoof(const FVector& At)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// A crack, a puff of dust and fire hanging from the roof, and the hole's scorch on its underside.
	if (Thumps.Num() > 0)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, Thumps[FMath::RandHelper(Thumps.Num())], At, FRotator::ZeroRotator, 0.5f, FMath::FRandRange(1.2f, 1.4f), 0.0f, Attenuation);
	}
	FFlash Flash;
	Flash.At = Now();
	Flash.Reach = BombRoofReach;
	Flash.Brightness = BombRoofBrightness;
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(BombRoofBrightness);
	Light->SetAttenuationRadius(BombFlashReach * 0.5f);
	Light->SetLightColor(FLinearColor(1.0f, 0.62f, 0.25f));
	Light->SetCastShadows(false);
	Light->SetupAttachment(RootComponent);
	Light->RegisterComponent();
	Light->SetWorldLocation(At - FVector(0.0f, 0.0f, 80.0f));
	Flash.Light = Light;
	if (BallMesh != nullptr)
	{
		UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(this);
		Ball->SetStaticMesh(BallMesh);
		if (BallMaterial != nullptr)
		{
			UMaterialInstanceDynamic* Shell = UMaterialInstanceDynamic::Create(BallMaterial, this);
			Shell->SetScalarParameterValue(BombShellParameter, 0.0f);
			Ball->SetMaterial(0, Shell);
		}
		Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ball->SetCastShadow(false);
		Ball->SetupAttachment(RootComponent);
		Ball->RegisterComponent();
		Ball->SetWorldLocation(At);
		Ball->SetWorldScale3D(FVector(0.4f));
		Flash.Ball = Ball;
	}
	if (ScorchMaterial != nullptr)
	{
		// Facing up onto the roof's underside.
		if (UDecalComponent* Scorch = UGameplayStatics::SpawnDecalAtLocation(this, ScorchMaterial, FVector(80.0f, BombRoofScorch, BombRoofScorch), At, FRotator(90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), BombScorchLife))
		{
			Scorch->SetFadeOut(BombScorchLife - 2.0f, 2.0f, false);
		}
	}
	Flashes.Add(Flash);
}

void ACheeseStrikeBombs::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Time = Now();
	const bool bServer = HasAuthority();

	// The Overload's rain: a wheel let go over every enemy, where they are now, give or take.
	if (bServer && GetKind() == CheeseStrikeStreaks::EStreak::GoudaOverload && Time >= NextRainAt && NextRainAt < RainUntil)
	{
		NextRainAt += CheeseStrikeStreaks::OverloadInterval;
		const ACheeseStrikeGameState* Match = GetWorld()->GetGameState<ACheeseStrikeGameState>();
		const ACheeseStrikePlayerState* By = Caller.Get();
		for (TActorIterator<ACheeseStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (By != nullptr && Match != nullptr && It->IsAlive() && Match->AreEnemies(By, It->GetPlayerState()))
			{
				const FVector Scatter = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetClampedToMaxSize(1.0f) * CheeseStrikeStreaks::OverloadScatter;
				AddDrop(It->GetActorLocation() + Scatter, Time + CheeseStrikeStreaks::FallTime());
			}
		}
	}

	ShowWarning();
	Shown.SetNumZeroed(Drops.Num());
	Through.SetNumZeroed(Drops.Num());
	Falling.SetNum(Drops.Num());
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const FCheeseStrikeDrop& Drop = Drops[Index];
		const float Fall = Drop.FallTime();
		const float Left = Drop.LandsAt - Time;
		if (Shown[Index] >= 3)
		{
			continue;
		}
		if (Left <= 0.0f)
		{
			Shown[Index] = 3;
			if (Falling[Index] != nullptr)
			{
				Falling[Index]->DestroyComponent();
				Falling[Index] = nullptr;
			}
			if (-Left < BombTooLate)
			{
				ShowBurst(Drop.Impact, Drop.Roof > 0.0f);
			}
			if (bServer)
			{
				Burst(Drop);
			}
			continue;
		}
		if (Left > Fall || GetNetMode() == NM_DedicatedServer)
		{
			continue;
		}
		// Falling: each Overload wheel whistles on its way down.
		if (Shown[Index] < 1)
		{
			Shown[Index] = 1;
			if (GetKind() == CheeseStrikeStreaks::EStreak::GoudaOverload)
			{
				UGameplayStatics::SpawnSoundAtLocation(this, CheeseStrikeSynth::MakeSound(this, CheeseStrikeSynth::Whistle(Left)), FVector(Drop.Impact) + FVector(0.0f, 0.0f, 800.0f), FRotator::ZeroRotator, 0.45f, FMath::FRandRange(0.9f, 1.1f), 0.0f, Attenuation);
			}
		}
		if (Falling[Index] == nullptr && WheelMesh != nullptr)
		{
			Shown[Index] = 2;
			UStaticMeshComponent* Wheel = NewObject<UStaticMeshComponent>(this);
			Wheel->SetStaticMesh(WheelMesh);
			if (WheelMaterial != nullptr)
			{
				Wheel->SetMaterial(0, WheelMaterial);
			}
			Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Wheel->SetupAttachment(RootComponent);
			Wheel->RegisterComponent();
			Wheel->SetWorldScale3D(BombWheelScale);
			Falling[Index] = Wheel;
		}
		if (UStaticMeshComponent* Wheel = Falling[Index])
		{
			// Tumbling as it comes, each wheel its own way; through a roof, from the burst under it.
			if (Drop.Roof > 0.0f && !Through[Index])
			{
				Through[Index] = true;
				ShowThroughRoof(FVector(Drop.Impact) + FVector(0.0f, 0.0f, Drop.Roof - 2.0f));
			}
			const float Spin = (Fall - Left) * 420.0f + Index * 47.0f;
			Wheel->SetWorldLocationAndRotation(FVector(Drop.Impact) + FVector(0.0f, 0.0f, CheeseStrikeStreaks::HeightBefore(Left, Drop.FallHeight(), Drop.FallSpeed()) + 40.0f), FRotator(Spin, Index * 61.0f, 70.0f));
		}
	}

	// The flashes fade and the fireballs swell, then go.
	for (int32 Index = Flashes.Num() - 1; Index >= 0; --Index)
	{
		FFlash& Flash = Flashes[Index];
		const float Along = (Time - Flash.At) / BombFlashTime;
		if (Along >= 1.0f)
		{
			if (UPointLightComponent* Light = Flash.Light.Get())
			{
				Light->DestroyComponent();
			}
			if (UStaticMeshComponent* Ball = Flash.Ball.Get())
			{
				Ball->DestroyComponent();
			}
			Flashes.RemoveAt(Index);
			continue;
		}
		if (UPointLightComponent* Light = Flash.Light.Get())
		{
			Light->SetIntensity(Flash.Brightness * FMath::Square(1.0f - Along));
		}
		if (UStaticMeshComponent* Ball = Flash.Ball.Get())
		{
			// A low dome swelling to exactly where the harm stops (the engine's sphere is 1 m across), breaking up as it goes;
			// through a roof, a smaller one under it.
			const float Across = FMath::Lerp(0.4f, Flash.Reach / 50.0f, FMath::Sqrt(Along));
			Ball->SetWorldScale3D(FVector(Across, Across, Across * 0.55f));
			if (UMaterialInstanceDynamic* Shell = Cast<UMaterialInstanceDynamic>(Ball->GetMaterial(0)))
			{
				Shell->SetScalarParameterValue(BombShellParameter, Along);
			}
		}
	}

	if (bServer && DoneAt > 0.0f && Time > DoneAt)
	{
		Destroy();
	}
}
