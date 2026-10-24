// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeWeapon.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundBase.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Animation/CheeseStrikeBodies.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
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
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CheeseStrike.h"
#include "Animation/CheeseStrikeArmsPose.h"
#include "CheeseStrikeArsenal.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGunfire.h"
#include "CheeseStrikeInventory.h"
#include "CheeseStrikeWeaponLooks.h"
#include "Map/CheeseStrikeMouse.h"
#include "Rules/CheeseStrikeModes.h"
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
	/** How long a muzzle flash shows, and how bright its light is (lumens) and how far it reaches. */
	constexpr float FlashTime = 0.05f;
	constexpr float FlashBrightness = 300.0f;
	constexpr float FlashReach = 350.0f;

	float ServerTime(const UWorld* World)
	{
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
	}

	/** The asset at `Path`, loaded now if it isn't yet; null for none. */
	template <typename T>
	T* LoadLook(const TCHAR* Path)
	{
		return Path != nullptr ? LoadObject<T>(nullptr, Path) : nullptr;
	}

	/** One of `Sounds` at random, or null. */
	USoundBase* AnyOf(const TArray<TObjectPtr<USoundBase>>& Sounds)
	{
		return Sounds.Num() > 0 ? Sounds[FMath::RandHelper(Sounds.Num())].Get() : nullptr;
	}
}

ACheeseStrikeWeapon::ACheeseStrikeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Stats = &CheeseStrikeWeapons::Blaster();
	WeaponId = Stats->Id;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Lyra's rifle (Content/LICENSES.md), until it becomes the weapon it is; then its look's model, loaded as it does.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rifle(TEXT("/Game/Weapons/Rifle/Mesh/SK_Rifle.SK_Rifle"));

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

	// The second gun of two, in the left hand.
	FirstPersonOffHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("First Person Off Hand"));
	FirstPersonOffHand->SetupAttachment(RootComponent);
	FirstPersonOffHand->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonOffHand->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonOffHand->bOnlyOwnerSee = true;
	FirstPersonOffHand->SetHiddenInGame(true);
	ThirdPersonOffHand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Third Person Off Hand"));
	ThirdPersonOffHand->SetupAttachment(RootComponent);
	ThirdPersonOffHand->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonOffHand->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonOffHand->bOwnerNoSee = true;
	ThirdPersonOffHand->SetHiddenInGame(true);

	// The muzzle's flash: a star of light in its player's view and on the body, and a moment's light round it.
	FirstPersonFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("First Person Flash"));
	FirstPersonFlash->SetupAttachment(RootComponent);
	FirstPersonFlash->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonFlash->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonFlash->bOnlyOwnerSee = true;
	FirstPersonFlash->SetCastShadow(false);
	FirstPersonFlash->SetHiddenInGame(true);
	ThirdPersonFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Third Person Flash"));
	ThirdPersonFlash->SetupAttachment(RootComponent);
	ThirdPersonFlash->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonFlash->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonFlash->bOwnerNoSee = true;
	ThirdPersonFlash->SetCastShadow(false);
	ThirdPersonFlash->SetHiddenInGame(true);
	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash Light"));
	FlashLight->SetupAttachment(RootComponent);
	FlashLight->SetIntensityUnits(ELightUnits::Lumens);
	FlashLight->SetIntensity(FlashBrightness);
	FlashLight->SetAttenuationRadius(FlashReach);
	FlashLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.38f));
	FlashLight->SetCastShadows(false);
	FlashLight->SetVisibility(false);

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
}

void ACheeseStrikeWeapon::ApplyWeapon()
{
	const FWeaponStats* Found = CheeseStrikeArsenal::Find(WeaponId);
	Stats = Found != nullptr ? Found : &CheeseStrikeWeapons::Blaster();
	Look = &CheeseStrikeWeaponLooks::For(Stats->Id);
	USkeletalMesh* Skinned = LoadLook<USkeletalMesh>(Look->SkinnedModel);
	UStaticMesh* Plain = LoadLook<UStaticMesh>(Look->PlainModel);
	for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
	{
		Mesh->SetSkeletalMeshAsset(Skinned);
	}
	for (UStaticMeshComponent* Model : {FirstPersonModel.Get(), ThirdPersonModel.Get()})
	{
		Model->SetStaticMesh(Plain);
	}
	// Two guns: the second the same model.
	for (UStaticMeshComponent* Model : {FirstPersonOffHand.Get(), ThirdPersonOffHand.Get()})
	{
		Model->SetStaticMesh(Stats->bDual ? Plain : nullptr);
	}
	// Its cheese, slot by slot.
	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Look->Materials); ++Slot)
	{
		if (UMaterialInterface* Material = LoadLook<UMaterialInterface>(Look->Materials[Slot]))
		{
			for (UPrimitiveComponent* Part : {GetModel(true), GetModel(false), static_cast<UPrimitiveComponent*>(FirstPersonOffHand), static_cast<UPrimitiveComponent*>(ThirdPersonOffHand)})
			{
				if (Part != nullptr && Slot < Part->GetNumMaterials())
				{
					Part->SetMaterial(Slot, Material);
				}
			}
		}
	}
	// The flash, if it has one.
	UStaticMesh* FlashModel = Look->FlashSize > 0.0f ? LoadLook<UStaticMesh>(CheeseStrikeWeaponLooks::FlashModel) : nullptr;
	UMaterialInterface* FlashSide = FlashModel != nullptr ? LoadLook<UMaterialInterface>(CheeseStrikeWeaponLooks::FlashMaterial) : nullptr;
	UMaterialInterface* FlashFront = FlashModel != nullptr ? LoadLook<UMaterialInterface>(CheeseStrikeWeaponLooks::FlashFrontMaterial) : nullptr;
	for (UStaticMeshComponent* Star : {FirstPersonFlash.Get(), ThirdPersonFlash.Get()})
	{
		Star->SetStaticMesh(FlashModel);
		if (FlashSide != nullptr && FlashFront != nullptr)
		{
			// The cards along the barrel, and the one facing along it (tools/art/weapon_models.py's Side and Front).
			Star->SetMaterialByName(FName(TEXT("SideMat")), FlashSide);
			Star->SetMaterialByName(FName(TEXT("FrontMat")), FlashFront);
		}
	}
	ModelFire = LoadLook<UAnimSequence>(Look->FireAnimation);
	ModelReload = LoadLook<UAnimSequence>(Look->ReloadAnimation);
	FireSounds.Reset();
	const bool bKnife = Stats->IsMelee();
	for (const TCHAR* Path : Look->Shots)
	{
		if (USoundBase* Sound = LoadLook<USoundBase>(Path))
		{
			FireSounds.Add(Sound);
		}
	}
	if (bKnife)
	{
		for (const TCHAR* Path : CheeseStrikeWeaponLooks::KnifeSwings)
		{
			FireSounds.Add(LoadLook<USoundBase>(Path));
		}
		HitSounds.Reset();
		for (const TCHAR* Path : CheeseStrikeWeaponLooks::KnifeHits)
		{
			HitSounds.Add(LoadLook<USoundBase>(Path));
		}
	}
	DrawSounds.Reset();
	for (const TCHAR* Path : CheeseStrikeWeaponLooks::DrawSounds)
	{
		DrawSounds.Add(LoadLook<USoundBase>(Path));
	}
	FireSounds.RemoveAll([](const TObjectPtr<USoundBase>& Sound) { return Sound == nullptr; });
	HitSounds.RemoveAll([](const TObjectPtr<USoundBase>& Sound) { return Sound == nullptr; });
	DrawSounds.RemoveAll([](const TObjectPtr<USoundBase>& Sound) { return Sound == nullptr; });
	DryFireSound = LoadLook<USoundBase>(Look->DryFire);
	ClipOutSound = LoadLook<USoundBase>(Look->ReloadSounds[0]);
	ClipInSound = LoadLook<USoundBase>(Look->ReloadSounds[1]);
	BoltSound = LoadLook<USoundBase>(Look->ReloadSounds[2]);
	ShotVolume = Look->ShotVolume;
	ShotPitch = Look->ShotPitch;
	SetInHand(bInHand);
	FitToHands();
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
	// The flash goes out.
	if (FlashUntil > 0.0f && GetWorld()->GetTimeSeconds() >= FlashUntil)
	{
		FlashUntil = 0.0f;
		FirstPersonFlash->SetHiddenInGame(true);
		ThirdPersonFlash->SetHiddenInGame(true);
		FlashLight->SetVisibility(false);
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
	if (Holder == nullptr || Look == nullptr)
	{
		return;
	}
	const TPair<UPrimitiveComponent*, USkeletalMeshComponent*> Parts[] = {
		{FirstPersonMesh, Holder->GetFirstPersonMesh()}, {ThirdPersonMesh, Holder->GetMesh()},
		{FirstPersonModel, Holder->GetFirstPersonMesh()}, {ThirdPersonModel, Holder->GetMesh()}};
	for (const TPair<UPrimitiveComponent*, USkeletalMeshComponent*>& Part : Parts)
	{
		PutInHand(Part.Key, Part.Value);
		// The model's own size, turn and place in the grip, then the grip's in the hand.
		Part.Key->SetRelativeTransform(Look->Model * Part.Key->GetRelativeTransform());
	}
	// The second gun of two, in the left hand as the first is in the right, mirrored across the body.
	if (Stats->bDual)
	{
		static const FName LeftHand(TEXT("hand_l"));
		for (const TPair<UStaticMeshComponent*, bool>& Off : {TPair<UStaticMeshComponent*, bool>(FirstPersonOffHand, true), TPair<UStaticMeshComponent*, bool>(ThirdPersonOffHand, false)})
		{
			USkeletalMeshComponent* Body = Off.Value ? Holder->GetFirstPersonMesh() : Holder->GetMesh();
			const UPrimitiveComponent* First = GetModel(Off.Value);
			if (Body == nullptr || First == nullptr || Body->GetBoneIndex(LeftHand) == INDEX_NONE)
			{
				continue;
			}
			Off.Key->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, LeftHand);
			Off.Key->SetRelativeTransform(MirroredGrip(Body, First->GetAttachSocketName(), First->GetRelativeTransform(), LeftHand));
		}
	}
	PutFlashOn(false);
}

void ACheeseStrikeWeapon::PutFlashOn(bool bLeft)
{
	if (Look == nullptr)
	{
		return;
	}
	// The flash at the muzzle, the light just in front of it.
	const FVector Muzzle = Look->Muzzle;
	const float Size = FMath::Max(Look->FlashSize, 0.01f);
	for (const TPair<UStaticMeshComponent*, bool>& Star : {TPair<UStaticMeshComponent*, bool>(FirstPersonFlash, true), TPair<UStaticMeshComponent*, bool>(ThirdPersonFlash, false)})
	{
		UPrimitiveComponent* Model = bLeft ? GetOffHandModel(Star.Value) : GetModel(Star.Value);
		if (Model != nullptr)
		{
			Star.Key->AttachToComponent(Model, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			// In the model's space, which may be scaled: the flash keeps its own size.
			const FVector Scale = Model->GetComponentScale();
			Star.Key->SetRelativeLocation(Muzzle);
			Star.Key->SetRelativeScale3D(FVector(Size) / Scale.GetMax());
		}
	}
	if (UPrimitiveComponent* Model = bLeft ? GetOffHandModel(false) : GetModel(false))
	{
		FlashLight->AttachToComponent(Model, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FlashLight->SetRelativeLocation(Muzzle + FVector(0.0f, 10.0f, 0.0f) / Model->GetComponentScale().GetMax());
	}
}

ECheeseStrikeHold ACheeseStrikeWeapon::GetHold() const
{
	return Look != nullptr ? Look->Hold : ECheeseStrikeHold::Rifle;
}

FVector ACheeseStrikeWeapon::GetModelAlong() const
{
	return Look != nullptr ? Look->Along : FVector(0.0, 1.0, 0.0);
}

UPrimitiveComponent* ACheeseStrikeWeapon::GetModel(bool bFirstPerson) const
{
	const bool bPlain = Look != nullptr && Look->PlainModel != nullptr;
	if (bPlain)
	{
		return bFirstPerson ? static_cast<UPrimitiveComponent*>(FirstPersonModel) : static_cast<UPrimitiveComponent*>(ThirdPersonModel);
	}
	return bFirstPerson ? static_cast<UPrimitiveComponent*>(FirstPersonMesh) : static_cast<UPrimitiveComponent*>(ThirdPersonMesh);
}

bool ACheeseStrikeWeapon::GetSight(FVector& Out) const
{
	if (Look == nullptr || !Look->Sight.IsSet())
	{
		return false;
	}
	Out = Look->Sight.GetValue();
	return true;
}

bool ACheeseStrikeWeapon::LeftHandHoldFor(const USkeletalMeshComponent* Body, FVector& OutHold) const
{
	const UPrimitiveComponent* Model = Holder != nullptr ? GetModel(Body == Holder->GetFirstPersonMesh()) : nullptr;
	if (Look == nullptr || !Look->LeftHand.IsSet() || Model == nullptr)
	{
		return false;
	}
	// The model rides the bone the hand follows (hand_r, or the mannequin's weapon_r): from its space into the bone's.
	OutHold = Model->GetRelativeTransform().TransformPosition(Look->LeftHand.GetValue());
	return true;
}

void ACheeseStrikeWeapon::SetInHand(bool bIsInHand)
{
	bInHand = bIsInHand;
	const bool bPlain = Look != nullptr && Look->PlainModel != nullptr;
	FirstPersonMesh->SetHiddenInGame(!bInHand || bPlain);
	ThirdPersonMesh->SetHiddenInGame(!bInHand || bPlain);
	FirstPersonModel->SetHiddenInGame(!bInHand || !bPlain);
	ThirdPersonModel->SetHiddenInGame(!bInHand || !bPlain);
	const bool bTwo = Stats != nullptr && Stats->bDual && FirstPersonOffHand->GetStaticMesh() != nullptr;
	FirstPersonOffHand->SetHiddenInGame(!bInHand || !bTwo);
	ThirdPersonOffHand->SetHiddenInGame(!bInHand || !bTwo);
	if (!bInHand)
	{
		FirstPersonFlash->SetHiddenInGame(true);
		ThirdPersonFlash->SetHiddenInGame(true);
		FlashLight->SetVisibility(false);
	}
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
	for (UPrimitiveComponent* Part : {static_cast<UPrimitiveComponent*>(ThirdPersonMesh), static_cast<UPrimitiveComponent*>(ThirdPersonModel), static_cast<UPrimitiveComponent*>(ThirdPersonFlash), static_cast<UPrimitiveComponent*>(ThirdPersonOffHand)})
	{
		Part->SetOwnerNoSee(!bOutside);
		Part->SetFirstPersonPrimitiveType(bOutside ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	}
}

void ACheeseStrikeWeapon::BeginDraw(float Now)
{
	DrawnAt = Now;
	// Drawn in play (not put in hand at once): the hands bring it up, and it is heard.
	if (Now > -999.0f)
	{
		ShowDraw();
	}
}

void ACheeseStrikeWeapon::ShowDraw()
{
	if (Holder == nullptr || Stats->DrawTime <= 0.0f)
	{
		return;
	}
	const float DrawTime = Stats->DrawTime;
	PlayOnBodies(&FCheeseStrikeAnimSet::Equip, [DrawTime](const UAnimSequence* Equip) { return Equip->GetPlayLength() > 0.0f ? Equip->GetPlayLength() / DrawTime : 1.0f; });
	PlaySound(AnyOf(DrawSounds));
}

void ACheeseStrikeWeapon::Flash()
{
	if (Look == nullptr || Look->FlashSize <= 0.0f || GetWorld() == nullptr)
	{
		return;
	}
	// Two guns fire in turn, each from its own muzzle.
	if (Stats->bDual)
	{
		PutFlashOn(CheeseStrikeWeapons::DualHand(*Stats, ShotsShown++) == 1);
	}
	// A different star each time: turned about the barrel at random (the flash's cards came in pointing back, so turned round).
	const FQuat Turn = FQuat(FVector::YAxisVector, FMath::FRandRange(0.0f, 2.0f * PI)) * FQuat(FVector::ZAxisVector, PI);
	for (UStaticMeshComponent* Star : {FirstPersonFlash.Get(), ThirdPersonFlash.Get()})
	{
		Star->SetRelativeRotation(Turn);
		Star->SetHiddenInGame(Star->GetStaticMesh() == nullptr);
	}
	FlashLight->SetVisibility(true);
	FlashUntil = GetWorld()->GetTimeSeconds() + FlashTime;
}

void ACheeseStrikeWeapon::PutAway()
{
	StopFiring();
	StopAiming();
	BurstLeft = 0;
	GetWorldTimerManager().ClearTimer(BurstTimer);
	GetWorldTimerManager().ClearTimer(PendingShotTimer);
	StrokeShownAt = -1000.0f;
	if (Holder != nullptr && Holder->IsLocallyControlled())
	{
		if (UCheeseStrikeMovementComponent* Movement = Holder->GetCheeseStrikeMovement())
		{
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

FTransform ACheeseStrikeWeapon::MirroredGrip(const USkeletalMeshComponent* Body, FName FromBone, const FTransform& InFromBone, FName ToBone)
{
	const USkeletalMesh* Mesh = Body != nullptr ? Body->GetSkeletalMeshAsset() : nullptr;
	if (Mesh == nullptr)
	{
		return InFromBone;
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 From = Skeleton.FindBoneIndex(FromBone);
	const int32 To = Skeleton.FindBoneIndex(ToBone);
	if (From == INDEX_NONE || To == INDEX_NONE)
	{
		return InFromBone;
	}
	// In the reference pose, which stands the same both sides: where it is in the body's space, mirrored across the
	// body's middle (its X is across), then taken into the other bone. A mirror on both sides of a turn is a turn again,
	// so the thing held keeps its own handedness.
	const FTransform FromInBody = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, From);
	const FTransform ToInBody = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, To);
	const FTransform HeldInBody = InFromBone * FromInBody;
	const FMatrix Mirror = FScaleMatrix(FVector(-1.0, 1.0, 1.0));
	FTransform Mirrored(Mirror * HeldInBody.ToMatrixNoScale() * Mirror);
	Mirrored.SetScale3D(HeldInBody.GetScale3D());
	return Mirrored.GetRelativeTransform(ToInBody);
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
	// Each body plays its own skeleton's copy, from the set for how this weapon is held (the body may not have caught up).
	for (USkeletalMeshComponent* Body : {Holder->GetFirstPersonMesh(), Holder->GetMesh()})
	{
		UCheeseStrikeAnimInstance* Instance = Body != nullptr ? Cast<UCheeseStrikeAnimInstance>(Body->GetAnimInstance()) : nullptr;
		UAnimSequence* Animation = Instance != nullptr ? (Instance->GetSet(GetHold()).*Which).Get() : nullptr;
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
		// The knife's swipe (the arms play it from SwipePhase), with a swoosh.
		StrokeShownAt = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
		USoundBase* Swoosh = AnyOf(FireSounds);
		if (Holder != nullptr && Holder->IsLocallyControlled() && Holder->IsPlayerControlled())
		{
			UGameplayStatics::PlaySound2D(this, Swoosh, ShotVolume);
		}
		else if (Swoosh != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Swoosh, GetActorLocation(), FRotator::ZeroRotator, ShotVolume, 1.0f, 0.0f, CheeseStrikeGunfire::AttenuationAsset());
		}
		return;
	}
	PlayOnBodies(&FCheeseStrikeAnimSet::Fire);
	if (ModelFire != nullptr)
	{
		for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
		{
			if (Mesh->GetSkeletalMeshAsset() != nullptr)
			{
				Mesh->PlayAnimation(ModelFire, false);
			}
		}
	}
	Flash();
	PlayShotSound(AnyOf(FireSounds));
}

void ACheeseStrikeWeapon::ShowReload()
{
	// The animations take the reload's time, however long they were made.
	const float ReloadTime = Stats->ReloadTime;
	PlayOnBodies(&FCheeseStrikeAnimSet::Reload, [ReloadTime](const UAnimSequence* Reload) { return Reload->GetPlayLength() > 0.0f ? Reload->GetPlayLength() / ReloadTime : 1.0f; });
	if (ModelReload != nullptr)
	{
		const float ModelRate = ModelReload->GetPlayLength() > 0.0f ? ModelReload->GetPlayLength() / ReloadTime : 1.0f;
		for (USkeletalMeshComponent* Mesh : {FirstPersonMesh.Get(), ThirdPersonMesh.Get()})
		{
			if (Mesh->GetSkeletalMeshAsset() != nullptr)
			{
				Mesh->SetPlayRate(ModelRate);
				Mesh->PlayAnimation(ModelReload, false);
			}
		}
	}
	// The magazine comes out at once, goes in two thirds of the way through, and the bolt follows.
	PlaySound(ClipOutSound);
	GetWorldTimerManager().SetTimer(ClipInTimer, FTimerDelegate::CreateUObject(this, &ACheeseStrikeWeapon::PlaySound, ClipInSound.Get()), ReloadTime * 0.65f, false);
	GetWorldTimerManager().SetTimer(BoltTimer, FTimerDelegate::CreateUObject(this, &ACheeseStrikeWeapon::PlaySound, BoltSound.Get()), ReloadTime * 0.85f, false);
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
	if (HasEndlessReserve())
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
		&& Now - LastShotAt >= CheeseStrikeWeapons::Cooldown(FiringStats(), LastInBurst) * 0.9f
		&& (Stats->IsMelee() || Magazine > 0);
}

bool ACheeseStrikeWeapon::IsDry() const
{
	return Stats->UsesRounds() && Magazine <= 0 && Reserve == 0;
}

float ACheeseStrikeWeapon::SwipePhase() const
{
	const float Since = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() - StrokeShownAt : 1000.0f;
	return Stats->IsMelee() && Since < CheeseStrikeArms::SwipeSeconds ? Since / CheeseStrikeArms::SwipeSeconds : -1.0f;
}

int32 ACheeseStrikeWeapon::NextShotIndex() const
{
	return CheeseStrikeWeapons::NextInRun(*Stats, LastShotIndex, ServerTime(GetWorld()) - LastShotAt);
}

const FWeaponStats& ACheeseStrikeWeapon::FiringStats() const
{
	if (Holder != nullptr && Holder->IsDowned())
	{
		DownedStats = CheeseStrikeModes::DownedWeapon(*Stats);
		return DownedStats;
	}
	return *Stats;
}

float ACheeseStrikeWeapon::CurrentSpread() const
{
	return CheeseStrikeWeapons::SpreadFor(FiringStats(), NextShotIndex(), bAimed) / 2.0f;
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
		GetWorldTimerManager().SetTimer(FireTimer, this, &ACheeseStrikeWeapon::FireOnce, FiringStats().FireInterval, true);
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
	// A knife has no sights, and two guns aren't aimed down one's (as Counter-Strike's Dual Berettas aren't).
	bAimed = !Stats->IsMelee() && !Stats->bDual;
}

void ACheeseStrikeWeapon::StopAiming()
{
	bAimed = false;
}

void ACheeseStrikeWeapon::FireOnce()
{
	if (Holder == nullptr || !Holder->IsLocallyControlled() || !Holder->CanAct() || Holder->InExecution() || !bInHand)
	{
		return;
	}
	const float Now = ServerTime(GetWorld());
	if (IsDrawing() || IsReloading() || BurstLeft > 0 || Now - LastShotAt < CheeseStrikeWeapons::Cooldown(FiringStats(), LastInBurst) * 0.9f)
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
	if (BurstLeft <= 0 || Holder == nullptr || !Holder->CanAct() || Holder->InExecution() || !bInHand || Magazine <= 0)
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
	const float Allowed = CheeseStrikeWeapons::ShotsPerSecond(FiringStats()) + RateBurst;
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
		const float BodyHit = FHitHistory::RayHitsBody(Origin, Direction, Then);
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
	const float Cone = CheeseStrikeWeapons::SpreadFor(FiringStats(), RunIndex, bWasAimed);
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
			Theirs.Damage += CheeseStrikeWeapons::DamageAt(FiringStats(), Hit.Distance, Hit.bHead);
			Theirs.bHead |= Hit.bHead;
			Theirs.Distance = Hit.Distance;
			++Theirs.Pellets;
		}
		else if (Hit.bWall && Hit.Wall.GetActor() != nullptr)
		{
			// Something of the arena's, or the template's, which takes the engine's damage.
			const float Damage = CheeseStrikeWeapons::DamageAt(FiringStats(), Hit.Distance, false);
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
	// The knife's stroke landing, heard by the one who struck.
	if (Stats->IsMelee())
	{
		UGameplayStatics::PlaySound2D(this, AnyOf(HitSounds));
	}
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
	if (HasAuthority() && Stats->UsesRounds() && !HasEndlessReserve())
	{
		Reserve = FMath::Clamp(Reserve + Rounds, 0, Stats->ReserveMax);
	}
}

void ACheeseStrikeWeapon::MakeReserveEndless()
{
	if (HasAuthority() && Stats->UsesRounds())
	{
		Reserve = -1;
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
	const int32 Taken = HasEndlessReserve() ? Wanted : FMath::Min(Wanted, Reserve);
	Magazine += Taken;
	if (!HasEndlessReserve())
	{
		Reserve -= Taken;
	}
	ReloadEndsAt = 0.0f;
}

void ACheeseStrikeWeapon::SetOverlay(UMaterialInterface* Overlay, UMaterialInterface* ViewOverlay)
{
	const TPair<UMeshComponent*, UMaterialInterface*> Parts[] = {
		{FirstPersonMesh.Get(), ViewOverlay}, {FirstPersonModel.Get(), ViewOverlay}, {ThirdPersonMesh.Get(), Overlay}, {ThirdPersonModel.Get(), Overlay},
		{FirstPersonOffHand.Get(), ViewOverlay}, {ThirdPersonOffHand.Get(), Overlay}};
	for (const TPair<UMeshComponent*, UMaterialInterface*>& Part : Parts)
	{
		if (Part.Key != nullptr && Part.Key->GetOverlayMaterial() != Part.Value)
		{
			Part.Key->SetOverlayMaterial(Part.Value);
		}
	}
}
