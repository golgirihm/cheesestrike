// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

// The character as it is made: its movement component and numbers, its
// meshes and camera, and that the game gives every player one.

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameModeBase.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikeGameMode.h"
#include "CheeseStrikePlayerController.h"
#include "Movement/CheeseStrikeMovement.h"
#include "Movement/CheeseStrikeMovementComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CharacterTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}
using namespace CharacterTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterMovesByTheRules, "CheeseStrike.Character.MovesByTheRules", Flags)

bool FTheCharacterMovesByTheRules::RunTest(const FString& Parameters)
{
	using namespace CheeseStrikeMovement;
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	const UCheeseStrikeMovementComponent* Movement = Character->GetCheeseStrikeMovement();
	if (!TestNotNull(TEXT("it moves by the game's own component"), Movement))
	{
		return false;
	}
	TestEqual(TEXT("walking"), Movement->MaxWalkSpeed, WalkSpeed);
	TestEqual(TEXT("crouched"), Movement->MaxWalkSpeedCrouched, WalkSpeed * CrouchFactor);
	TestEqual(TEXT("jumping"), Movement->JumpZVelocity, JumpSpeed);
	TestEqual(TEXT("under the usual gravity"), Movement->GravityScale, 1.0f);
	TestEqual(TEXT("steering in the air"), Movement->AirControl, CheeseStrikeMovement::AirControl());
	TestEqual(TEXT("and slowing in it no faster"), Movement->BrakingDecelerationFalling, AirSteer);
	TestEqual(TEXT("stepping"), Movement->MaxStepHeight, StepHeight);
	TestEqual(TEXT("crouching"), Movement->GetCrouchedHalfHeight(), CrouchedHalfHeight);
	TestTrue(TEXT("is allowed"), Movement->NavAgentProps.bCanCrouch);
	TestEqual(TEXT("the capsule"), Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), CapsuleHalfHeight);
	TestEqual(TEXT("its radius"), Character->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), CapsuleRadius);

	// Sprinting and sliding start off.
	TestFalse(TEXT("not sprinting"), Movement->WantsToSprint());
	TestFalse(TEXT("not sliding"), Movement->IsSliding());
	TestEqual(TEXT("walking as the rules have it"), Movement->MaxWalkSpeed, Speed(false, false, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheCharacterIsSeenFromItsEyes, "CheeseStrike.Character.IsSeenFromItsEyes", Flags)

bool FTheCharacterIsSeenFromItsEyes::RunTest(const FString& Parameters)
{
	const ACheeseStrikeCharacter* Character = GetDefault<ACheeseStrikeCharacter>();
	const USkeletalMeshComponent* Body = Character->GetMesh();
	const USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
	const UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
	if (!TestNotNull(TEXT("a body"), Body) || !TestNotNull(TEXT("a first-person body"), Arms) || !TestNotNull(TEXT("a camera"), Camera))
	{
		return false;
	}
	TestNotNull(TEXT("the body is the mannequin"), Body->GetSkeletalMeshAsset());
	TestNotNull(TEXT("and is animated"), Body->AnimClass.Get());
	TestTrue(TEXT("which its own player doesn't see"), Body->bOwnerNoSee);
	TestNotNull(TEXT("the first-person body is the mannequin"), Arms->GetSkeletalMeshAsset());
	TestNotNull(TEXT("and is animated"), Arms->AnimClass.Get());
	TestTrue(TEXT("which only its own player sees"), Arms->bOnlyOwnerSee);
	TestTrue(TEXT("the camera is at the eyes"), Camera->GetAttachParent() == Character->GetEyes());
	TestTrue(TEXT("which stand on the capsule"), Character->GetEyes()->GetAttachParent() == Character->GetCapsuleComponent());
	TestEqual(TEXT("at eye height"), static_cast<float>(Character->GetEyes()->GetRelativeLocation().Z), CheeseStrikeMovement::EyeHeight);
	TestEqual(TEXT("which the engine knows as the base eye height"), Character->BaseEyeHeight, CheeseStrikeMovement::EyeHeight);
	TestEqual(TEXT("and lowers when crouched"), Character->CrouchedEyeHeight, CheeseStrikeMovement::CrouchedEyeHeightAboveCentre);
	TestTrue(TEXT("by as much as the capsule drops"), CheeseStrikeMovement::CapsuleHalfHeight - CheeseStrikeMovement::CrouchedHalfHeight > 40.0f);
	TestTrue(TEXT("and turns with the player"), Camera->bUsePawnControlRotation);
	TestEqual(TEXT("the body stands on the capsule's floor"), static_cast<float>(Body->GetRelativeLocation().Z), -CheeseStrikeMovement::CapsuleHalfHeight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEveryPlayerGetsTheCharacter, "CheeseStrike.Character.EveryPlayerGetsTheCharacter", Flags)

bool FEveryPlayerGetsTheCharacter::RunTest(const FString& Parameters)
{
	const AGameModeBase* Mode = GetDefault<ACheeseStrikeGameMode>();
	TestEqual(TEXT("the pawn"), Mode->DefaultPawnClass.Get(), static_cast<UClass*>(ACheeseStrikeCharacter::StaticClass()));
	TestEqual(TEXT("the controller"), Mode->PlayerControllerClass.Get(), static_cast<UClass*>(ACheeseStrikePlayerController::StaticClass()));
	TestFalse(TEXT("the character can be made"), ACheeseStrikeCharacter::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("so can the controller"), ACheeseStrikePlayerController::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestFalse(TEXT("and the mode"), ACheeseStrikeGameMode::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	return true;
}

#endif
