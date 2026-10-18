// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CheeseStrikeCharacter.generated.h"

class UCameraComponent;
class UCheeseStrikeMovementComponent;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * The player's character, seen from its own eyes: the mannequin, with a
 * second copy of it that only its own player sees, whose head the camera
 * sits on. It walks, sprints, crouches, slides and jumps as
 * CheeseStrikeMovementComponent has it, with every move predicted on its own
 * machine and checked on the server.
 *
 * Input comes from the actions the game makes in code (UCheeseStrikeInputSet),
 * not from assets.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACheeseStrikeCharacter(const FObjectInitializer& ObjectInitializer);

	UCheeseStrikeMovementComponent* GetCheeseStrikeMovement() const;
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** Handles aim inputs from either controls or UI interfaces: degrees to turn by. */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSprint(bool bHeld);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoCrouch(bool bHeld);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void MoveInput(const FInputActionValue& Value);
	void MouseLookInput(const FInputActionValue& Value);
	void StickLookInput(const FInputActionValue& Value);
	void SprintStarted();
	void SprintEnded();
	void CrouchStarted();
	void CrouchEnded();

	/** Pawn mesh: first person view (seen only by self). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** First person camera, on the first-person mesh's head. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;
};
