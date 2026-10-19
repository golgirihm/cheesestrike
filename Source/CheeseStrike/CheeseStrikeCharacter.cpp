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

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(Eyes);
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;
}

UCheeseStrikeMovementComponent* ACheeseStrikeCharacter::GetCheeseStrikeMovement() const
{
	return Cast<UCheeseStrikeMovementComponent>(GetCharacterMovement());
}

void ACheeseStrikeCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACheeseStrikeCharacter::ApplyStartAt()
{
	// For watching a run from outside: -StartAt=X,Y,Z,Yaw puts the local
	// player there, facing that way, instead of wherever the level starts
	// them. Once, on the first tick it is possessed.
	FString StartAt;
	if (!bStartAtApplied && IsLocallyControlled() && IsPlayerControlled() && GetController() != nullptr && FParse::Value(FCommandLine::Get(), TEXT("StartAt="), StartAt, false))
	{
		bStartAtApplied = true;
		TArray<FString> Parts;
		StartAt.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 3)
		{
			SetActorLocation(FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])), false, nullptr, ETeleportType::TeleportPhysics);
			if (Parts.Num() >= 4 && GetController() != nullptr)
			{
				GetController()->SetControlRotation(FRotator(0.0f, FCString::Atof(*Parts[3]), 0.0f));
			}
		}
	}
}

void ACheeseStrikeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyStartAt();
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
	UE_LOG(LogCheeseStrike, Display, TEXT("State: at (%.0f, %.0f, %.0f) speed %.0f eye %.0f above the floor pitch %.1f crouched %d sprinting %d sliding %d (%.2f s) falling %d jump %d/%d"),
		At.X, At.Y, At.Z, GetVelocity().Size2D(), Eye.Z - (At.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), GetControlRotation().Pitch,
		bIsCrouched ? 1 : 0, Movement != nullptr && Movement->IsSprinting() ? 1 : 0, Movement != nullptr && Movement->IsSliding() ? 1 : 0,
		Movement != nullptr ? Movement->GetSlideElapsed() : -1.0f, Movement != nullptr && Movement->IsFalling() ? 1 : 0, bPressedJump ? 1 : 0, JumpCurrentCount);
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
