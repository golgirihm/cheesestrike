// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeapon.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeCharacter.h"

namespace
{
	/** How far a reported shot may start from where the server has the shooter's eyes, allowing for the time their position took to arrive. */
	constexpr float OriginTolerance = 250.0f;
	/** Shots that may arrive together after a delay on the network, over the rate. */
	constexpr float RateBurst = 3.0f;
	/** The socket on each hand the weapon sits in. */
	const FName HandSocket(TEXT("HandGrip_R"));
	const FName MuzzleSocket(TEXT("Muzzle"));

	float ServerTime(const UWorld* World)
	{
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
	}
}

ACheeseStrikeWeapon::ACheeseStrikeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Stats = &CheeseStrikeWeapons::Blaster();

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rifle(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> Shoot(TEXT("/Game/Variant_Shooter/Anims/FP_Rifle_Shoot_Montage.FP_Rifle_Shoot_Montage"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> ArmsAnimation(TEXT("/Game/Variant_Shooter/Anims/ABP_FP_Weapon"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimation(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle"));
	FiringMontage = Shoot.Object;
	FirstPersonAnimation = ArmsAnimation.Class;
	ThirdPersonAnimation = BodyAnimation.Class;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);
	FirstPersonMesh->SetSkeletalMeshAsset(Rifle.Object);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);
	ThirdPersonMesh->SetSkeletalMeshAsset(Rifle.Object);
	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;

	Magazine = Stats->MagazineSize;
	Reserve = Stats->Reserve;
}

void ACheeseStrikeWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, Magazine, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, Reserve, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, ReloadEndsAt, COND_OwnerOnly);
}

void ACheeseStrikeWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(GetOwner()))
	{
		Hold(Character);
	}
}

void ACheeseStrikeWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A late owner: the weapon can arrive on a client before the character it belongs to.
	if (Holder == nullptr)
	{
		if (ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(GetOwner()))
		{
			Hold(Character);
		}
	}
}

void ACheeseStrikeWeapon::Hold(ACheeseStrikeCharacter* Character)
{
	if (Character == nullptr || Holder == Character)
	{
		return;
	}
	Holder = Character;
	const FAttachmentTransformRules Snap(EAttachmentRule::SnapToTarget, false);
	AttachToActor(Character, Snap);
	FirstPersonMesh->AttachToComponent(Character->GetFirstPersonMesh(), Snap, HandSocket);
	ThirdPersonMesh->AttachToComponent(Character->GetMesh(), Snap, HandSocket);
	Character->GetFirstPersonMesh()->SetAnimInstanceClass(FirstPersonAnimation);
	Character->GetMesh()->SetAnimInstanceClass(ThirdPersonAnimation);
	Character->SetWeapon(this);
}

bool ACheeseStrikeWeapon::IsReloading() const
{
	return ReloadEndsAt > 0.0f && ServerTime(GetWorld()) < ReloadEndsAt;
}

int32 ACheeseStrikeWeapon::NextShotIndex() const
{
	return CheeseStrikeWeapons::NextInRun(*Stats, LastShotIndex, ServerTime(GetWorld()) - LastShotAt);
}

float ACheeseStrikeWeapon::CurrentSpread() const
{
	return CheeseStrikeWeapons::SpreadFor(*Stats, NextShotIndex(), bAimed);
}

float ACheeseStrikeWeapon::SinceHit() const
{
	return ServerTime(GetWorld()) - LastHitAt;
}

void ACheeseStrikeWeapon::StartFiring()
{
	if (bTriggerHeld)
	{
		return;
	}
	bTriggerHeld = true;
	FireOnce();
	if (Stats->bAutomatic)
	{
		GetWorldTimerManager().SetTimer(FireTimer, this, &ACheeseStrikeWeapon::FireOnce, Stats->FireInterval, true);
	}
}

void ACheeseStrikeWeapon::StopFiring()
{
	bTriggerHeld = false;
	GetWorldTimerManager().ClearTimer(FireTimer);
}

void ACheeseStrikeWeapon::StartAiming()
{
	bAimed = true;
}

void ACheeseStrikeWeapon::StopAiming()
{
	bAimed = false;
}

void ACheeseStrikeWeapon::FireOnce()
{
	if (Holder == nullptr || !Holder->IsLocallyControlled() || !Holder->IsAlive())
	{
		return;
	}
	const float Now = ServerTime(GetWorld());
	if (IsReloading() || Now - LastShotAt < Stats->FireInterval * 0.9f)
	{
		return;
	}
	if (Magazine <= 0)
	{
		StartReload();
		return;
	}
	const int32 Index = NextShotIndex();
	const int32 Seed = FMath::Rand();
	const UCameraComponent* Camera = Holder->GetFirstPersonCameraComponent();
	const FVector Origin = Camera->GetComponentLocation();
	const FVector Aim = Camera->GetForwardVector();
	// The view climbs by the shot's kick, so the next lands where the crosshair is.
	const FVector2D Kick = CheeseStrikeWeapons::RecoilFor(*Stats, Index, bAimed) - CheeseStrikeWeapons::RecoilFor(*Stats, Index - 1, bAimed);
	Holder->DoAim(Kick.X, Kick.Y);
	if (!HasAuthority())
	{
		--Magazine;
	}
	LastShotAt = Now;
	LastShotIndex = Index;
	++ShotsFired;
	ShowShot();
	ServerFire(Origin, Aim, Seed, Index, bAimed, Now);
}

void ACheeseStrikeWeapon::ShowShot()
{
	if (Holder == nullptr || FiringMontage == nullptr)
	{
		return;
	}
	if (UAnimInstance* Arms = Holder->GetFirstPersonMesh()->GetAnimInstance())
	{
		Arms->Montage_Play(FiringMontage);
	}
}

bool ACheeseStrikeWeapon::ServerAllowsShot(float Now)
{
	ServerShotTimes.RemoveAll([Now](float Time) { return Time < Now - 1.0f; });
	const float Allowed = 1.0f / Stats->FireInterval + RateBurst;
	if (ServerShotTimes.Num() >= Allowed || Magazine <= 0 || IsReloading())
	{
		return false;
	}
	ServerShotTimes.Add(Now);
	return true;
}

void ACheeseStrikeWeapon::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, int32 Seed, int32 Index, bool bWasAimed, float ClientTime)
{
	UWorld* World = GetWorld();
	if (Holder == nullptr || World == nullptr || !Holder->IsAlive())
	{
		return;
	}
	const float Now = ServerTime(World);
	if (!ServerAllowsShot(Now))
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Shot from %s refused: rate or magazine"), *Holder->GetName());
		return;
	}
	// When the shooter fired, as near as can be told: never ahead of now, never further back than is kept.
	const float When = FMath::Clamp(ClientTime, Now - FHitHistory::Kept, Now);
	// The shot must have come from about where the shooter's eyes were.
	const FHitHistory::FSample ShooterThen = Holder->GetHitHistory().At(When);
	const FVector EyesThen = Holder->GetHitHistory().Num() > 0 ? ShooterThen.Head : Holder->GetFirstPersonCameraComponent()->GetComponentLocation();
	if (FVector::Dist(Origin, EyesThen) > OriginTolerance)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Shot from %s refused: from %.0f cm away from them"), *Holder->GetName(), FVector::Dist(Origin, EyesThen));
		return;
	}
	--Magazine;
	// Which way it went: the shooter's aim, spread as the shooter's seed had it.
	const int32 RunIndex = FMath::Max(0, Index);
	FRandomStream Random(Seed);
	const FVector Direction = CheeseStrikeWeapons::Spread(FVector(AimDirection).GetSafeNormal(), CheeseStrikeWeapons::SpreadFor(*Stats, RunIndex, bWasAimed), Random);

	// What the arena stops it at, with every player left out: they are tested where they were.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeShot), true, Holder);
	TArray<ACheeseStrikeCharacter*> Players;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Players.Add(*It);
		Params.AddIgnoredActor(*It);
	}
	FHitResult Wall;
	const FVector End = Origin + Direction * Stats->MaxRange;
	const bool bWallHit = World->LineTraceSingleByChannel(Wall, Origin, End, ECC_Visibility, Params);
	float Reach = bWallHit ? Wall.Distance : Stats->MaxRange;

	// The nearest player along it, where they were when the shooter fired.
	ACheeseStrikeCharacter* Victim = nullptr;
	bool bHead = false;
	for (ACheeseStrikeCharacter* Player : Players)
	{
		if (Player == Holder || !Player->IsAlive())
		{
			continue;
		}
		const FHitHistory::FSample Then = Player->GetHitHistory().Num() > 0 ? Player->GetHitHistory().At(When) : Player->SampleNow(When);
		const float HeadHit = CheeseStrikeWeapons::RayHitsSphere(Origin, Direction, Then.Head, CheeseStrikeWeapons::HeadRadius);
		const float BodyHit = CheeseStrikeWeapons::RayHitsCapsule(Origin, Direction, Then.Feet, Then.Radius, Then.Height);
		float Nearest = -1.0f;
		bool bThisHead = false;
		if (HeadHit >= 0.0f && (BodyHit < 0.0f || HeadHit <= BodyHit + CheeseStrikeWeapons::HeadRadius))
		{
			Nearest = HeadHit;
			bThisHead = true;
		}
		else if (BodyHit >= 0.0f)
		{
			Nearest = BodyHit;
		}
		if (Nearest >= 0.0f && Nearest < Reach)
		{
			Reach = Nearest;
			Victim = Player;
			bHead = bThisHead;
		}
	}

	if (Victim != nullptr)
	{
		const float Damage = CheeseStrikeWeapons::DamageAt(*Stats, Reach, bHead);
		Victim->TakeShot(Damage, Holder, bHead);
		ClientHit(Damage, bHead);
		UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s%s at %.0f cm for %.0f (health %.0f)"), *Holder->GetName(), *Victim->GetName(), bHead ? TEXT(" on the head") : TEXT(""), Reach, Damage, Victim->GetHealth());
	}
	else if (bWallHit && Wall.GetActor() != nullptr)
	{
		// Something of the arena's, or the template's, which takes the engine's damage.
		const float Damage = CheeseStrikeWeapons::DamageAt(*Stats, Reach, false);
		UGameplayStatics::ApplyPointDamage(Wall.GetActor(), Damage, Direction, Wall, Holder->GetController(), this, nullptr);
		if (Wall.GetActor()->IsA<APawn>())
		{
			ClientHit(Damage, false);
		}
		UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s at %.0f cm"), *Holder->GetName(), *Wall.GetActor()->GetName(), Reach);
	}
	else
	{
		UE_LOG(LogCheeseStrike, Verbose, TEXT("Shot: %s hit nothing"), *Holder->GetName());
	}
}

void ACheeseStrikeWeapon::ClientHit_Implementation(float Damage, bool bHead)
{
	++HitsConfirmed;
	LastHitDamage = Damage;
	bLastHitHead = bHead;
	LastHitAt = ServerTime(GetWorld());
}

void ACheeseStrikeWeapon::StartReload()
{
	if (Holder == nullptr || !Holder->IsLocallyControlled() || IsReloading() || Magazine >= Stats->MagazineSize || Reserve <= 0)
	{
		return;
	}
	StopFiring();
	ServerReload();
}

void ACheeseStrikeWeapon::ServerReload_Implementation()
{
	if (IsReloading() || Magazine >= Stats->MagazineSize || Reserve <= 0)
	{
		return;
	}
	ReloadEndsAt = ServerTime(GetWorld()) + Stats->ReloadTime;
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ACheeseStrikeWeapon::FinishReload, Stats->ReloadTime, false);
}

void ACheeseStrikeWeapon::FinishReload()
{
	const int32 Wanted = Stats->MagazineSize - Magazine;
	const int32 Taken = FMath::Min(Wanted, Reserve);
	Magazine += Taken;
	Reserve -= Taken;
	ReloadEndsAt = 0.0f;
}
