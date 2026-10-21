// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeapon.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include "Animation/CheeseStrikeAnimInstance.h"
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
#include "Map/CheeseStrikeMouse.h"

namespace
{
	/** How far a reported shot may start from where the server has the shooter's eyes, allowing for the time their position took to arrive. */
	constexpr float OriginTolerance = 250.0f;
	/** Shots that may arrive together after a delay on the network, over the rate. */
	constexpr float RateBurst = 3.0f;
	/** The bone on the mannequin's right hand the weapon sits in, and how the rifle turns in it to point the way the hand does. */
	const FName HandSocket(TEXT("weapon_r"));
	const FRotator GripRotation(0.0f, -90.0f, 0.0f);
	/**
	 * For a body without weapon_r (Belica): how the rifle sits on hand_r.
	 * The turn was measured in the game, as the first-person rifle's turn in
	 * her hand_r's space while both bodies play the same animation (the
	 * trace's "Grip wanted" line); the place is the mannequin's weapon_r
	 * offset from its hand, a couple of centimetres.
	 */
	const FTransform HandGrip(FRotator(-0.6f, 87.3f, 7.7f), FVector(-1.2f, 1.3f, -0.5f));
	/** The bones Belica's own guns are skinned to. */
	const TCHAR* const BuiltInWeaponBones[] = {TEXT("weapon"), TEXT("weapon_l"), TEXT("pistol")};

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

	// Lyra's rifle, its animations and its sounds (Content/LICENSES.md).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rifle(TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Fire(TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Fire.Weap_Rifle_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Reload(TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Reload.Weap_Rifle_Reload"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot1(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_01.Weapons_Rifle_Punch-Close_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot2(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_02.Weapons_Rifle_Punch-Close_02"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot3(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_03.Weapons_Rifle_Punch-Close_03"));
	static ConstructorHelpers::FObjectFinder<USoundBase> DryFire(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_DryFire_01.Weapons_Rifle_DryFire_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ClipOut(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipOut_01.Weapons_Rifle_ClipOut_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ClipIn(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipIn_01.Weapons_Rifle_ClipIn_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Bolt(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Bolt_01.Weapons_Rifle_Bolt_01"));
	RifleFire = Fire.Object;
	RifleReload = Reload.Object;
	FireSounds = {Shot1.Object, Shot2.Object, Shot3.Object};
	DryFireSound = DryFire.Object;
	ClipOutSound = ClipOut.Object;
	ClipInSound = ClipIn.Object;
	BoltSound = Bolt.Object;

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
	PutInHand(FirstPersonMesh, Character->GetFirstPersonMesh());
	PutInHand(ThirdPersonMesh, Character->GetMesh());
	Character->SetWeapon(this);
}

void ACheeseStrikeWeapon::PutInHand(USceneComponent* Rifle, USkeletalMeshComponent* Body)
{
	if (Rifle == nullptr || Body == nullptr)
	{
		return;
	}
	const FAttachmentTransformRules Snap(EAttachmentRule::SnapToTarget, false);
	const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
	// The rifle is modelled along its Y axis; weapon_r points its X the way the character faces.
	if (Mesh == nullptr || Mesh->GetRefSkeleton().FindBoneIndex(HandSocket) != INDEX_NONE)
	{
		Rifle->AttachToComponent(Body, Snap, HandSocket);
		Rifle->SetRelativeRotation(GripRotation);
		return;
	}
	Rifle->AttachToComponent(Body, Snap, FName(TEXT("hand_r")));
	Rifle->SetRelativeTransform(HandGrip);
}

void ACheeseStrikeWeapon::HideBuiltInWeapons(USkeletalMeshComponent* Body)
{
	const USkeletalMesh* Mesh = Body != nullptr ? Body->GetSkeletalMeshAsset() : nullptr;
	if (Mesh == nullptr)
	{
		return;
	}
	for (const TCHAR* Bone : BuiltInWeaponBones)
	{
		if (Mesh->GetRefSkeleton().FindBoneIndex(FName(Bone)) != INDEX_NONE)
		{
			Body->HideBoneByName(FName(Bone), PBO_None);
		}
	}
}

void ACheeseStrikeWeapon::PlayOnBodies(TObjectPtr<UAnimSequence> FCheeseStrikeAnimSet::* Which, TFunction<float(const UAnimSequence*)> RateFor)
{
	if (Holder == nullptr)
	{
		return;
	}
	// Each body plays its own skeleton's copy.
	for (USkeletalMeshComponent* Body : {Holder->GetFirstPersonMesh(), Holder->GetMesh()})
	{
		UCheeseStrikeAnimInstance* Instance = Body != nullptr ? Cast<UCheeseStrikeAnimInstance>(Body->GetAnimInstance()) : nullptr;
		UAnimSequence* Animation = Instance != nullptr ? (Instance->GetSet().*Which).Get() : nullptr;
		if (Animation != nullptr)
		{
			Instance->PlaySlotAnimationAsDynamicMontage(Animation, UCheeseStrikeAnimInstance::SlotName(), 0.1f, 0.15f, RateFor ? RateFor(Animation) : 1.0f);
		}
	}
}

void ACheeseStrikeWeapon::PlaySound(USoundBase* Sound) const
{
	if (Sound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, ThirdPersonMesh->GetComponentLocation());
	}
}

void ACheeseStrikeWeapon::ShowShot()
{
	PlayOnBodies(&FCheeseStrikeAnimSet::Fire);
	for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		Mesh->PlayAnimation(RifleFire, false);
	}
	if (FireSounds.Num() > 0)
	{
		PlaySound(FireSounds[FMath::RandHelper(FireSounds.Num())]);
	}
}

void ACheeseStrikeWeapon::ShowReload()
{
	// The animations take the reload's time, however long they were made.
	const float ReloadTime = Stats->ReloadTime;
	PlayOnBodies(&FCheeseStrikeAnimSet::Reload, [ReloadTime](const UAnimSequence* Reload) { return Reload->GetPlayLength() > 0.0f ? Reload->GetPlayLength() / ReloadTime : 1.0f; });
	const float RifleRate = RifleReload != nullptr && RifleReload->GetPlayLength() > 0.0f ? RifleReload->GetPlayLength() / Stats->ReloadTime : 1.0f;
	for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		Mesh->SetPlayRate(RifleRate);
		Mesh->PlayAnimation(RifleReload, false);
	}
	// The magazine comes out at once, goes in two thirds of the way through, and the bolt follows.
	PlaySound(ClipOutSound);
	GetWorldTimerManager().SetTimer(ClipInTimer, FTimerDelegate::CreateUObject(this, &ACheeseStrikeWeapon::PlaySound, ClipInSound.Get()), Stats->ReloadTime * 0.65f, false);
	GetWorldTimerManager().SetTimer(BoltTimer, FTimerDelegate::CreateUObject(this, &ACheeseStrikeWeapon::PlaySound, BoltSound.Get()), Stats->ReloadTime * 0.85f, false);
}

void ACheeseStrikeWeapon::MulticastShot_Implementation()
{
	// The shooter has shown it already.
	if (Holder != nullptr && !Holder->IsLocallyControlled())
	{
		ShowShot();
	}
}

void ACheeseStrikeWeapon::MulticastReload_Implementation()
{
	if (Holder != nullptr && !Holder->IsLocallyControlled())
	{
		ShowReload();
	}
}

bool ACheeseStrikeWeapon::IsReloading() const
{
	return ReloadEndsAt > 0.0f && ServerTime(GetWorld()) < ReloadEndsAt;
}

FString ACheeseStrikeWeapon::DescribeReload() const
{
	const float Left = ReloadEndsAt > 0.0f ? ReloadEndsAt - ServerTime(GetWorld()) : -1.0f;
	return FString::Printf(TEXT("reload ends in %.1f s, timer %.1f s"), Left, GetWorldTimerManager().GetTimerRemaining(ReloadTimer));
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
	if (Holder == nullptr || !Holder->IsLocallyControlled() || !Holder->CanAct())
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
		PlaySound(DryFireSound);
		StartReload();
		return;
	}
	const int32 Index = NextShotIndex();
	const int32 Seed = FMath::Rand();
	const UCameraComponent* Camera = Holder->GetFirstPersonCameraComponent();
	const FVector Origin = Camera->GetComponentLocation();
	// A player's view is their camera's; a bot's camera takes its control rotation only when someone looks through it, so a bot aims by that.
	const FVector Aim = Holder->IsPlayerControlled() ? Camera->GetForwardVector() : Holder->GetControlRotation().Vector();
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
	if (Holder == nullptr || World == nullptr || !Holder->CanAct())
	{
		return;
	}
	const float Now = ServerTime(World);
	if (!ServerAllowsShot(Now))
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Shot from %s refused: rate or magazine"), *Holder->GetName());
		return;
	}
	// Firing gives up the protection of a fresh spawn.
	Holder->EndProtection();
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
	MulticastShot();
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
		if (Victim->TakeShot(Damage, Holder, bHead))
		{
			ClientHit(Damage, bHead);
			UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s%s at %.0f cm for %.0f (health %.0f)"), *Holder->GetName(), *Victim->GetName(), bHead ? TEXT(" on the head") : TEXT(""), Reach, Damage, Victim->GetHealth());
		}
		else
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s, who is protected"), *Holder->GetName(), *Victim->GetName());
		}
	}
	else if (bWallHit && Wall.GetActor() != nullptr)
	{
		// Something of the arena's, or the template's, which takes the engine's damage.
		const float Damage = CheeseStrikeWeapons::DamageAt(*Stats, Reach, false);
		UGameplayStatics::ApplyPointDamage(Wall.GetActor(), Damage, Direction, Wall, Holder->GetController(), this, nullptr);
		if (Wall.GetActor()->IsA<APawn>() || Wall.GetActor()->IsA<ACheeseStrikeMouse>())
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
	ShowReload();
	ServerReload();
}

void ACheeseStrikeWeapon::AddReserve(int32 Rounds)
{
	if (HasAuthority())
	{
		Reserve = FMath::Clamp(Reserve + Rounds, 0, Stats->ReserveMax);
	}
}

void ACheeseStrikeWeapon::ServerReload_Implementation()
{
	if (IsReloading() || Magazine >= Stats->MagazineSize || Reserve <= 0)
	{
		return;
	}
	ReloadEndsAt = ServerTime(GetWorld()) + Stats->ReloadTime;
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ACheeseStrikeWeapon::FinishReload, Stats->ReloadTime, false);
	MulticastReload();
}

void ACheeseStrikeWeapon::FinishReload()
{
	const int32 Wanted = Stats->MagazineSize - Magazine;
	const int32 Taken = FMath::Min(Wanted, Reserve);
	Magazine += Taken;
	Reserve -= Taken;
	ReloadEndsAt = 0.0f;
}
