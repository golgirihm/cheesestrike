// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Weapons/CheeseStrikeShot.h"
#include "CheeseStrikeCharacter.generated.h"

class ACheeseStrikeWeapon;
class UCameraComponent;
class UCheeseStrikeMovementComponent;
class USkeletalMeshComponent;
struct FInputActionValue;

/**
 * The player's character, seen from its own eyes: the mannequin, with a
 * second copy of it that only its own player sees, whose head the camera
 * sits on. The template's animations have no crouch, so when the character
 * crouches the first-person copy, and the camera with it, come down
 * smoothly by what the capsule drops while the body everyone else sees
 * stays as it is; the Eyes component is where the server takes the head
 * to be. It walks, sprints,
 * crouches, slides and jumps as CheeseStrikeMovementComponent has it, with
 * every move predicted on its own machine and checked on the server.
 *
 * It holds a weapon (CheeseStrikeWeapon), has health that only the server
 * takes from, and keeps, on the server, where it has lately been, for shots
 * to be judged against where the shooter saw it (FHitHistory).
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
	USceneComponent* GetEyes() const { return Eyes; }
	/** How far above the capsule's centre the eyes are just now, as they move between standing and crouched. */
	float GetEyeOffset() const { return EyeOffset; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** Writes where the character is and what it is doing to the log, for looking at a run from outside (with -TraceState, ten times a second for the local player). */
	UFUNCTION(Exec)
	void CheeseStrikeState();

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

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoFire(bool bHeld);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAimDownSights(bool bHeld);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoReload();

	/** The weapon in hand, or null before one arrives. */
	ACheeseStrikeWeapon* GetWeapon() const { return HeldWeapon; }
	void SetWeapon(ACheeseStrikeWeapon* InWeapon) { HeldWeapon = InWeapon; }

	float GetHealth() const { return Health; }
	bool IsAlive() const { return Health > 0.0f; }
	/** Takes `Damage` off the health (server only); for the server's own records, from whom and whether on the head. */
	void TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead);

	/** Where this character has lately been, kept on the server. */
	const FHitHistory& GetHitHistory() const { return HitHistory; }
	/** Where it is right now, as a sample. */
	FHitHistory::FSample SampleNow(float Time) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void MoveInput(const FInputActionValue& Value);
	void MouseLookInput(const FInputActionValue& Value);
	void StickLookInput(const FInputActionValue& Value);
	void SprintStarted();
	void SprintEnded();
	void CrouchStarted();
	void CrouchEnded();
	void FireStarted();
	void FireEnded();
	void AimStarted();
	void AimEnded();
	void ReloadStarted();

	/** Pawn mesh: first person view (seen only by self). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	/** Where the eyes are: on the capsule, at eye height. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Eyes;

	/** First person camera, at the eyes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

	/** Where the eyes are above the capsule's centre now, on their way to BaseEyeHeight. */
	float EyeOffset = 0.0f;

	/** The weapon in hand: spawned by the server, which owns it, and attached on every machine. */
	UPROPERTY(Replicated)
	TObjectPtr<ACheeseStrikeWeapon> HeldWeapon;
	UPROPERTY(Replicated)
	float Health = 100.0f;
	/** Where it has lately been: the server's record. */
	FHitHistory HitHistory;
	/** The usual field of view, to come back to after aiming down the sights. */
	float BaseFieldOfView = 90.0f;
	/** Seconds since the state was last traced to the log. */
	float SinceTrace = 0.0f;
	/** Whether -StartAt has been applied. */
	bool bStartAtApplied = false;

	void ApplyStartAt();
	void StartAt(const FVector& At, float Yaw);
	UFUNCTION(Server, Reliable)
	void ServerStartAt(FVector_NetQuantize At, float Yaw);
	UFUNCTION(Client, Reliable)
	void ClientStartAt(FVector_NetQuantize At, float Yaw);
};
