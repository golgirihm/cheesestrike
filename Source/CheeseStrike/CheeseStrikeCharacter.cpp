// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CheeseStrike.h"
#include "Animation/CheeseStrikeAnimInstance.h"
#include "Input/CheeseStrikeInput.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Map/CheeseStrikeMouse.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeInventory.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** With this on, every character writes its state to the log four times a second, for watching a run from outside. */
static TAutoConsoleVariable<int32> CVarTraceState(TEXT("CheeseStrike.TraceState"), 0, TEXT("Log each character's state four times a second."));

const FVector ACheeseStrikeCharacter::DefaultCameraAtHead(-2.8f, 5.89f, 0.0f);
const FVector ACheeseStrikeCharacter::CameraFromEyes(-7.8f, -1.0f, 0.0f);

namespace
{
	/** The body, the same for everyone and for its own player's first-person view. */
	const TCHAR* const BodyMesh = TEXT("/Game/ParagonLtBelica/Characters/Heroes/Belica/Meshes/Belica.Belica");
	/** Words in the names of a body's material slots that are its head, hidden from its own player. */
	const TCHAR* const HeadSlotWords[] = {TEXT("head"), TEXT("hair"), TEXT("eye"), TEXT("mouth"), TEXT("lash")};
}

ACheeseStrikeCharacter::ACheeseStrikeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCheeseStrikeMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	using namespace CheeseStrikeMovement;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The body everyone else sees, and the copy of it its own player sees
	// (the same mesh and animation, so the arms in view are the character's
	// own), each animated in code with Lyra's rifle set retargeted to it:
	// Lt. Belica standing on the capsule's floor, turned to face its front.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(BodyMesh);
	const TSubclassOf<UAnimInstance> BodyAnimation = UCheeseStrikeBelicaAnimInstance::StaticClass();
	GetMesh()->SetSkeletalMeshAsset(Body.Object);
	GetMesh()->SetAnimInstanceClass(BodyAnimation);
	GetMesh()->SetRelativeLocationAndRotation(FVector(-10.0f, 0.0f, -CapsuleHalfHeight), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetSkeletalMeshAsset(Body.Object);
	FirstPersonMesh->SetAnimInstanceClass(BodyAnimation);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// The eyes stand on the capsule at eye height, standing or crouched (the
	// engine moves BaseEyeHeight between the two), and the view follows them
	// smoothly rather than at once.
	BaseEyeHeight = EyeHeight;
	CrouchedEyeHeight = CrouchedEyeHeightAboveCentre;
	EyeOffset = EyeHeight;
	Eyes = CreateDefaultSubobject<USceneComponent>(TEXT("Eyes"));
	Eyes->SetupAttachment(GetCapsuleComponent());
	Eyes->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeight));

	// The camera sits on the first-person body's head, between its eyes
	// (FitFirstPersonBody); crouching lowers that body, and the camera with it.
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(CameraAtHead, FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	// The first-person body is drawn at a narrower field of view than the
	// world, which keeps a rifle this close from stretching; it isn't shrunk
	// towards the camera, which would pull it out of the hands' line.
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = false;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
	BaseFieldOfView = FirstPersonCameraComponent->FieldOfView;
	Health = CheeseStrikeWeapons::MaxHealth;

	Inventory = CreateDefaultSubobject<UCheeseStrikeInventory>(TEXT("Inventory"));
}

void ACheeseStrikeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeCharacter, Health);
	DOREPLIFETIME(ACheeseStrikeCharacter, KilledBy);
	DOREPLIFETIME(ACheeseStrikeCharacter, DiedAt);
	DOREPLIFETIME(ACheeseStrikeCharacter, ProtectedUntil);
}

UCheeseStrikeMovementComponent* ACheeseStrikeCharacter::GetCheeseStrikeMovement() const
{
	return Cast<UCheeseStrikeMovementComponent>(GetCharacterMovement());
}

bool ACheeseStrikeCharacter::FindEyes(const USkeletalMesh* Body, FName& Left, FName& Right)
{
	if (Body == nullptr)
	{
		return false;
	}
	const TPair<const TCHAR*, const TCHAR*> Pairs[] = {{TEXT("eyeball_l"), TEXT("eyeball_r")}, {TEXT("L_eye"), TEXT("R_eye")}};
	for (const TPair<const TCHAR*, const TCHAR*>& Pair : Pairs)
	{
		if (Body->GetRefSkeleton().FindBoneIndex(FName(Pair.Key)) != INDEX_NONE && Body->GetRefSkeleton().FindBoneIndex(FName(Pair.Value)) != INDEX_NONE)
		{
			Left = FName(Pair.Key);
			Right = FName(Pair.Value);
			return true;
		}
	}
	return false;
}

void ACheeseStrikeCharacter::FitFirstPersonBody()
{
	const USkeletalMesh* Asset = FirstPersonMesh->GetSkeletalMeshAsset();
	if (Asset == nullptr)
	{
		return;
	}
	const FName Head(TEXT("head"));
	FName LeftEye;
	FName RightEye;
	if (Asset->GetRefSkeleton().FindBoneIndex(Head) != INDEX_NONE && FindEyes(Asset, LeftEye, RightEye))
	{
		// Just under the eyes in the reference pose, in the head bone's own space (whose X is up the head and Y forward):
		// a shooter's view, which frames the arms and rifle and keeps the eye height of the movement's design.
		const FVector Between = (Asset->GetComposedRefPoseMatrix(LeftEye).GetOrigin() + Asset->GetComposedRefPoseMatrix(RightEye).GetOrigin()) * 0.5;
		CameraAtHead = Asset->GetComposedRefPoseMatrix(Head).InverseTransformPosition(Between) + CameraFromEyes;
	}
	else
	{
		CameraAtHead = DefaultCameraAtHead;
	}
	FirstPersonCameraComponent->SetRelativeLocation(CameraAtHead);
	// The head, which the camera is inside, is not drawn for its own player.
	const TArray<FSkeletalMaterial>& Slots = Asset->GetMaterials();
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		const FString Name = Slots[Slot].MaterialSlotName.ToString();
		bool bHead = false;
		for (const TCHAR* Word : HeadSlotWords)
		{
			bHead |= Name.Contains(Word, ESearchCase::IgnoreCase);
		}
		if (bHead)
		{
			for (int32 Level = 0; Level < Asset->GetLODNum(); ++Level)
			{
				FirstPersonMesh->ShowMaterialSection(Slot, INDEX_NONE, false, Level);
			}
		}
	}
	ACheeseStrikeWeapon::HideBuiltInWeapons(FirstPersonMesh);
}

void ACheeseStrikeCharacter::BeginPlay()
{
	Super::BeginPlay();
	ACheeseStrikeWeapon::HideBuiltInWeapons(GetMesh());
	FitFirstPersonBody();
	FirstPersonBase = FirstPersonMesh->GetRelativeTransform();
	// A fresh character can't be hurt for its first seconds, unless it fires.
	if (HasAuthority())
	{
		ProtectedUntil = ServerNow() + CheeseStrikeRules::SpawnProtection;
	}
}

void ACheeseStrikeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// The server gives every character its weapons as it is possessed, with the primary its player chose; they arrive on
	// every machine as actors of this character's (and go with it). A mode may give others after (SetPlayerDefaults).
	if (HasAuthority() && Inventory->GetHeld() == nullptr)
	{
		Inventory->GiveSpawnLoadout(NewController);
	}
}

ACheeseStrikeWeapon* ACheeseStrikeCharacter::GetWeapon() const
{
	return Inventory != nullptr ? Inventory->GetHeld() : nullptr;
}

float ACheeseStrikeCharacter::ServerNow() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
}

bool ACheeseStrikeCharacter::CanAct() const
{
	const AGameStateBase* State = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	return IsAlive() && (State == nullptr || !State->HasMatchEnded());
}

bool ACheeseStrikeCharacter::TakesInput() const
{
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	return CanAct() && (Player == nullptr || !Player->IsPaused());
}

bool ACheeseStrikeCharacter::IsProtected() const
{
	return IsAlive() && ServerNow() < ProtectedUntil;
}

FString ACheeseStrikeCharacter::DisplayName() const
{
	const APlayerState* Player = GetPlayerState();
	return Player != nullptr ? Player->GetPlayerName() : GetName();
}

FHitHistory::FSample ACheeseStrikeCharacter::SampleNow(float Time) const
{
	FHitHistory::FSample Sample;
	Sample.Time = Time;
	Sample.Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	Sample.Height = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
	Sample.Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Sample.Head = Eyes->GetComponentLocation();
	Sample.Yaw = GetActorRotation().Yaw;
	return Sample;
}

bool ACheeseStrikeCharacter::TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead)
{
	if (!HasAuthority() || !CanAct() || IsProtected())
	{
		return false;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	LastHurtBy = From;
	LastHurtAt = ServerNow();
	if (!IsAlive())
	{
		Die(From, bHead);
	}
	return true;
}

void ACheeseStrikeCharacter::Die(ACheeseStrikeCharacter* From, bool bHead)
{
	KilledBy = From != nullptr ? From->DisplayName() : FString();
	DiedAt = ServerNow();
	UE_LOG(LogCheeseStrike, Display, TEXT("%s is killed by %s"), *DisplayName(), From != nullptr ? *From->DisplayName() : TEXT("nobody"));
	ShowDeath();
	if (ACheeseStrikeGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ACheeseStrikeGameMode>() : nullptr)
	{
		Mode->Killed(From, this, bHead);
	}
}

void ACheeseStrikeCharacter::CheeseStrikeDie()
{
	if (HasAuthority())
	{
		ServerDie_Implementation();
	}
	else
	{
		ServerDie();
	}
}

void ACheeseStrikeCharacter::CheeseStrikeOutside(float Distance, float Yaw, float Height, float Rise)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	const bool bOutside = Distance > 0.0f;
	// The body and its rifle, seen as others see them, or kept from the player as in play.
	GetMesh()->SetOwnerNoSee(!bOutside);
	GetMesh()->SetFirstPersonPrimitiveType(bOutside ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	FirstPersonMesh->SetVisibility(!bOutside, true);
	for (const EWeaponSlot Slot : {EWeaponSlot::Primary, EWeaponSlot::Sidearm, EWeaponSlot::Knife})
	{
		if (ACheeseStrikeWeapon* Carried = Inventory->GetInSlot(Slot))
		{
			Carried->SetSeenFromOutside(bOutside);
		}
	}
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->Destroy();
		OutsideCamera = nullptr;
	}
	if (!bOutside)
	{
		Player->SetViewTarget(this);
		return;
	}
	// Once dead, the body lies where it fell, not where the character stood: the view is round its head.
	const FVector Middle = (bDeathShown ? GetMesh()->GetBoneLocation(TEXT("head")) : GetActorLocation()) + FVector(0.0f, 0.0f, Height);
	const FVector Way = FRotator(0.0f, GetActorRotation().Yaw + Yaw, 0.0f).Vector();
	const FVector At = Middle + Way * Distance + FVector(0.0f, 0.0f, Rise);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OutsideCamera = GetWorld()->SpawnActor<ACameraActor>(At, (Middle - At).Rotation(), Parameters);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->GetCameraComponent()->SetFieldOfView(60.0f);
		// Viewing through a camera turns the controls to face the way it does; the character keeps facing and aiming as before.
		const FRotator Kept = Player->GetControlRotation();
		Player->SetViewTarget(OutsideCamera);
		Player->SetControlRotation(Kept);
	}
}

void ACheeseStrikeCharacter::CheeseStrikeViewFrom(float X, float Y, float Z, float Pitch, float Yaw)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->Destroy();
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OutsideCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(X, Y, Z), FRotator(Pitch, Yaw, 0.0f), Parameters);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->GetCameraComponent()->SetFieldOfView(70.0f);
		const FRotator Kept = Player->GetControlRotation();
		Player->SetViewTarget(OutsideCamera);
		Player->SetControlRotation(Kept);
	}
}

void ACheeseStrikeCharacter::CheeseStrikeViewMouse(float Distance, float Yaw, float Height)
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (Player == nullptr || GetWorld() == nullptr)
	{
		return;
	}
	TActorIterator<ACheeseStrikeMouse> Mouse(GetWorld());
	if (!Mouse)
	{
		UE_LOG(LogCheeseStrike, Warning, TEXT("CheeseStrikeViewMouse: no mice"));
		return;
	}
	const FVector Middle = Mouse->GetActorLocation();
	const FVector At = Middle + FRotator(0.0f, Mouse->GetActorRotation().Yaw + Yaw, 0.0f).Vector() * Distance + FVector(0.0f, 0.0f, Height);
	CheeseStrikeViewFrom(At.X, At.Y, At.Z, (Middle - At).Rotation().Pitch, (Middle - At).Rotation().Yaw);
	if (OutsideCamera != nullptr)
	{
		OutsideCamera->AttachToActor(*Mouse, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void ACheeseStrikeCharacter::ServerDie_Implementation()
{
	EndProtection();
	TakeShot(CheeseStrikeWeapons::MaxHealth, nullptr, false);
}

void ACheeseStrikeCharacter::OnRep_Health()
{
	if (!IsAlive())
	{
		ShowDeath();
	}
}

void ACheeseStrikeCharacter::ShowDeath()
{
	if (bDeathShown)
	{
		return;
	}
	bDeathShown = true;
	if (ACheeseStrikeWeapon* Held = GetWeapon())
	{
		Held->StopFiring();
		Held->StopAiming();
	}
	// The body goes on the way the character was going, falling as it goes.
	const FVector Going = GetVelocity();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The body falls where it is, and its own player sees it fall: the view
	// stays where the eyes were, free to look round, until a new character
	// comes.
	FirstPersonCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	FirstPersonMesh->SetVisibility(false, true);
	// The body is no longer the first-person view's stand-in for lighting: it is a body, seen by all.
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetAllPhysicsLinearVelocity(Going);
}

void ACheeseStrikeCharacter::ApplyStartAt()
{
	// For watching a run from outside: -StartAt=X,Y,Z,Yaw puts the local
	// player's first character there, facing that way, instead of wherever
	// the level starts them: once, on the first tick it is possessed; the
	// characters that follow a death come back where the rules say.
	FString Spec;
	ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	if (!bStartAtApplied && IsLocallyControlled() && Player != nullptr && FParse::Value(FCommandLine::Get(), TEXT("StartAt="), Spec, false))
	{
		bStartAtApplied = true;
		if (!Player->TakeStartAt())
		{
			return;
		}
		TArray<FString> Parts;
		Spec.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			const FVector At(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			const float Yaw = Parts.Num() >= 4 ? FCString::Atof(*Parts[3]) : 0.0f;
			if (HasAuthority())
			{
				StartAt(At, Yaw);
			}
			else
			{
				ServerStartAt(At, Yaw);
			}
		}
	}
}

void ACheeseStrikeCharacter::StartAt(const FVector& At, float Yaw)
{
	SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	if (GetController() != nullptr)
	{
		GetController()->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

void ACheeseStrikeCharacter::ServerStartAt_Implementation(FVector_NetQuantize At, float Yaw)
{
	StartAt(At, Yaw);
	// The client's own view turns too: the control rotation isn't sent back.
	ClientStartAt(At, Yaw);
}

void ACheeseStrikeCharacter::ClientStartAt_Implementation(FVector_NetQuantize At, float Yaw)
{
	if (GetController() != nullptr)
	{
		GetController()->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
}

FVector ACheeseStrikeCharacter::LeftHandOnRifle() const
{
	const USkeletalMeshComponent* Rifle = GetWeapon() != nullptr ? GetWeapon()->GetFirstPersonMesh() : nullptr;
	if (Rifle == nullptr || FirstPersonMesh == nullptr)
	{
		return FVector::ZeroVector;
	}
	return Rifle->GetComponentTransform().InverseTransformPosition(FirstPersonMesh->GetBoneLocation(TEXT("hand_l")));
}

static TAutoConsoleVariable<float> CVarSightSide(TEXT("CheeseStrike.SightSide"), ACheeseStrikeCharacter::SightSide, TEXT("Where the rifle's sight line is across it, from its bounds' middle (cm)."));
static TAutoConsoleVariable<float> CVarSightHeight(TEXT("CheeseStrike.SightHeight"), ACheeseStrikeCharacter::SightHeight, TEXT("Where the rifle's sight line is, down from its top (cm)."));
static TAutoConsoleVariable<float> CVarSightBack(TEXT("CheeseStrike.SightBack"), ACheeseStrikeCharacter::SightBack, TEXT("How far along the rifle from its back end the eye is when aiming (cm)."));

void ACheeseStrikeCharacter::BringRifleToEye()
{
	const ACheeseStrikeWeapon* Held = GetWeapon();
	const USkeletalMeshComponent* Rifle = Held != nullptr ? Held->GetFirstPersonMesh() : nullptr;
	const USceneComponent* Parent = FirstPersonMesh->GetAttachParent();
	// Only for its own player, alive, with the camera riding the arms (after a death it is left where the eyes were).
	if (!IsLocallyControlled() || Parent == nullptr || !IsAlive() || FirstPersonCameraComponent->GetAttachParent() != FirstPersonMesh)
	{
		return;
	}
	// Where the arms are without aiming, and the eye in them.
	const FTransform Unaimed = FirstPersonBase * FirstPersonMesh->GetAttachParent()->GetSocketTransform(FirstPersonMesh->GetAttachSocketName());
	const FVector Eye = (FirstPersonMesh->GetSocketTransform(FName("head"), RTS_Component) * Unaimed).TransformPosition(CameraAtHead);
	// A weapon just drawn comes up from below the view, the arms with it, the eye staying where it is.
	const float Lowered = Held != nullptr ? FMath::Square(1.0f - Held->DrawnFraction()) : 0.0f;
	if (Lowered > 0.001f)
	{
		const FVector Down = -FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Z) * DrawDrop * Lowered;
		FirstPersonMesh->SetWorldLocation(Unaimed.GetLocation() + Down);
		FirstPersonCameraComponent->SetWorldLocation(Eye);
		return;
	}
	if (AimFraction < 0.001f || Rifle == nullptr || Rifle->GetSkeletalMeshAsset() == nullptr)
	{
		FirstPersonMesh->SetRelativeTransform(FirstPersonBase);
		FirstPersonCameraComponent->SetRelativeLocation(CameraAtHead);
		return;
	}
	// The rifle as the arms hold it (its place in them, from their last pose), and where it should be: its length (its Y)
	// along the view, its top (its Z) up the view, and its sight line (a little down from its top) through the eye, the
	// eye SightBack along it from its back end, behind the rear sight as a cheek on the stock puts it.
	const FTransform RifleInArms = Rifle->GetComponentTransform().GetRelativeTransform(FirstPersonMesh->GetComponentTransform());
	const FRotator View = GetControlRotation();
	const FMatrix ViewAxes = FRotationMatrix(View);
	const FQuat Along = FRotationMatrix::MakeFromYZ(ViewAxes.GetUnitAxis(EAxis::X), ViewAxes.GetUnitAxis(EAxis::Z)).ToQuat();
	const FBoxSphereBounds Bounds = Rifle->GetSkeletalMeshAsset()->GetBounds();
	const FVector Sight(Bounds.Origin.X + CVarSightSide.GetValueOnGameThread(), Bounds.Origin.Y - Bounds.BoxExtent.Y + CVarSightBack.GetValueOnGameThread(), Bounds.Origin.Z + Bounds.BoxExtent.Z - CVarSightHeight.GetValueOnGameThread());
	const FVector Scale = Rifle->GetComponentScale();
	const FTransform RifleAimed(Along, Eye - Along.RotateVector(Sight * Scale), Scale);
	// Child = its place in the parent * the parent; so the arms = the rifle's place in them, undone, * where the rifle is to be.
	const FTransform ArmsAimed = RifleInArms.Inverse() * RifleAimed;
	FTransform Arms;
	Arms.Blend(Unaimed, ArmsAimed, AimFraction);
	FirstPersonMesh->SetWorldTransform(Arms);
	// The eye stays where the head had it, wherever the arms (which carry the camera on their head) have gone.
	FirstPersonCameraComponent->SetWorldLocation(Eye);
}

void ACheeseStrikeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyStartAt();
	// The server keeps where everyone has been, for shots fired at where they were seen.
	if (HasAuthority() && GetWorld() != nullptr && GetWorld()->GetGameState() != nullptr)
	{
		HitHistory.Add(SampleNow(GetWorld()->GetGameState()->GetServerWorldTimeSeconds()));
	}
	// Aiming down the sights narrows the view, smoothly, and brings the rifle to the eye: the arms are
	// moved and turned so that the rifle's sight line runs straight down the view from just in front of
	// the eye, which stays where the head has it. Shots leave from the eye along the view, so they go
	// where the sights point.
	const bool bAimed = GetWeapon() != nullptr && GetWeapon()->IsAimed();
	const float WantedFieldOfView = BaseFieldOfView * (bAimed ? GetWeapon()->GetStats().AimZoom : 1.0f);
	FirstPersonCameraComponent->SetFieldOfView(FMath::FInterpTo(FirstPersonCameraComponent->FieldOfView, WantedFieldOfView, DeltaSeconds, 12.0f));
	AimFraction = FMath::FInterpTo(AimFraction, bAimed ? 1.0f : 0.0f, DeltaSeconds, 12.0f);
	BringRifleToEye();
	// Traced with the console variable, or with -TraceState on the command line.
	static const bool bTraceSwitch = FParse::Param(FCommandLine::Get(), TEXT("TraceState"));
	if ((bTraceSwitch || CVarTraceState.GetValueOnGameThread() != 0) && GetWorld() != nullptr)
	{
		SinceTrace += DeltaSeconds;
		if (SinceTrace >= 0.1f)
		{
			SinceTrace = 0.0f;
			if (IsLocallyControlled() && IsPlayerControlled())
			{
				CheeseStrikeState();
			}
			else if (!IsAlive())
			{
				// Another player's body, as this machine has it.
				const FVector Pelvis = GetMesh()->GetBoneLocation(TEXT("pelvis"));
				UE_LOG(LogCheeseStrike, Display, TEXT("Body of %s: pelvis (%.0f, %.0f, %.0f) fallen %d visible %d"), *DisplayName(), Pelvis.X, Pelvis.Y, Pelvis.Z, GetMesh()->IsSimulatingPhysics() ? 1 : 0, GetMesh()->IsVisible() ? 1 : 0);
			}
		}
	}
	// The eyes settle at their height for standing or crouched; the view
	// itself rides the first-person body's head, which the crouch animation
	// lowers.
	EyeOffset = FMath::FInterpTo(EyeOffset, BaseEyeHeight, DeltaSeconds, CheeseStrikeMovement::CrouchRate);
	Eyes->SetRelativeLocation(FVector(0.0f, 0.0f, EyeOffset));
}

void ACheeseStrikeCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule's centre has just dropped; the eyes stay where they were for the moment, then come down.
	EyeOffset += ScaledHalfHeightAdjust;
}

void ACheeseStrikeCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	EyeOffset -= ScaledHalfHeightAdjust;
}

void ACheeseStrikeCharacter::CheeseStrikeState()
{
	const UCheeseStrikeMovementComponent* Movement = GetCheeseStrikeMovement();
	const FVector At = GetActorLocation();
	const FVector Eye = FirstPersonCameraComponent->GetComponentLocation();
	const FVector Body = GetMesh()->GetComponentLocation();
	const ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	UE_LOG(LogCheeseStrike, Display, TEXT("State: at (%.0f, %.0f, %.0f) speed %.0f eye %.0f above the floor pitch %.1f yaw %.1f crouched %d sprinting %d sliding %d (%.2f s) falling %d jump %d/%d health %.0f alive %d protected %d ammo %d/%d aimed %d fov %.0f shots %d hits %d reloading %d weapon %s drawing %d body (%.0f, %.0f, %.0f) fallen %d camera (%.0f, %.0f, %.0f)"),
		At.X, At.Y, At.Z, GetVelocity().Size2D(), Eye.Z - (At.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::NormalizeAxis(GetControlRotation().Pitch), FRotator::NormalizeAxis(GetControlRotation().Yaw),
		bIsCrouched ? 1 : 0, Movement != nullptr && Movement->IsSprinting() ? 1 : 0, Movement != nullptr && Movement->IsSliding() ? 1 : 0,
		Movement != nullptr ? Movement->GetSlideElapsed() : -1.0f, Movement != nullptr && Movement->IsFalling() ? 1 : 0, bPressedJump ? 1 : 0, JumpCurrentCount,
		Health, IsAlive() ? 1 : 0, IsProtected() ? 1 : 0, HeldWeapon != nullptr ? HeldWeapon->GetMagazine() : -1, HeldWeapon != nullptr ? HeldWeapon->GetReserve() : -1, HeldWeapon != nullptr && HeldWeapon->IsAimed() ? 1 : 0,
		FirstPersonCameraComponent->FieldOfView, HeldWeapon != nullptr ? HeldWeapon->ShotsFired : 0, HeldWeapon != nullptr ? HeldWeapon->HitsConfirmed : 0, HeldWeapon != nullptr && HeldWeapon->IsReloading() ? 1 : 0,
		HeldWeapon != nullptr ? *HeldWeapon->GetWeaponId().ToString() : TEXT("none"), HeldWeapon != nullptr && HeldWeapon->IsDrawing() ? 1 : 0,
		Body.X, Body.Y, Body.Z, GetMesh()->IsSimulatingPhysics() ? 1 : 0, Eye.X, Eye.Y, Eye.Z);
	if (HeldWeapon != nullptr)
	{
		// Where the hands and the rifle are, in each body, relative to the eyes: for checking the weapon sits in the hands.
		const FVector FirstHand = FirstPersonMesh->GetBoneLocation(TEXT("hand_r")) - Eye;
		const FVector FirstRifle = HeldWeapon->GetFirstPersonMesh()->GetComponentLocation() - Eye;
		const FVector Hand = GetMesh()->GetBoneLocation(TEXT("hand_r")) - Eye;
		const FVector Rifle = HeldWeapon->GetThirdPersonMesh()->GetComponentLocation() - Eye;
		// The rifle is modelled along its Y axis.
		const FVector FirstBarrel = HeldWeapon->GetFirstPersonMesh()->GetRightVector();
		const FVector Barrel = HeldWeapon->GetThirdPersonMesh()->GetRightVector();
		const FVector View = FirstPersonCameraComponent->GetForwardVector();
		const FVector Head = GetMesh()->GetBoneLocation(TEXT("head")) - GetMesh()->GetComponentLocation();
		const UCheeseStrikeAnimInstance* Animation = Cast<UCheeseStrikeAnimInstance>(GetMesh()->GetAnimInstance());
		// What the player's view is drawn from, which should be this character's camera.
		if (const APlayerController* Player = Cast<APlayerController>(GetController()))
		{
			if (Player->PlayerCameraManager != nullptr)
			{
				const FVector Seen = Player->PlayerCameraManager->GetCameraLocation();
				const FRotator Facing = Player->PlayerCameraManager->GetCameraRotation();
				UE_LOG(LogCheeseStrike, Display, TEXT("View: from (%.0f, %.0f, %.0f) facing (pitch %.0f, yaw %.0f) through %s"), Seen.X, Seen.Y, Seen.Z, Facing.Pitch, Facing.Yaw, *GetNameSafe(Player->GetViewTarget()));
			}
		}
		// Where the first-person rifle is in the body's right hand's space: how a body without weapon_r should hold it.
		const FTransform Wanted = HeldWeapon->GetFirstPersonMesh()->GetComponentTransform().GetRelativeTransform(GetMesh()->GetSocketTransform(TEXT("hand_r")));
		UE_LOG(LogCheeseStrike, Display, TEXT("Grip wanted on the body's hand_r: at (%.1f, %.1f, %.1f) turned (pitch %.1f, yaw %.1f, roll %.1f)"), Wanted.GetLocation().X, Wanted.GetLocation().Y, Wanted.GetLocation().Z, Wanted.Rotator().Pitch, Wanted.Rotator().Yaw, Wanted.Rotator().Roll);
		UE_LOG(LogCheeseStrike, Display, TEXT("Weapon in hand: %s"), *HeldWeapon->DescribeModel(Eye));
		// And where the left hand is on the rifle, in the rifle's own space: it should stay put whatever the aim.
		const FVector LeftOnRifle = LeftHandOnRifle();
		UE_LOG(LogCheeseStrike, Display, TEXT("Hands: first-person hand (%.0f, %.0f, %.0f) rifle (%.0f, %.0f, %.0f) barrel (%.2f, %.2f, %.2f) left hand on the rifle at (%.1f, %.1f, %.1f); body hand (%.0f, %.0f, %.0f) rifle (%.0f, %.0f, %.0f) barrel (%.2f, %.2f, %.2f) head (%.0f, %.0f, %.0f) over the feet; view (%.2f, %.2f, %.2f) aim pitch %.0f"),
			FirstHand.X, FirstHand.Y, FirstHand.Z, FirstRifle.X, FirstRifle.Y, FirstRifle.Z, FirstBarrel.X, FirstBarrel.Y, FirstBarrel.Z, LeftOnRifle.X, LeftOnRifle.Y, LeftOnRifle.Z,
			Hand.X, Hand.Y, Hand.Z, Rifle.X, Rifle.Y, Rifle.Z, Barrel.X, Barrel.Y, Barrel.Z, Head.X, Head.Y, Head.Z, View.X, View.Y, View.Z, Animation != nullptr ? Animation->AimPitch : 0.0f);
	}
	if (!IsAlive())
	{
		const FVector Pelvis = GetMesh()->GetBoneLocation(TEXT("pelvis"));
		const FVector Head = GetMesh()->GetBoneLocation(TEXT("head"));
		UE_LOG(LogCheeseStrike, Display, TEXT("Dead: pelvis (%.0f, %.0f, %.0f) head (%.0f, %.0f, %.0f) visible %d"), Pelvis.X, Pelvis.Y, Pelvis.Z, Head.X, Head.Y, Head.Z, GetMesh()->IsVisible() ? 1 : 0);
	}
}

void ACheeseStrikeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const UCheeseStrikeInputSet* Input = UCheeseStrikeInputSet::Get(this);
	if (EnhancedInputComponent == nullptr || Input == nullptr)
	{
		UE_LOG(LogCheeseStrike, Error, TEXT("'%s' has no Enhanced Input component or no input set to bind to."), *GetNameSafe(this));
		return;
	}
	EnhancedInputComponent->BindAction(Input->Move, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::MoveInput);
	EnhancedInputComponent->BindAction(Input->MouseLook, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::MouseLookInput);
	EnhancedInputComponent->BindAction(Input->StickLook, ETriggerEvent::Triggered, this, &ACheeseStrikeCharacter::StickLookInput);
	EnhancedInputComponent->BindAction(Input->Jump, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::DoJumpStart);
	EnhancedInputComponent->BindAction(Input->Jump, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::DoJumpEnd);
	EnhancedInputComponent->BindAction(Input->Sprint, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::SprintStarted);
	EnhancedInputComponent->BindAction(Input->Sprint, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::SprintEnded);
	EnhancedInputComponent->BindAction(Input->Crouch, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::CrouchStarted);
	EnhancedInputComponent->BindAction(Input->Crouch, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::CrouchEnded);
	EnhancedInputComponent->BindAction(Input->Fire, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::FireStarted);
	EnhancedInputComponent->BindAction(Input->Fire, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::FireEnded);
	EnhancedInputComponent->BindAction(Input->Aim, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::AimStarted);
	EnhancedInputComponent->BindAction(Input->Aim, ETriggerEvent::Completed, this, &ACheeseStrikeCharacter::AimEnded);
	EnhancedInputComponent->BindAction(Input->Reload, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::ReloadStarted);
	EnhancedInputComponent->BindAction(Input->DrawPrimary, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::PrimaryStarted);
	EnhancedInputComponent->BindAction(Input->DrawSidearm, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::SidearmStarted);
	EnhancedInputComponent->BindAction(Input->DrawKnife, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::KnifeStarted);
	EnhancedInputComponent->BindAction(Input->NextWeapon, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::NextWeaponStarted);
	EnhancedInputComponent->BindAction(Input->PreviousWeapon, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::PreviousWeaponStarted);
	EnhancedInputComponent->BindAction(Input->LastWeapon, ETriggerEvent::Started, this, &ACheeseStrikeCharacter::LastWeaponStarted);
}

void ACheeseStrikeCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoMove(Axis.X, Axis.Y);
}

void ACheeseStrikeCharacter::MouseLookInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	DoAim(Axis.X * CheeseStrikeInput::MouseDegreesPerCount, Axis.Y * CheeseStrikeInput::MouseDegreesPerCount);
}

void ACheeseStrikeCharacter::StickLookInput(const FInputActionValue& Value)
{
	// A stick turns at a rate, however many frames a second are drawn.
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Seconds = GetWorld() != nullptr ? GetWorld()->GetDeltaSeconds() : 0.0f;
	DoAim(Axis.X * CheeseStrikeInput::StickYawDegreesPerSecond * Seconds, Axis.Y * CheeseStrikeInput::StickPitchDegreesPerSecond * Seconds);
}

void ACheeseStrikeCharacter::DoAim(float Yaw, float Pitch)
{
	const ACheeseStrikePlayerController* Player = Cast<ACheeseStrikePlayerController>(GetController());
	if (GetController() && (Player == nullptr || !Player->IsPaused()))
	{
		// As fast as the player's options say, and up and down whichever way they say.
		const FVector2D Scale = Player != nullptr ? Player->LookScale() : FVector2D(1.0, 1.0);
		AddControllerYawInput(Yaw * Scale.X);
		AddControllerPitchInput(Pitch * Scale.Y);
	}
}

void ACheeseStrikeCharacter::DoMove(float Right, float Forward)
{
	if (GetController() && TakesInput())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ACheeseStrikeCharacter::DoJumpStart()
{
	if (TakesInput())
	{
		Jump();
	}
}

void ACheeseStrikeCharacter::DoJumpEnd()
{
	StopJumping();
}

void ACheeseStrikeCharacter::DoSprint(bool bHeld)
{
	if (UCheeseStrikeMovementComponent* Movement = GetCheeseStrikeMovement())
	{
		Movement->SetWantsToSprint(bHeld && TakesInput());
	}
}

void ACheeseStrikeCharacter::DoCrouch(bool bHeld)
{
	if (bHeld && TakesInput())
	{
		Crouch();
	}
	else
	{
		UnCrouch();
	}
}

void ACheeseStrikeCharacter::DoFire(bool bHeld)
{
	ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	if (HeldWeapon == nullptr)
	{
		return;
	}
	if (bHeld && TakesInput())
	{
		HeldWeapon->StartFiring();
	}
	else
	{
		HeldWeapon->StopFiring();
	}
}

void ACheeseStrikeCharacter::DoAimDownSights(bool bHeld)
{
	ACheeseStrikeWeapon* HeldWeapon = GetWeapon();
	if (HeldWeapon == nullptr)
	{
		return;
	}
	if (bHeld && TakesInput())
	{
		HeldWeapon->StartAiming();
	}
	else
	{
		HeldWeapon->StopAiming();
	}
}

void ACheeseStrikeCharacter::DoReload()
{
	if (GetWeapon() != nullptr && TakesInput())
	{
		GetWeapon()->StartReload();
	}
}

void ACheeseStrikeCharacter::DoDrawWeapon(EWeaponSlot Slot)
{
	if (TakesInput())
	{
		// The sights stay up through a change while they are held.
		const bool bAiming = GetWeapon() != nullptr && GetWeapon()->IsAimed();
		Inventory->Draw(Slot);
		if (bAiming && GetWeapon() != nullptr)
		{
			GetWeapon()->StartAiming();
		}
	}
}

void ACheeseStrikeCharacter::DoNextWeapon(int32 Step)
{
	if (TakesInput())
	{
		const bool bAiming = GetWeapon() != nullptr && GetWeapon()->IsAimed();
		Inventory->DrawNext(Step);
		if (bAiming && GetWeapon() != nullptr)
		{
			GetWeapon()->StartAiming();
		}
	}
}

void ACheeseStrikeCharacter::DoLastWeapon()
{
	if (TakesInput())
	{
		const bool bAiming = GetWeapon() != nullptr && GetWeapon()->IsAimed();
		Inventory->DrawLast();
		if (bAiming && GetWeapon() != nullptr)
		{
			GetWeapon()->StartAiming();
		}
	}
}

void ACheeseStrikeCharacter::PrimaryStarted()
{
	DoDrawWeapon(EWeaponSlot::Primary);
}

void ACheeseStrikeCharacter::SidearmStarted()
{
	DoDrawWeapon(EWeaponSlot::Sidearm);
}

void ACheeseStrikeCharacter::KnifeStarted()
{
	DoDrawWeapon(EWeaponSlot::Knife);
}

void ACheeseStrikeCharacter::NextWeaponStarted()
{
	DoNextWeapon(1);
}

void ACheeseStrikeCharacter::PreviousWeaponStarted()
{
	DoNextWeapon(-1);
}

void ACheeseStrikeCharacter::LastWeaponStarted()
{
	DoLastWeapon();
}

void ACheeseStrikeCharacter::FireStarted()
{
	DoFire(true);
}

void ACheeseStrikeCharacter::FireEnded()
{
	DoFire(false);
}

void ACheeseStrikeCharacter::AimStarted()
{
	DoAimDownSights(true);
}

void ACheeseStrikeCharacter::AimEnded()
{
	DoAimDownSights(false);
}

void ACheeseStrikeCharacter::ReloadStarted()
{
	DoReload();
}

void ACheeseStrikeCharacter::SprintStarted()
{
	DoSprint(true);
}

void ACheeseStrikeCharacter::SprintEnded()
{
	DoSprint(false);
}

void ACheeseStrikeCharacter::CrouchStarted()
{
	DoCrouch(true);
}

void ACheeseStrikeCharacter::CrouchEnded()
{
	DoCrouch(false);
}
