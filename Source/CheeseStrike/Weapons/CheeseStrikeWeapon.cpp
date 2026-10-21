// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeapon.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "CheeseStrikeArsenal.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGunfire.h"
#include "CheeseStrikeInventory.h"
#include "Map/CheeseStrikeMouse.h"
#include "Movement/CheeseStrikeMovementComponent.h"

/** With this on, every shot heard here is written to the log with how loud and how dull it is heard. */
static TAutoConsoleVariable<int32> CVarTraceGunfire(TEXT("CheeseStrike.TraceGunfire"), 0, TEXT("Log every shot heard: how far off, at what volume and how dull."));

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

	/** Which model a weapon has. */
	enum class EWeaponModel : uint8
	{
		Rifle,
		Pistol,
		Blade,
	};
	/**
	 * How each weapon looks and sounds until it has a model of its own: the
	 * rifle at a size, its shots higher or lower and softer or louder; the
	 * pistol Lyra's; the knife a plain blade.
	 */
	struct FWeaponLook
	{
		const TCHAR* Id;
		EWeaponModel Model;
		float Scale;
		float Volume;
		float Pitch;
	};
	const FWeaponLook WeaponLooks[] = {
		{TEXT("blaster"), EWeaponModel::Rifle, 1.0f, 1.0f, 1.0f},
		{TEXT("carbine"), EWeaponModel::Rifle, 0.92f, 0.95f, 1.08f},
		{TEXT("grater"), EWeaponModel::Rifle, 0.8f, 0.8f, 1.25f},
		{TEXT("munster"), EWeaponModel::Rifle, 1.15f, 1.1f, 0.85f},
		{TEXT("shotgun"), EWeaponModel::Rifle, 1.05f, 1.2f, 0.65f},
		{TEXT("roquefort"), EWeaponModel::Rifle, 1.2f, 1.2f, 0.6f},
		{TEXT("pistol"), EWeaponModel::Pistol, 1.0f, 0.75f, 1.35f},
		{TEXT("knife"), EWeaponModel::Blade, 1.0f, 0.6f, 1.6f},
	};
	/** The knife's stand-in blade: the engine's metre cube made 22 cm long, 3 cm deep and a few millimetres thin, along Y as the rifle is. */
	const FVector KnifeBladeSize(0.006f, 0.24f, 0.035f);
	const FVector KnifeBladeOffset(0.0f, 9.0f, 2.0f);
}

ACheeseStrikeWeapon::ACheeseStrikeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Stats = &CheeseStrikeWeapons::Blaster();
	WeaponId = Stats->Id;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Lyra's rifle, its animations and its sounds (Content/LICENSES.md).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rifle(TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Pistol(TEXT("/Game/Weapons/Pistol/Meshes/SKM_Pistol.SKM_Pistol"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Block(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Fire(TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Fire.Weap_Rifle_Fire"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Reload(TEXT("/Game/Weapons/Rifle/Animations/Weap_Rifle_Reload.Weap_Rifle_Reload"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot1(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_01.Weapons_Rifle_Punch-Close_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot2(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_02.Weapons_Rifle_Punch-Close_02"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Shot3(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Punch-Close_03.Weapons_Rifle_Punch-Close_03"));
	static ConstructorHelpers::FObjectFinder<USoundBase> DryFire(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_DryFire_01.Weapons_Rifle_DryFire_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ClipOut(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipOut_01.Weapons_Rifle_ClipOut_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> ClipIn(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_ClipIn_01.Weapons_Rifle_ClipIn_01"));
	static ConstructorHelpers::FObjectFinder<USoundBase> Bolt(TEXT("/Game/Audio/Sounds/Weapons/Rifle/Weapons_Rifle_Bolt_01.Weapons_Rifle_Bolt_01"));
	RifleMesh = Rifle.Object;
	PistolMesh = Pistol.Object;
	BlockMesh = Block.Object;
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

	FirstPersonModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("First Person Model"));
	FirstPersonModel->SetupAttachment(RootComponent);
	FirstPersonModel->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonModel->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonModel->bOnlyOwnerSee = true;
	FirstPersonModel->SetHiddenInGame(true);

	ThirdPersonModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Third Person Model"));
	ThirdPersonModel->SetupAttachment(RootComponent);
	ThirdPersonModel->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonModel->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonModel->bOwnerNoSee = true;
	ThirdPersonModel->SetHiddenInGame(true);

	Magazine = Stats->MagazineSize;
	Reserve = Stats->Reserve;
}

void ACheeseStrikeWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, WeaponId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, Magazine, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, Reserve, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACheeseStrikeWeapon, ReloadEndsAt, COND_OwnerOnly);
}

void ACheeseStrikeWeapon::SetWeapon(FName Id)
{
	const FWeaponStats* Found = CheeseStrikeArsenal::Find(Id);
	Stats = Found != nullptr ? Found : &CheeseStrikeWeapons::Blaster();
	WeaponId = Stats->Id;
	Magazine = Stats->MagazineSize;
	Reserve = Stats->Reserve;
	ApplyWeapon();
}

void ACheeseStrikeWeapon::ApplyWeapon()
{
	const FWeaponStats* Found = CheeseStrikeArsenal::Find(WeaponId);
	Stats = Found != nullptr ? Found : &CheeseStrikeWeapons::Blaster();
	const FWeaponLook* Look = &WeaponLooks[0];
	for (const FWeaponLook& Each : WeaponLooks)
	{
		if (Stats->Id == FName(Each.Id))
		{
			Look = &Each;
		}
	}
	ShotVolume = Look->Volume;
	ShotPitch = Look->Pitch;
	const bool bBlade = Look->Model == EWeaponModel::Blade;
	ModelScale = bBlade ? KnifeBladeSize : FVector(Look->Scale);
	for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		Mesh->SetSkeletalMeshAsset(bBlade ? nullptr : Look->Model == EWeaponModel::Pistol ? PistolMesh.Get() : RifleMesh.Get());
	}
	for (UStaticMeshComponent* Model : {FirstPersonModel.Get(), ThirdPersonModel.Get()})
	{
		Model->SetStaticMesh(bBlade ? BlockMesh.Get() : nullptr);
	}
	SetInHand(bInHand);
}

void ACheeseStrikeWeapon::BeginPlay()
{
	Super::BeginPlay();
	// A client has its id from the server by now.
	ApplyWeapon();
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
	if (Holder == nullptr || !Holder->IsLocallyControlled())
	{
		return;
	}
	UCheeseStrikeMovementComponent* Movement = Holder->GetCheeseStrikeMovement();
	if (bInHand && Movement != nullptr)
	{
		// The weapon in hand says how its carrier moves, through the moves they send.
		Movement->SetCarryingHeavy(Stats->CarrySpeed < 1.0f);
		if (Stats->IsMelee())
		{
			const bool bLunge = IsLunging() && Holder->CanAct();
			Movement->SetLunging(bLunge);
			if (bLunge)
			{
				Holder->AddMovementInput(Holder->GetActorForwardVector(), 1.0f);
			}
		}
	}
	// The knife's stroke, in its player's view: a cut from the right, across and back.
	if (Stats->IsMelee() && bInHand)
	{
		const float Since = ServerTime(GetWorld()) - LastShotAt;
		const float Through = Stats->SwingTime > 0.0f ? FMath::Clamp(Since / Stats->SwingTime, 0.0f, 1.0f) : 1.0f;
		const float Arc = FMath::Sin(Through * PI);
		const FQuat Cut = FRotator(-35.0f * Arc, 50.0f * Arc, 20.0f * Arc).Quaternion();
		FirstPersonModel->SetRelativeTransform(FTransform(Cut) * FirstPersonModelInHand);
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
	FitToHands();
	if (UCheeseStrikeInventory* Inventory = Character->GetInventory())
	{
		Inventory->Refresh();
	}
}

void ACheeseStrikeWeapon::FitToHands()
{
	if (Holder == nullptr)
	{
		return;
	}
	PutInHand(FirstPersonMesh, Holder->GetFirstPersonMesh());
	PutInHand(ThirdPersonMesh, Holder->GetMesh());
	PutInHand(FirstPersonModel, Holder->GetFirstPersonMesh());
	PutInHand(ThirdPersonModel, Holder->GetMesh());
	for (USceneComponent* Part : {static_cast<USceneComponent*>(FirstPersonMesh), static_cast<USceneComponent*>(ThirdPersonMesh)})
	{
		Part->SetRelativeScale3D(Stats->IsMelee() ? FVector::OneVector : ModelScale);
	}
	// The blade sits in the fist, along the way the rifle would point.
	for (UStaticMeshComponent* Model : {FirstPersonModel.Get(), ThirdPersonModel.Get()})
	{
		Model->SetRelativeTransform(FTransform(KnifeBladeOffset) * Model->GetRelativeTransform());
		Model->SetRelativeScale3D(ModelScale);
	}
	FirstPersonModelInHand = FirstPersonModel->GetRelativeTransform();
}

void ACheeseStrikeWeapon::SetInHand(bool bIsInHand)
{
	bInHand = bIsInHand;
	const bool bBlade = Stats->IsMelee();
	FirstPersonMesh->SetHiddenInGame(!bInHand || bBlade);
	ThirdPersonMesh->SetHiddenInGame(!bInHand || bBlade);
	FirstPersonModel->SetHiddenInGame(!bInHand || !bBlade);
	ThirdPersonModel->SetHiddenInGame(!bInHand || !bBlade);
}

FString ACheeseStrikeWeapon::DescribeModel(const FVector& Eye) const
{
	FString Out = WeaponId.ToString();
	for (const UPrimitiveComponent* Part : {static_cast<const UPrimitiveComponent*>(FirstPersonMesh), static_cast<const UPrimitiveComponent*>(ThirdPersonMesh), static_cast<const UPrimitiveComponent*>(FirstPersonModel), static_cast<const UPrimitiveComponent*>(ThirdPersonModel)})
	{
		const FVector At = Part->GetComponentLocation() - Eye;
		Out += FString::Printf(TEXT("; %s at (%.0f, %.0f, %.0f) size %.1f shown %d"), *Part->GetName(), At.X, At.Y, At.Z, Part->Bounds.SphereRadius, Part->IsVisible() && !Part->bHiddenInGame ? 1 : 0);
	}
	return Out;
}

void ACheeseStrikeWeapon::SetSeenFromOutside(bool bOutside)
{
	for (UPrimitiveComponent* Part : {static_cast<UPrimitiveComponent*>(ThirdPersonMesh), static_cast<UPrimitiveComponent*>(ThirdPersonModel)})
	{
		Part->SetOwnerNoSee(!bOutside);
		Part->SetFirstPersonPrimitiveType(bOutside ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	}
}

void ACheeseStrikeWeapon::BeginDraw(float Now)
{
	DrawnAt = Now;
}

void ACheeseStrikeWeapon::PutAway()
{
	StopFiring();
	StopAiming();
	BurstLeft = 0;
	GetWorldTimerManager().ClearTimer(BurstTimer);
	GetWorldTimerManager().ClearTimer(PendingShotTimer);
	LungeEndsAt = 0.0f;
	if (Holder != nullptr && Holder->IsLocallyControlled())
	{
		if (UCheeseStrikeMovementComponent* Movement = Holder->GetCheeseStrikeMovement())
		{
			Movement->SetLunging(false);
			Movement->SetCarryingHeavy(false);
		}
	}
	CancelReload();
}

void ACheeseStrikeWeapon::CancelReload()
{
	GetWorldTimerManager().ClearTimer(ClipInTimer);
	GetWorldTimerManager().ClearTimer(BoltTimer);
	const bool bWasReloading = IsReloading() || GetWorldTimerManager().IsTimerActive(ReloadTimer);
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(ReloadTimer);
		ReloadEndsAt = 0.0f;
	}
	// The hands stop reloading.
	if (bWasReloading && Holder != nullptr)
	{
		for (USkeletalMeshComponent* Body : {Holder->GetFirstPersonMesh(), Holder->GetMesh()})
		{
			if (UAnimInstance* Instance = Body != nullptr ? Body->GetAnimInstance() : nullptr)
			{
				Instance->StopSlotAnimation(0.15f, UCheeseStrikeAnimInstance::SlotName());
			}
		}
	}
}

void ACheeseStrikeWeapon::ServerDraw_Implementation(uint8 Request)
{
	ACheeseStrikeCharacter* Character = Cast<ACheeseStrikeCharacter>(GetOwner());
	if (UCheeseStrikeInventory* Inventory = Character != nullptr ? Character->GetInventory() : nullptr)
	{
		Inventory->DrawAsked(this, Request);
	}
}

void ACheeseStrikeWeapon::PutInHand(USceneComponent* Rifle, USkeletalMeshComponent* Body)
{
	if (Rifle == nullptr || Body == nullptr)
	{
		return;
	}
	const FAttachmentTransformRules Snap(EAttachmentRule::SnapToTarget, false);
	const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
	// A hero's body (CheeseStrikeBodies) holds it on hand_r at its own grip: a hero's weapon_r, where it has one, stays
	// where its own guns were.
	const int32 Hero = Mesh != nullptr ? CheeseStrikeBodies::IndexOfMesh(Mesh->GetPathName()) : INDEX_NONE;
	// The rifle is modelled along its Y axis; weapon_r points its X the way the character faces.
	if (Mesh == nullptr || (Hero == INDEX_NONE && Mesh->GetRefSkeleton().FindBoneIndex(HandSocket) != INDEX_NONE))
	{
		Rifle->AttachToComponent(Body, Snap, HandSocket);
		Rifle->SetRelativeRotation(GripRotation);
		return;
	}
	Rifle->AttachToComponent(Body, Snap, FName(TEXT("hand_r")));
	Rifle->SetRelativeTransform(Hero != INDEX_NONE ? CheeseStrikeBodies::All()[Hero].HandGrip : HandGrip);
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
	if (Sound == nullptr)
	{
		return;
	}
	if (Holder != nullptr && Holder->IsLocallyControlled() && Holder->IsPlayerControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, CheeseStrikeGunfire::AttenuationAsset());
}

void ACheeseStrikeWeapon::PlayShotSound(USoundBase* Sound) const
{
	if (Sound == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	// Its own player hears it from their own hands, whole.
	if (Holder != nullptr && Holder->IsLocallyControlled() && Holder->IsPlayerControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound, ShotVolume, ShotPitch);
		return;
	}
	// Everyone else from where it was fired: fading with distance, duller further off, and only so many at once.
	const FVector From = Holder != nullptr ? Holder->GetActorLocation() : GetActorLocation();
	UGameplayStatics::PlaySoundAtLocation(this, Sound, From, FRotator::ZeroRotator, ShotVolume, ShotPitch, 0.0f, CheeseStrikeGunfire::AttenuationAsset(), CheeseStrikeGunfire::ConcurrencyAsset());
	if (CVarTraceGunfire.GetValueOnGameThread() != 0)
	{
		const APlayerController* Listener = GetWorld()->GetFirstPlayerController();
		FVector Ear;
		FVector Front;
		FVector Right;
		if (Listener != nullptr && Listener->IsLocalController())
		{
			Listener->GetAudioListenerPosition(Ear, Front, Right);
			const float Distance = FVector::Dist(Ear, From);
			UE_LOG(LogCheeseStrike, Display, TEXT("Gunfire: %s's %s heard %.0f m off at %.2f of its volume, nothing above %.0f Hz"),
				Holder != nullptr ? *Holder->DisplayName() : *GetName(), *Stats->DisplayName.ToString(), Distance / 100.0f, CheeseStrikeGunfire::Volume(Distance) * ShotVolume, CheeseStrikeGunfire::LowPass(Distance));
		}
	}
}

void ACheeseStrikeWeapon::ShowShot()
{
	if (Stats->IsMelee())
	{
		// A whoosh, until the knife has a sound of its own: the rifle's magazine sliding, quick and high.
		if (Holder != nullptr && Holder->IsLocallyControlled() && Holder->IsPlayerControlled())
		{
			UGameplayStatics::PlaySound2D(this, ClipOutSound, ShotVolume, ShotPitch);
		}
		else if (ClipOutSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ClipOutSound, GetActorLocation(), FRotator::ZeroRotator, ShotVolume, ShotPitch, 0.0f, CheeseStrikeGunfire::AttenuationAsset());
		}
		return;
	}
	PlayOnBodies(&FCheeseStrikeAnimSet::Fire);
	for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		if (Mesh->GetSkeletalMeshAsset() == RifleMesh)
		{
			Mesh->PlayAnimation(RifleFire, false);
		}
	}
	if (FireSounds.Num() > 0)
	{
		PlayShotSound(FireSounds[FMath::RandHelper(FireSounds.Num())]);
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
		if (Mesh->GetSkeletalMeshAsset() == RifleMesh)
		{
			Mesh->SetPlayRate(RifleRate);
			Mesh->PlayAnimation(RifleReload, false);
		}
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

FString ACheeseStrikeWeapon::DescribeRounds() const
{
	if (!Stats->UsesRounds())
	{
		return FString();
	}
	if (Stats->HasEndlessReserve())
	{
		return FString::Printf(TEXT("%d"), Magazine);
	}
	return FString::Printf(TEXT("%d / %d"), Magazine, Reserve);
}

bool ACheeseStrikeWeapon::IsDrawing() const
{
	return !CheeseStrikeWeapons::IsDrawn(*Stats, DrawnAt, ServerTime(GetWorld()));
}

float ACheeseStrikeWeapon::DrawnFraction() const
{
	return Stats->DrawTime > 0.0f ? FMath::Clamp((ServerTime(GetWorld()) - DrawnAt) / Stats->DrawTime, 0.0f, 1.0f) : 1.0f;
}

bool ACheeseStrikeWeapon::IsReady() const
{
	const float Now = ServerTime(GetWorld());
	return bInHand && !IsDrawing() && !IsReloading() && BurstLeft <= 0
		&& Now - LastShotAt >= CheeseStrikeWeapons::Cooldown(*Stats, LastInBurst) * 0.9f
		&& (Stats->IsMelee() || Magazine > 0);
}

bool ACheeseStrikeWeapon::IsDry() const
{
	return Stats->UsesRounds() && Magazine <= 0 && Reserve == 0;
}

bool ACheeseStrikeWeapon::IsLunging() const
{
	return Stats->IsMelee() && GetWorld() != nullptr && GetWorld()->GetTimeSeconds() < LungeEndsAt;
}

int32 ACheeseStrikeWeapon::NextShotIndex() const
{
	return CheeseStrikeWeapons::NextInRun(*Stats, LastShotIndex, ServerTime(GetWorld()) - LastShotAt);
}

float ACheeseStrikeWeapon::CurrentSpread() const
{
	return CheeseStrikeWeapons::SpreadFor(*Stats, NextShotIndex(), bAimed) / 2.0f;
}

float ACheeseStrikeWeapon::SinceHit() const
{
	return ServerTime(GetWorld()) - LastHitAt;
}

float ACheeseStrikeWeapon::SinceShot() const
{
	return ServerTime(GetWorld()) - FMath::Max(LastShotAt, ServerLastShotAt);
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
	else if (IsDrawing() && bInHand)
	{
		// A pull while it is still coming up fires as soon as it is up, held or not.
		const float Left = Stats->DrawTime - (ServerTime(GetWorld()) - DrawnAt);
		GetWorldTimerManager().SetTimer(PendingShotTimer, this, &ACheeseStrikeWeapon::FireOnce, FMath::Max(Left, 0.01f), false);
	}
}

void ACheeseStrikeWeapon::StopFiring()
{
	bTriggerHeld = false;
	GetWorldTimerManager().ClearTimer(FireTimer);
}

void ACheeseStrikeWeapon::StartAiming()
{
	// A knife has no sights.
	bAimed = !Stats->IsMelee();
}

void ACheeseStrikeWeapon::StopAiming()
{
	bAimed = false;
}

void ACheeseStrikeWeapon::FireOnce()
{
	if (Holder == nullptr || !Holder->IsLocallyControlled() || !Holder->CanAct() || !bInHand)
	{
		return;
	}
	const float Now = ServerTime(GetWorld());
	if (IsDrawing() || IsReloading() || BurstLeft > 0 || Now - LastShotAt < CheeseStrikeWeapons::Cooldown(*Stats, LastInBurst) * 0.9f)
	{
		return;
	}
	if (Stats->IsMelee())
	{
		Strike(Now);
		return;
	}
	if (Magazine <= 0)
	{
		PlaySound(DryFireSound);
		StartReload();
		return;
	}
	FireShot(0);
	// A burst goes on by itself, a shot every BurstInterval.
	BurstLeft = FMath::Min(Stats->Burst - 1, Magazine);
	if (BurstLeft > 0)
	{
		GetWorldTimerManager().SetTimer(BurstTimer, this, &ACheeseStrikeWeapon::FireBurstShot, Stats->BurstInterval, false);
	}
}

void ACheeseStrikeWeapon::FireBurstShot()
{
	if (BurstLeft <= 0 || Holder == nullptr || !Holder->CanAct() || !bInHand || Magazine <= 0)
	{
		BurstLeft = 0;
		return;
	}
	const int32 InBurst = Stats->Burst - BurstLeft;
	--BurstLeft;
	FireShot(InBurst);
	if (BurstLeft > 0)
	{
		GetWorldTimerManager().SetTimer(BurstTimer, this, &ACheeseStrikeWeapon::FireBurstShot, Stats->BurstInterval, false);
	}
}

void ACheeseStrikeWeapon::FireShot(int32 InBurst)
{
	const float Now = ServerTime(GetWorld());
	const int32 Index = NextShotIndex();
	const int32 Seed = FMath::Rand();
	const UCameraComponent* Camera = Holder->GetFirstPersonCameraComponent();
	const FVector Origin = Camera->GetComponentLocation();
	// A player's view is their camera's; a bot's camera takes its control rotation only when someone looks through it, so a bot aims by that.
	const FVector Aim = Holder->IsPlayerControlled() ? Camera->GetForwardVector() : Holder->GetControlRotation().Vector();
	// The view climbs by the shot's kick, so the next lands where the crosshair is: by its degrees, whatever the player's
	// look sensitivity and however they have their look inverted.
	const FVector2D Kick = CheeseStrikeWeapons::RecoilFor(*Stats, Index, bAimed) - CheeseStrikeWeapons::RecoilFor(*Stats, Index - 1, bAimed);
	Holder->AddControllerYawInput(Kick.X);
	Holder->AddControllerPitchInput(Kick.Y);
	if (!HasAuthority())
	{
		--Magazine;
	}
	LastShotAt = Now;
	LastShotIndex = Index;
	LastInBurst = InBurst;
	++ShotsFired;
	ShowShot();
	ServerFire(Origin, Aim, Seed, Index, bAimed, Now);
}

void ACheeseStrikeWeapon::Strike(float Now)
{
	const UCameraComponent* Camera = Holder->GetFirstPersonCameraComponent();
	const FVector Origin = Camera->GetComponentLocation();
	const FVector Aim = Holder->IsPlayerControlled() ? Camera->GetForwardVector() : Holder->GetControlRotation().Vector();
	LastShotAt = Now;
	LastShotIndex = 0;
	LastInBurst = 0;
	++ShotsFired;
	// The striker lunges as the stroke goes (Tick, through the movement's saved moves).
	LungeEndsAt = GetWorld()->GetTimeSeconds() + Stats->SwingTime;
	ShowShot();
	ServerStrike(Origin, Aim, Now);
}

bool ACheeseStrikeWeapon::ServerAllowsShot(float Now)
{
	if (!bInHand || !CheeseStrikeWeapons::IsDrawn(*Stats, DrawnAt, Now, CheeseStrikeWeapons::DrawAllowance))
	{
		return false;
	}
	ServerShotTimes.RemoveAll([Now](float Time) { return Time < Now - 1.0f; });
	const float Allowed = CheeseStrikeWeapons::ShotsPerSecond(*Stats) + RateBurst;
	if (ServerShotTimes.Num() >= Allowed || (Stats->UsesRounds() && Magazine <= 0) || IsReloading())
	{
		return false;
	}
	ServerShotTimes.Add(Now);
	ServerLastShotAt = Now;
	return true;
}

bool ACheeseStrikeWeapon::ServerTrustsOrigin(const FVector& Origin, float When) const
{
	const FHitHistory::FSample ShooterThen = Holder->GetHitHistory().At(When);
	const FVector EyesThen = Holder->GetHitHistory().Num() > 0 ? ShooterThen.Head : Holder->GetFirstPersonCameraComponent()->GetComponentLocation();
	if (FVector::Dist(Origin, EyesThen) > OriginTolerance)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Shot from %s refused: from %.0f cm away from them"), *Holder->GetName(), FVector::Dist(Origin, EyesThen));
		return false;
	}
	return true;
}

ACheeseStrikeWeapon::FRayHit ACheeseStrikeWeapon::TraceRay(const FVector& Origin, const FVector& Direction, float Range, float When) const
{
	FRayHit Hit;
	UWorld* World = GetWorld();
	// What the arena stops it at, with every player left out: they are tested where they were.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheeseStrikeShot), true, Holder);
	TArray<ACheeseStrikeCharacter*> Players;
	for (TActorIterator<ACheeseStrikeCharacter> It(World); It; ++It)
	{
		Players.Add(*It);
		Params.AddIgnoredActor(*It);
	}
	const FVector End = Origin + Direction * Range;
	Hit.bWall = World->LineTraceSingleByChannel(Hit.Wall, Origin, End, ECC_Visibility, Params);
	Hit.Distance = Hit.bWall ? Hit.Wall.Distance : Range;

	// The nearest player along it, where they were when the shooter fired.
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
		if (Nearest >= 0.0f && Nearest < Hit.Distance)
		{
			Hit.Distance = Nearest;
			Hit.Victim = Player;
			Hit.bHead = bThisHead;
			Hit.VictimYaw = Then.Yaw;
		}
	}
	return Hit;
}

void ACheeseStrikeWeapon::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, int32 Seed, int32 Index, bool bWasAimed, float ClientTime)
{
	UWorld* World = GetWorld();
	if (Holder == nullptr || World == nullptr || !Holder->CanAct() || Stats->IsMelee())
	{
		return;
	}
	const float Now = ServerTime(World);
	if (!ServerAllowsShot(Now))
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Shot from %s refused: not in hand or drawn, the rate, or the magazine"), *Holder->GetName());
		return;
	}
	// Firing gives up the protection of a fresh spawn.
	Holder->EndProtection();
	// When the shooter fired, as near as can be told: never ahead of now, never further back than is kept.
	const float When = FMath::Clamp(ClientTime, Now - FHitHistory::Kept, Now);
	// The shot must have come from about where the shooter's eyes were.
	if (!ServerTrustsOrigin(Origin, When))
	{
		return;
	}
	--Magazine;
	MulticastShot();

	// Which ways it went: the shooter's aim, each pellet spread as the shooter's seed had it.
	const int32 RunIndex = FMath::Max(0, Index);
	FRandomStream Random(Seed);
	const float Cone = CheeseStrikeWeapons::SpreadFor(*Stats, RunIndex, bWasAimed);
	struct FHurt
	{
		float Damage = 0.0f;
		bool bHead = false;
		float Distance = 0.0f;
		int32 Pellets = 0;
	};
	TMap<ACheeseStrikeCharacter*, FHurt> Hurt;
	for (int32 Pellet = 0; Pellet < FMath::Max(Stats->Pellets, 1); ++Pellet)
	{
		const FVector Direction = CheeseStrikeWeapons::Spread(FVector(AimDirection).GetSafeNormal(), Cone, Random);
		const FRayHit Hit = TraceRay(Origin, Direction, Stats->MaxRange, When);
		if (Hit.Victim != nullptr)
		{
			FHurt& Theirs = Hurt.FindOrAdd(Hit.Victim);
			Theirs.Damage += CheeseStrikeWeapons::DamageAt(*Stats, Hit.Distance, Hit.bHead);
			Theirs.bHead |= Hit.bHead;
			Theirs.Distance = Hit.Distance;
			++Theirs.Pellets;
		}
		else if (Hit.bWall && Hit.Wall.GetActor() != nullptr)
		{
			// Something of the arena's, or the template's, which takes the engine's damage.
			const float Damage = CheeseStrikeWeapons::DamageAt(*Stats, Hit.Distance, false);
			UGameplayStatics::ApplyPointDamage(Hit.Wall.GetActor(), Damage, Direction, Hit.Wall, Holder->GetController(), this, nullptr);
			if (Hit.Wall.GetActor()->IsA<APawn>() || Hit.Wall.GetActor()->IsA<ACheeseStrikeMouse>())
			{
				ClientHit(Damage, false);
			}
			UE_LOG(LogCheeseStrike, Verbose, TEXT("Shot: %s hit %s at %.0f cm"), *Holder->GetName(), *Hit.Wall.GetActor()->GetName(), Hit.Distance);
		}
	}
	for (const TPair<ACheeseStrikeCharacter*, FHurt>& Each : Hurt)
	{
		ACheeseStrikeCharacter* Victim = Each.Key;
		const FHurt& Theirs = Each.Value;
		if (Victim->TakeShot(Theirs.Damage, Holder, Theirs.bHead))
		{
			ClientHit(Theirs.Damage, Theirs.bHead);
			UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s with the %s%s at %.0f cm for %.0f%s (health %.0f)"), *Holder->GetName(), *Victim->GetName(), *Stats->DisplayName.ToString(),
				Theirs.bHead ? TEXT(" on the head") : TEXT(""), Theirs.Distance, Theirs.Damage,
				Stats->Pellets > 1 ? *FString::Printf(TEXT(", %d of %d pellets"), Theirs.Pellets, Stats->Pellets) : TEXT(""), Victim->GetHealth());
		}
		else
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("Shot: %s hit %s, who is protected"), *Holder->GetName(), *Victim->GetName());
		}
	}
}

void ACheeseStrikeWeapon::ServerStrike_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, float ClientTime)
{
	UWorld* World = GetWorld();
	if (Holder == nullptr || World == nullptr || !Holder->CanAct() || !Stats->IsMelee())
	{
		return;
	}
	const float Now = ServerTime(World);
	// A stroke no sooner than half its interval after the last, for the network's jitter.
	if (Now - ServerLastStrike < Stats->FireInterval * 0.5f || !ServerAllowsShot(Now))
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("Stroke from %s refused: not in hand or drawn, or too soon"), *Holder->GetName());
		return;
	}
	const float When = FMath::Clamp(ClientTime, Now - FHitHistory::Kept, Now);
	if (!ServerTrustsOrigin(Origin, When))
	{
		return;
	}
	ServerLastStrike = Now;
	Holder->EndProtection();
	MulticastShot();
	// Straight ahead first, then fanned out to the sides: the first that reaches someone lands.
	const TArray<FVector> Fan = CheeseStrikeWeapons::SweepDirections(*Stats, FVector(AimDirection).GetSafeNormal());
	const FRayHit* Struck = nullptr;
	TArray<FRayHit> Hits;
	for (const FVector& Direction : Fan)
	{
		Hits.Add(TraceRay(Origin, Direction, Stats->Reach, When));
	}
	for (int32 Ray = 0; Ray < Hits.Num() && Struck == nullptr; ++Ray)
	{
		if (Hits[Ray].Victim != nullptr)
		{
			Struck = &Hits[Ray];
			const float Damage = CheeseStrikeWeapons::StrokeDamage(*Stats, Fan[Ray], Struck->VictimYaw);
			const bool bBack = CheeseStrikeWeapons::FromBehind(Fan[Ray], Struck->VictimYaw);
			if (Struck->Victim->TakeShot(Damage, Holder, false))
			{
				ClientHit(Damage, false);
				UE_LOG(LogCheeseStrike, Display, TEXT("Stroke: %s cut %s%s at %.0f cm for %.0f (health %.0f)"), *Holder->GetName(), *Struck->Victim->GetName(), bBack ? TEXT(" in the back") : TEXT(""), Struck->Distance, Damage, Struck->Victim->GetHealth());
			}
		}
	}
	if (Struck != nullptr)
	{
		return;
	}
	for (int32 Ray = 0; Ray < Hits.Num(); ++Ray)
	{
		AActor* Thing = Hits[Ray].bWall ? Hits[Ray].Wall.GetActor() : nullptr;
		if (Thing != nullptr && (Thing->IsA<APawn>() || Thing->IsA<ACheeseStrikeMouse>()))
		{
			UGameplayStatics::ApplyPointDamage(Thing, Stats->Damage, Fan[Ray], Hits[Ray].Wall, Holder->GetController(), this, nullptr);
			ClientHit(Stats->Damage, false);
			return;
		}
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
	if (Holder == nullptr || !Holder->IsLocallyControlled() || !bInHand || !Stats->UsesRounds() || IsReloading() || Magazine >= Stats->MagazineSize || Reserve == 0)
	{
		return;
	}
	StopFiring();
	ShowReload();
	ServerReload();
}

void ACheeseStrikeWeapon::AddReserve(int32 Rounds)
{
	if (HasAuthority() && Stats->UsesRounds() && !Stats->HasEndlessReserve())
	{
		Reserve = FMath::Clamp(Reserve + Rounds, 0, Stats->ReserveMax);
	}
}

void ACheeseStrikeWeapon::ServerReload_Implementation()
{
	if (!bInHand || IsReloading() || Magazine >= Stats->MagazineSize || Reserve == 0)
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
	const int32 Taken = Stats->HasEndlessReserve() ? Wanted : FMath::Min(Wanted, Reserve);
	Magazine += Taken;
	if (!Stats->HasEndlessReserve())
	{
		Reserve -= Taken;
	}
	ReloadEndsAt = 0.0f;
}
