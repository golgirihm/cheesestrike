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
 * Killed, it falls where it is and its player looks on from where their
 * eyes were until the game mode gives them a new one; a new one can't be
 * hurt for its first seconds unless it fires (CheeseStrikeRules).
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
	virtual void Destroyed() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** Writes where the character is and what it is doing to the log, for looking at a run from outside (with -TraceState, ten times a second for the local player). */
	UFUNCTION(Exec)
	void CheeseStrikeState();
	/** Kills the character, from the console, for looking at a death without another player. */
	UFUNCTION(Exec)
	void CheeseStrikeDie();
	/**
	 * For looking at the body from outside with one player (a second game
	 * on the same PC renders poorly): puts the view `Distance` cm from the
	 * character, `Yaw` degrees round from in front, looking at the point
	 * `Height` cm above its middle (70 is the face), and shows the body
	 * and its rifle as others see them. With no distance, the view goes
	 * back to the eyes.
	 */
	UFUNCTION(Exec)
	void CheeseStrikeOutside(float Distance = 0.0f, float Yaw = 0.0f, float Height = 30.0f);
	/** For looking at a map (development): puts the view at X, Y, Z facing Pitch and Yaw, the character left where it is; CheeseStrikeOutside with no distance puts it back. */
	UFUNCTION(Exec)
	void CheeseStrikeViewFrom(float X, float Y, float Z, float Pitch, float Yaw);
	/** For looking at a mouse (development): puts the view `Distance` cm from the first one, `Yaw` degrees round from in front of it and `Height` cm up, following it; CheeseStrikeOutside with no distance puts it back. */
	UFUNCTION(Exec)
	void CheeseStrikeViewMouse(float Distance = 600.0f, float Yaw = 45.0f, float Height = 150.0f);

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

	/** Aiming: the rifle's sight line, across it from its bounds' middle and down from its top, and how far along it from its back end the eye is (cm). */
	static constexpr float SightSide = 0.0f;
	static constexpr float SightHeight = 2.0f;
	static constexpr float SightBack = 14.0f;

	/** The weapon in hand, or null before one arrives. */
	ACheeseStrikeWeapon* GetWeapon() const { return HeldWeapon; }
	void SetWeapon(ACheeseStrikeWeapon* InWeapon) { HeldWeapon = InWeapon; }

	float GetHealth() const { return Health; }
	bool IsAlive() const { return Health > 0.0f; }
	/** Whether it can do anything but look: alive, in a match still on. */
	bool CanAct() const;
	/** Whether its player's input reaches it: it can act, and its player hasn't paused (a paused player looks at the menu, and the match goes on without them). */
	bool TakesInput() const;
	/** Whether it has just come back and can't yet be hurt. */
	bool IsProtected() const;
	/** Server only: ends that, as firing does. */
	void EndProtection() { ProtectedUntil = 0.0f; }
	/** The player's name, as the feed and the scoreboard show it, or the actor's where there is no player. */
	FString DisplayName() const;
	/** Who killed it, once it is dead, and the server's time it died (-1 while alive). */
	const FString& GetKilledBy() const { return KilledBy; }
	float GetDiedAt() const { return DiedAt; }
	/**
	 * Takes `Damage` off the health (server only); for the server's own
	 * records, from whom and whether on the head. False if it couldn't be
	 * hurt: dead already, or protected.
	 */
	bool TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead);
	/** Server only: who last hurt it, and the server's time they did; a bot turns to whoever shoots it. */
	ACheeseStrikeCharacter* GetLastHurtBy() const { return LastHurtBy.Get(); }
	float GetLastHurtAt() const { return LastHurtAt; }

	/** The bones of a body's two eyes (eyeball_l and _r, or L_eye and R_eye), if it has them. */
	static bool FindEyes(const USkeletalMesh* Body, FName& Left, FName& Right);

	/** Where the first-person left hand is in the first-person rifle's own space, for the trace and the tests: it should stay put whatever the aim. */
	FVector LeftHandOnRifle() const;

	/** Puts it at `At`, facing `Yaw` (on the server), for -StartAt and for tests. */
	void StartAt(const FVector& At, float Yaw);

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
	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0f;
	/** Who killed it, and when, in the server's time; set by the server as it dies. */
	UPROPERTY(Replicated)
	FString KilledBy;
	UPROPERTY(Replicated)
	float DiedAt = -1.0f;
	/** The server's time its protection as a fresh spawn ends. */
	UPROPERTY(Replicated)
	float ProtectedUntil = 0.0f;
	/** Whether its death has been shown here. */
	bool bDeathShown = false;
	TWeakObjectPtr<ACheeseStrikeCharacter> LastHurtBy;
	float LastHurtAt = -1000.0f;
	/** Where it has lately been: the server's record. */
	FHitHistory HitHistory;
	/** The usual field of view, to come back to after aiming down the sights. */
	float BaseFieldOfView = 90.0f;
	/** How far the view has come from the head to the rifle's sights, 0 to 1. */
	float AimFraction = 0.0f;
	/** Where the eye sits to look through the sights, from the rifle's bounds: so far forward of its back end, so far above its top (the rifle points a little under the view, so the eye sits a little under the top), so far to the left of its middle. */
	/** The arms' place on the body when not aiming. */
	FTransform FirstPersonBase = FTransform::Identity;
	/** Aiming: the arms brought so that the rifle's sight line runs down the view through the eye (Tick). */
	void BringRifleToEye();
	/** Where the camera sits in the first-person body's head bone: just under its eyes (from the body's eye bones), or where the template had it on the mannequin. */
	FVector CameraAtHead = DefaultCameraAtHead;
	static const FVector DefaultCameraAtHead;
	/** The camera from the eyes, in the head's space: 8 cm down and 1 cm back, as the template's camera sits from Belica's eyes. */
	static const FVector CameraFromEyes;
	/** Puts the camera just under the body's eyes, and hides the head (face, hair, eyes) in the first-person copy, which the camera sits inside. */
	void FitFirstPersonBody();
	/** Seconds since the state was last traced to the log. */
	float SinceTrace = 0.0f;
	/** Whether -StartAt has been applied. */
	bool bStartAtApplied = false;

	UFUNCTION()
	void OnRep_Health();
	/** Server only: it has just run out of health. */
	void Die(ACheeseStrikeCharacter* From, bool bHead);
	/** On every machine: the body falls, the view stays where the eyes were, and nothing more is done. */
	void ShowDeath();
	/** The server's clock. */
	float ServerNow() const;

	void ApplyStartAt();
	UFUNCTION(Server, Reliable)
	void ServerDie();
	UFUNCTION(Server, Reliable)
	void ServerStartAt(FVector_NetQuantize At, float Yaw);
	UFUNCTION(Client, Reliable)
	void ClientStartAt(FVector_NetQuantize At, float Yaw);

	/** The camera of CheeseStrikeOutside, while it is in use. */
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> OutsideCamera;
};
