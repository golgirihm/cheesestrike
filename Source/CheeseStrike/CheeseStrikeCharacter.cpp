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
#include "Input/CheeseStrikeInput.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Weapons/CheeseStrikeWeapon.h"

/** With this on, every character writes its state to the log four times a second, for watching a run from outside. */
static TAutoConsoleVariable<int32> CVarTraceState(TEXT("CheeseStrike.TraceState"), 0, TEXT("Log each character's state four times a second."));

ACheeseStrikeCharacter::ACheeseStrikeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCheeseStrikeMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	using namespace CheeseStrikeMovement;
	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	// The body everyone else sees, and the copy of it its own player sees,
	// each moved by the template's animation: the mannequin standing on the
	// capsule's floor, turned to face its front.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mannequin(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimation(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> ArmsAnimation(TEXT("/Game/FirstPerson/Anims/ABP_FP_Copy"));
	GetMesh()->SetSkeletalMeshAsset(Mannequin.Object);
	GetMesh()->SetAnimInstanceClass(BodyAnimation.Class);
	GetMesh()->SetRelativeLocationAndRotation(FVector(-10.0f, 0.0f, -CapsuleHalfHeight), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetSkeletalMeshAsset(Mannequin.Object);
	FirstPersonMesh->SetAnimInstanceClass(ArmsAnimation.Class);
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
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
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

void ACheeseStrikeCharacter::TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead)
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	if (!IsAlive())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s is killed by %s"), *GetName(), *GetNameSafe(From));
	}
}

void ACheeseStrikeCharacter::ApplyStartAt()
{
	// For watching a run from outside: -StartAt=X,Y,Z,Yaw puts the local
	// player there, facing that way, instead of wherever the level starts
	// them. Once, on the first tick it is possessed.
	FString Spec;
	if (!bStartAtApplied && IsLocallyControlled() && IsPlayerControlled() && GetController() != nullptr && FParse::Value(FCommandLine::Get(), TEXT("StartAt="), Spec, false))
	{
		bStartAtApplied = true;
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

void ACheeseStrikeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyStartAt();
	// The server keeps where everyone has been, for shots fired at where they were seen.
	if (HasAuthority() && GetWorld() != nullptr && GetWorld()->GetGameState() != nullptr)
	{
		HitHistory.Add(SampleNow(GetWorld()->GetGameState()->GetServerWorldTimeSeconds()));
	}
	// Aiming down the sights narrows the view, smoothly.
	const bool bAimed = HeldWeapon != nullptr && HeldWeapon->IsAimed();
	const float WantedFieldOfView = BaseFieldOfView * (bAimed ? CheeseStrikeWeapons::Blaster().AimZoom : 1.0f);
	FirstPersonCameraComponent->SetFieldOfView(FMath::FInterpTo(FirstPersonCameraComponent->FieldOfView, WantedFieldOfView, DeltaSeconds, 12.0f));
	// Traced with the console variable, or with -TraceState on the command line.
	static const bool bTraceSwitch = FParse::Param(FCommandLine::Get(), TEXT("TraceState"));
	if ((bTraceSwitch || CVarTraceState.GetValueOnGameThread() != 0) && GetWorld() != nullptr && IsLocallyControlled() && IsPlayerControlled())
	{
		SinceTrace += DeltaSeconds;
		if (SinceTrace >= 0.1f)
		{
			SinceTrace = 0.0f;
			CheeseStrikeState();
		}
	}
	// The eyes settle at their height for standing or crouched.
	EyeOffset = FMath::FInterpTo(EyeOffset, BaseEyeHeight, DeltaSeconds, CheeseStrikeMovement::CrouchRate);
	Eyes->SetRelativeLocation(FVector(0.0f, 0.0f, EyeOffset));
	// The first-person body goes down with the eyes, as the body it copies
	// has no crouch: its feet sink below the floor, which only its own
	// player could see, by looking down.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float EyesAboveFloor = EyeOffset + HalfHeight;
	FirstPersonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, EyesAboveFloor - (CheeseStrikeMovement::EyeHeight + CheeseStrikeMovement::CapsuleHalfHeight)));
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
	UE_LOG(LogCheeseStrike, Display, TEXT("State: at (%.0f, %.0f, %.0f) speed %.0f eye %.0f above the floor pitch %.1f yaw %.1f crouched %d sprinting %d sliding %d (%.2f s) falling %d jump %d/%d health %.0f ammo %d/%d aimed %d fov %.0f shots %d hits %d reloading %d"),
		At.X, At.Y, At.Z, GetVelocity().Size2D(), Eye.Z - (At.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), GetControlRotation().Pitch, GetControlRotation().Yaw,
		bIsCrouched ? 1 : 0, Movement != nullptr && Movement->IsSprinting() ? 1 : 0, Movement != nullptr && Movement->IsSliding() ? 1 : 0,
		Movement != nullptr ? Movement->GetSlideElapsed() : -1.0f, Movement != nullptr && Movement->IsFalling() ? 1 : 0, bPressedJump ? 1 : 0, JumpCurrentCount,
		Health, HeldWeapon != nullptr ? HeldWeapon->GetMagazine() : -1, HeldWeapon != nullptr ? HeldWeapon->GetReserve() : -1, HeldWeapon != nullptr && HeldWeapon->IsAimed() ? 1 : 0,
		FirstPersonCameraComponent->FieldOfView, HeldWeapon != nullptr ? HeldWeapon->ShotsFired : 0, HeldWeapon != nullptr ? HeldWeapon->HitsConfirmed : 0, HeldWeapon != nullptr && HeldWeapon->IsReloading() ? 1 : 0);
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
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ACheeseStrikeCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void ACheeseStrikeCharacter::DoJumpStart()
{
	Jump();
}

void ACheeseStrikeCharacter::DoJumpEnd()
{
	StopJumping();
}

void ACheeseStrikeCharacter::DoSprint(bool bHeld)
{
	if (UCheeseStrikeMovementComponent* Movement = GetCheeseStrikeMovement())
	{
		Movement->SetWantsToSprint(bHeld);
	}
}

void ACheeseStrikeCharacter::DoCrouch(bool bHeld)
{
	if (bHeld)
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
	if (bHeld)
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
	if (bHeld)
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
	if (HeldWeapon != nullptr)
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
