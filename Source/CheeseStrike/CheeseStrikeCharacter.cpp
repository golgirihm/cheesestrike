// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#include "CheeseStrikeCharacter.h"
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
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikePlayerController.h"
#include "Rules/CheeseStrikeRules.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** With this on, every character writes its state to the log four times a second, for watching a run from outside. */
static TAutoConsoleVariable<int32> CVarTraceState(TEXT("CheeseStrike.TraceState"), 0, TEXT("Log each character's state four times a second."));

const FVector ACheeseStrikeCharacter::CameraAtHead(-2.8f, 5.89f, 0.0f);

ACheeseStrikeCharacter::ACheeseStrikeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCheeseStrikeMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	using namespace CheeseStrikeMovement;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The body everyone else sees, and the copy of it its own player sees,
	// each animated in code (UCheeseStrikeAnimInstance) with Lyra's rifle
	// set: the mannequin standing on the capsule's floor, turned to face
	// its front.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mannequin(TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"));
	GetMesh()->SetSkeletalMeshAsset(Mannequin.Object);
	GetMesh()->SetAnimInstanceClass(UCheeseStrikeAnimInstance::StaticClass());
	GetMesh()->SetRelativeLocationAndRotation(FVector(-10.0f, 0.0f, -CapsuleHalfHeight), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetSkeletalMeshAsset(Mannequin.Object);
	FirstPersonMesh->SetAnimInstanceClass(UCheeseStrikeAnimInstance::StaticClass());
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

	// The camera sits on the first-person body's head, as the template's
	// first-person rig expects (its arms are placed for a camera there);
	// crouching lowers that body, and the camera with it.
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
}

void ACheeseStrikeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheeseStrikeCharacter, HeldWeapon);
	DOREPLIFETIME(ACheeseStrikeCharacter, Health);
	DOREPLIFETIME(ACheeseStrikeCharacter, KilledBy);
	DOREPLIFETIME(ACheeseStrikeCharacter, DiedAt);
	DOREPLIFETIME(ACheeseStrikeCharacter, ProtectedUntil);
}

UCheeseStrikeMovementComponent* ACheeseStrikeCharacter::GetCheeseStrikeMovement() const
{
	return Cast<UCheeseStrikeMovementComponent>(GetCharacterMovement());
}

void ACheeseStrikeCharacter::BeginPlay()
{
	Super::BeginPlay();
	// The server gives every character its weapon; it arrives on every machine as an actor of this character's.
	if (HasAuthority() && HeldWeapon == nullptr && GetWorld() != nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		Parameters.Instigator = this;
		HeldWeapon = GetWorld()->SpawnActor<ACheeseStrikeWeapon>(ACheeseStrikeWeapon::StaticClass(), GetActorTransform(), Parameters);
	}
	// A fresh character can't be hurt for its first seconds, unless it fires.
	if (HasAuthority())
	{
		ProtectedUntil = ServerNow() + CheeseStrikeRules::SpawnProtection;
	}
}

void ACheeseStrikeCharacter::Destroyed()
{
	if (HasAuthority() && HeldWeapon != nullptr)
	{
		HeldWeapon->Destroy();
	}
	Super::Destroyed();
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
	return Sample;
}

bool ACheeseStrikeCharacter::TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead)
{
	if (!HasAuthority() || !CanAct() || IsProtected())
	{
		return false;
	}
	Health = FMath::Max(0.0f, Health - Damage);
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
	if (HeldWeapon != nullptr)
	{
		HeldWeapon->StopFiring();
		HeldWeapon->StopAiming();
	}
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
	const USkeletalMeshComponent* Rifle = HeldWeapon != nullptr ? HeldWeapon->GetFirstPersonMesh() : nullptr;
	if (Rifle == nullptr || FirstPersonMesh == nullptr)
	{
		return FVector::ZeroVector;
	}
	return Rifle->GetComponentTransform().InverseTransformPosition(FirstPersonMesh->GetBoneLocation(TEXT("hand_l")));
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
	// Aiming down the sights narrows the view, smoothly, and brings the eye
	// from the head to the rifle's sight line: a point behind the rifle's
	// back end and just above its top, found in the head's own space so
	// that the camera, which rides the head, lands there.
	const bool bAimed = HeldWeapon != nullptr && HeldWeapon->IsAimed();
	const float WantedFieldOfView = BaseFieldOfView * (bAimed ? CheeseStrikeWeapons::Blaster().AimZoom : 1.0f);
	FirstPersonCameraComponent->SetFieldOfView(FMath::FInterpTo(FirstPersonCameraComponent->FieldOfView, WantedFieldOfView, DeltaSeconds, 12.0f));
	AimFraction = FMath::FInterpTo(AimFraction, bAimed ? 1.0f : 0.0f, DeltaSeconds, 12.0f);
	const USkeletalMeshComponent* Rifle = HeldWeapon != nullptr ? HeldWeapon->GetFirstPersonMesh() : nullptr;
	if (IsAlive() && Rifle != nullptr && Rifle->GetSkeletalMeshAsset() != nullptr && IsLocallyControlled())
	{
		const FBoxSphereBounds Bounds = Rifle->GetSkeletalMeshAsset()->GetBounds();
		const FVector SightOnRifle(Bounds.Origin.X + SightEyeSide, Bounds.Origin.Y - Bounds.BoxExtent.Y + SightEyeDistance, Bounds.Origin.Z + Bounds.BoxExtent.Z + SightEyeHeight);
		const FVector SightInWorld = Rifle->GetComponentTransform().TransformPosition(SightOnRifle);
		const FVector SightAtHead = FirstPersonMesh->GetSocketTransform(FName("head")).InverseTransformPosition(SightInWorld);
		FirstPersonCameraComponent->SetRelativeLocation(FMath::Lerp(CameraAtHead, SightAtHead, AimFraction));
	}
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
	UE_LOG(LogCheeseStrike, Display, TEXT("State: at (%.0f, %.0f, %.0f) speed %.0f eye %.0f above the floor pitch %.1f yaw %.1f crouched %d sprinting %d sliding %d (%.2f s) falling %d jump %d/%d health %.0f alive %d protected %d ammo %d/%d aimed %d fov %.0f shots %d hits %d reloading %d body (%.0f, %.0f, %.0f) fallen %d camera (%.0f, %.0f, %.0f)"),
		At.X, At.Y, At.Z, GetVelocity().Size2D(), Eye.Z - (At.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::NormalizeAxis(GetControlRotation().Pitch), FRotator::NormalizeAxis(GetControlRotation().Yaw),
		bIsCrouched ? 1 : 0, Movement != nullptr && Movement->IsSprinting() ? 1 : 0, Movement != nullptr && Movement->IsSliding() ? 1 : 0,
		Movement != nullptr ? Movement->GetSlideElapsed() : -1.0f, Movement != nullptr && Movement->IsFalling() ? 1 : 0, bPressedJump ? 1 : 0, JumpCurrentCount,
		Health, IsAlive() ? 1 : 0, IsProtected() ? 1 : 0, HeldWeapon != nullptr ? HeldWeapon->GetMagazine() : -1, HeldWeapon != nullptr ? HeldWeapon->GetReserve() : -1, HeldWeapon != nullptr && HeldWeapon->IsAimed() ? 1 : 0,
		FirstPersonCameraComponent->FieldOfView, HeldWeapon != nullptr ? HeldWeapon->ShotsFired : 0, HeldWeapon != nullptr ? HeldWeapon->HitsConfirmed : 0, HeldWeapon != nullptr && HeldWeapon->IsReloading() ? 1 : 0,
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
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
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
	if (HeldWeapon != nullptr && TakesInput())
	{
		HeldWeapon->StartReload();
	}
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
