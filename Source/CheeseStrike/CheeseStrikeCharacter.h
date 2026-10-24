// Copyright (c) 2026 Hamid Golgiri. All rights reserved.
// Begun from Epic Games' First Person template (Copyright Epic Games, Inc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Weapons/CheeseStrikeShot.h"
#include "Rules/CheeseStrikeDowned.h"
#include "Input/CheeseStrikeAimAssist.h"
#include "CheeseStrikeCharacter.generated.h"

class ACheeseStrikeGameState;
class ACheeseStrikePlayerState;
class ACheeseStrikeWeapon;
class UCameraComponent;
class UMaterialInterface;
class UCheeseStrikeGrenades;
class UCheeseStrikeInventory;
class UCheeseStrikeLastStand;
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
 * It carries weapons (UCheeseStrikeInventory: a primary, the sidearm and
 * the knife, one in hand, CheeseStrikeWeapon), has health that only the server
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
	virtual void PossessedBy(AController* NewController) override;
	/** Under the map's kill height (into its fondue, off its edge): killed by the fall (server only; CheeseStrikeFalls). */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	/** Lands: a long fall hurts (server only; CheeseStrikeFalls::DamageFor), by the speed it comes down at. */
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	/** Writes where the character is and what it is doing to the log, for looking at a run from outside (with -TraceState, ten times a second for the local player). */
	UFUNCTION(Exec)
	void CheeseStrikeState();
	/** Kills the character, from the console, for looking at a death without another player. */
	UFUNCTION(Exec)
	void CheeseStrikeDie();
	/** Protects the character for `Seconds` from now, from the console, for looking at its ghostly look (it ends as usual when it attacks). */
	UFUNCTION(Exec)
	void CheeseStrikeProtect(float Seconds = 30.0f);
	/**
	 * For looking at the body from outside with one player (a second game
	 * on the same PC renders poorly): puts the view `Distance` cm from the
	 * character, `Yaw` degrees round from in front, looking at the point
	 * `Height` cm above its middle (70 is the face) from `Rise` cm above
	 * that, and shows the body and its rifle as others see them; once
	 * dead, round the head of the body where it fell (a large Rise looks
	 * down on it). With no distance, the view goes back to the eyes.
	 */
	UFUNCTION(Exec)
	void CheeseStrikeOutside(float Distance = 0.0f, float Yaw = 0.0f, float Height = 30.0f, float Rise = 20.0f);
	/** Shows its player the body and its weapons as others see them, the first-person body hidden (`bOutside`), or as in play: for a view from outside it (an execution's, CheeseStrikeOutside's). */
	void SeeOwnBody(bool bOutside);
	/** For looking at a map (development): puts the view at X, Y, Z facing Pitch and Yaw, the character left where it is; CheeseStrikeOutside with no distance puts it back. */
	UFUNCTION(Exec)
	void CheeseStrikeViewFrom(float X, float Y, float Z, float Pitch, float Yaw);
	/** For looking at a mouse (development): puts the view `Distance` cm from the first one, `Yaw` degrees round from in front of it and `Height` cm up, following it; CheeseStrikeOutside with no distance puts it back. */
	UFUNCTION(Exec)
	void CheeseStrikeViewMouse(float Distance = 600.0f, float Yaw = 45.0f, float Height = 150.0f);
	/** For looking at the bodies (development): this machine shows the character as body `Index` of CheeseStrikeBodies::All, in side `Team`'s colours (-1 for its own); with no index, as its player's side has it again. */
	UFUNCTION(Exec)
	void CheeseStrikeBody(int32 Index = -1, int32 Team = 0);

	/** Which of CheeseStrikeBodies::All it has here, and in which side's colours (INDEX_NONE for its own); INDEX_NONE until it has one. */
	int32 GetBody() const { return BodyIndex; }
	int32 GetBodyTeam() const { return BodyTeam; }

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

	/** Changing weapons: to the one in a slot, to the next (`Step` 1) or the one before (-1), or back to the last. */
	void DoDrawWeapon(EWeaponSlot Slot);
	void DoNextWeapon(int32 Step);
	void DoLastWeapon();
	/** Throws a cheese grenade along the view, if it has one and its hands are free (UCheeseStrikeGrenades). */
	void DoThrowGrenade();

	/** Aiming: the rifle's sight line, across it from its bounds' middle and down from its top, and how far along it from its back end the eye is (cm). */
	static constexpr float SightSide = 0.0f;
	static constexpr float SightHeight = 2.0f;
	static constexpr float SightBack = 14.0f;
	/** How far the view has come down the sights, 0 to 1; from ScopeFrom, a scoped weapon is looked through (its model hidden, the HUD's scope drawn). */
	float GetAimFraction() const { return AimFraction; }
	static constexpr float ScopeFrom = 0.9f;
	/** Where the eye goes to aim down the weapon in hand, in its model's own space: its look's, or for the rifle from its bounds by the sight variables. */
	bool WeaponSight(FVector& OutSight) const;
	/** Where the camera sits in the first-person body's head bone (FitFirstPersonBody). */
	FVector GetCameraAtHead() const { return CameraAtHead; }
	/**
	 * Down, the view keeps out of whatever is round the head (a ball this big,
	 * cm, swept from the capsule's middle) and this far over the floor or
	 * whatever is under it (a slope, a step, a wall it lies against), however
	 * the head is turned with the aim.
	 */
	static constexpr float DownViewRoom = 8.0f;
	static constexpr float DownViewHeight = 22.0f;
	/** How far its own player's view is rolled (degrees): leaning into a slide. */
	float GetViewRoll() const { return ViewRoll; }

	/** The weapon in hand, or null before one arrives. */
	ACheeseStrikeWeapon* GetWeapon() const;
	/** What it carries: the weapons in their slots, and which is in hand. */
	UCheeseStrikeInventory* GetInventory() const { return Inventory; }
	/** Its cheese grenades: a count of their own, apart from the weapons' slots. */
	UCheeseStrikeGrenades* GetGrenades() const { return Grenades; }
	/** Its last stand, in Execution: whether it is down, and its player's Interact. */
	UCheeseStrikeLastStand* GetLastStand() const { return LastStand; }
	/** Whether it is down in a last stand: on its back (crouched), with only its sidearm, finished only up close. */
	bool IsDowned() const;
	/** Whether it is in an execution still playing (UCheeseStrikeLastStand), dealing it or dealt it: it neither moves nor fires, and can't be hurt. */
	bool InExecution() const;
	/**
	 * How far down it is, 0 standing to 1 lying, at an even pace as it goes
	 * down (CheeseStrikeDowned::Seconds) and gets up (RiseSeconds), on every
	 * machine: the eyes, the shots' shape of it and the first-person view follow it.
	 */
	float GetDownBlend() const { return DownBlend; }
	/** Down: how much room there is ahead of its hips for its legs (cm; CheeseStrikeDowned::LegLength or more for all they need), as last measured, and measured now. */
	float GetDownLegRoom() const { return DownLegRoom; }
	float MeasureLegRoom() const;
	/** Down: where it aims, in degrees to the right of the way its body faces (within CheeseStrikeDowned::ConeYaw), on every machine. */
	float GetDownAimYaw() const;
	/** Down: where its body lies (CheeseStrikeDowned::Shape). */
	CheeseStrikeDowned::FShape GetDownShape() const;
	/** A point in the world in its body's frame (CheeseStrikeDowned's: ahead, right, up from the capsule's axis on the floor), for looking at the pose and testing it. */
	FVector DownBodyFrame(const FVector& World) const;
	/** Where its chest is, standing or lying, for aiming at it. */
	FVector ChestAt() const;
	/** Server: as it goes down, it falls back from whatever is just in front of it, as far as there is room behind, so that its legs have room. */
	void MakeRoomToLieDown();
	/**
	 * Down, the body faces where it does and turns round after the view only
	 * slowly (CheeseStrikeDowned::TurnBody), and its own machine keeps the
	 * view to the cone in front of it; getting up, it turns to the view; else
	 * as usual.
	 */
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.0f) override;
	/** Server only: sets the health, as a revive does. */
	void SetHealth(float InHealth);

	float GetHealth() const { return Health; }
	bool IsAlive() const { return Health > 0.0f; }
	/** Whether it can do anything but look: alive, in a match still on, and not held by the count before it or its round. */
	bool CanAct() const;
	/** Whether the count before the match or its round holds it where it stands (ACheeseStrikeGameState::IsFrozen). */
	bool IsFrozen() const;
	/** Whether its player's input reaches it: it can act, it isn't in an execution (either side of it), and its player hasn't paused (a paused player looks at the menu, and the match goes on without them). */
	bool TakesInput() const;
	/** Whether it has just come back and can't yet be hurt (CheeseStrikeStart: a few seconds from the count's end, or until it attacks). */
	bool IsProtected() const;
	/** Server only: ends that, as firing, striking with the knife and calling in a streak do; its ghostly look fades from now on every machine. */
	void EndProtection();
	/**
	 * How ghostly it looks on this machine (0 to 1, CheeseStrikeStart::GhostAmount):
	 * while protected its body, its first-person arms and its weapons wear
	 * M_Ghost as an overlay (pale, translucent, with a shimmer and a bright
	 * rim), fading as the protection ends.
	 */
	float GetGhostShown() const { return GhostShown; }
	/** The ghost's overlay materials, once made here: for the body others see, and for the first-person arms and weapon (a softer rim, seen up close). */
	class UMaterialInstanceDynamic* GetGhostMaterial() const { return GhostMaterial; }
	class UMaterialInstanceDynamic* GetGhostViewMaterial() const { return GhostViewMaterial; }
	/** The player's name, as the feed and the scoreboard show it, or the actor's where there is no player. */
	FString DisplayName() const;
	/** Who killed it, once it is dead, and the server's time it died (-1 while alive). */
	const FString& GetKilledBy() const { return KilledBy; }
	/** How it died when nobody killed it ("fell into the fondue"), for the death screen; empty otherwise. */
	const FString& GetDiedHow() const { return DiedHow; }
	float GetDiedAt() const { return DiedAt; }
	/**
	 * Takes `Damage` off the health (server only); for the server's own
	 * records, from whom and whether on the head. False if it couldn't be
	 * hurt: dead already, protected, or on the shooter's side in a team mode
	 * (friendly fire is off), or between rounds.
	 */
	bool TakeShot(float Damage, ACheeseStrikeCharacter* From, bool bHead);
	/**
	 * Takes `Damage` off the health from a streak (server only): `By`
	 * called it in, and has the kill even if their character has since
	 * died; `How` names it for the feed. False if it couldn't be hurt, as
	 * for TakeShot, or isn't `By`'s enemy.
	 */
	bool TakeBlast(float Damage, ACheeseStrikePlayerState* By, const FString& How);
	/**
	 * Takes `Damage` off the health from a fall (server only). A kill goes
	 * to whoever hurt it in the seconds before (CheeseStrikeFalls::CreditFor),
	 * as a fall into the fondue does, else the feed says it fell; in
	 * Execution a fall that would kill puts it down, as any hit would. Every
	 * machine hears it land hard. False if it couldn't be hurt.
	 */
	bool TakeFall(float Damage);
	/**
	 * Takes `Damage` off the health from its own grenade (server only), as
	 * a fall does: a kill goes to whoever hurt it in the seconds before
	 * (CheeseStrikeFalls::CreditFor), else the feed says how it died (`How`,
	 * "blew themself up") and nobody scores; in Execution a blast that would
	 * kill puts it down. False if it couldn't be hurt.
	 */
	bool TakeOwnBlast(float Damage, const FString& How);
	/** Server only: who last hurt it, and the server's time they did; a bot turns to whoever shoots it. */
	ACheeseStrikeCharacter* GetLastHurtBy() const { return LastHurtBy.Get(); }
	float GetLastHurtAt() const { return LastHurtAt; }

	/**
	 * The body `State`'s player has in `Match` (CheeseStrikeBodies::For, by their rank by player id on their side, or in
	 * the match without sides), and in `OutTeam` the side whose colours it wears (INDEX_NONE for its own): what their
	 * character puts on, for anything else that shows them (the podium).
	 */
	static int32 BodyOf(const ACheeseStrikeGameState* Match, const ACheeseStrikePlayerState* State, int32& OutTeam);

	/** The bones of a body's two eyes (eyeball_l and _r, or L_eye and R_eye), if it has them. */
	static bool FindEyes(const USkeletalMesh* Body, FName& Left, FName& Right);

	/** Where the first-person left hand is in the first-person rifle's own space, for the trace and the tests: it should stay put whatever the aim. */
	FVector LeftHandOnRifle() const;
	/** Its own player's view: where the weapon in hand is in it this frame, as the camera has it (ACheeseStrikeCameraManager), for how steadily it is held. */
	void NoteWeaponInView(const FTransform& InView);
	/**
	 * How much the weapon has trembled in view since this was last asked: the
	 * mean change, frame to frame, of how it moves (cm) and turns (degrees)
	 * from one frame to the next. Steady movement (the walk's bob, a turn)
	 * adds little; a shake adds much.
	 */
	FVector2D TakeViewTremble();

	/** Puts it at `At`, facing `Yaw` (on the server), for -StartAt and for tests. */
	void StartAt(const FVector& At, float Yaw);

	/** Where this character has lately been, kept on the server. */
	const FHitHistory& GetHitHistory() const { return HitHistory; }
	/** Where it is right now, as a sample. */
	FHitHistory::FSample SampleNow(float Time) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** What the aim assist did on the last frame the look stick turned the view (CheeseStrikeAimAssist), for the tests. */
	const CheeseStrikeAimAssist::FAssist& GetAimAssist() const { return AimAssist; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void MoveInput(const FInputActionValue& Value);
	void MouseLookInput(const FInputActionValue& Value);
	void StickLookInput(const FInputActionValue& Value);
	void SprintStarted();
	void SprintEnded();
	void CrouchStarted();
	void CrouchEnded();
	/** A gamepad's: pressing the left stick sprints until the stick is let go (MoveEnded), the player aims or fires, or it is pressed again; B crouches or stands. */
	void SprintToggled();
	void CrouchToggled();
	void MoveEnded();
	void FireStarted();
	void FireEnded();
	void AimStarted();
	void AimEnded();
	void ReloadStarted();
	void PrimaryStarted();
	void SidearmStarted();
	void KnifeStarted();
	void NextWeaponStarted();
	void PreviousWeaponStarted();
	void LastWeaponStarted();
	/** The Grenade key (G): throws one. */
	void GrenadeStarted();
	/** The Streak key: its player calls in their next streak (ACheeseStrikePlayerState::StreakPressed). */
	void StreakStarted();
	/** Interact (E), held and let go: reviving or finishing someone down (UCheeseStrikeLastStand); pressed, taking the weapon on a stand in reach (UCheeseStrikeInventory::PressForPickup). */
	void InteractStarted();
	void InteractEnded();

	/** How long the look stick has been held at its edge across, for the turn's boost (CheeseStrikeInput::StickTurn). */
	float StickEdgeSeconds = 0.0f;
	/** Whether a gamepad's sprint is on until the stick is let go (SprintToggled). */
	bool bSprintLatched = false;
	CheeseStrikeAimAssist::FAssist AimAssist;

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

	/** The weapons it carries: spawned by the server, which owns them, and attached on every machine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCheeseStrikeInventory> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCheeseStrikeLastStand> LastStand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCheeseStrikeGrenades> Grenades;
	UPROPERTY(ReplicatedUsing=OnRep_Health)
	float Health = 100.0f;
	/** Who killed it, and when, in the server's time; set by the server as it dies. */
	UPROPERTY(Replicated)
	FString KilledBy;
	UPROPERTY(Replicated)
	FString DiedHow;
	UPROPERTY(Replicated)
	float DiedAt = -1.0f;
	/** The server's time its protection as a fresh spawn ends. */
	UPROPERTY(Replicated)
	float ProtectedUntil = 0.0f;
	/** The ghost's overlay, made on first need, and how ghostly it was last shown. */
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> GhostMaterial;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> GhostViewMaterial;
	float GhostShown = 0.0f;
	/** Puts on or takes off the ghostly look, as the protection says. */
	void UpdateGhost();
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
	/** Whether the view is through a scope now, the first-person body hidden. */
	bool bLookingThroughScope = false;
	/** Where the eye sits to look through the sights, from the rifle's bounds: so far forward of its back end, so far above its top (the rifle points a little under the view, so the eye sits a little under the top), so far to the left of its middle. */
	/** The arms' place on the body when not aiming. */
	FTransform FirstPersonBase = FTransform::Identity;
	/**
	 * Keeps the first-person body on the body others see and the camera on its
	 * head, just under the eyes (down, a little ahead of the face), whatever
	 * the arms do: they are placed in view by the animation
	 * (FCheeseStrikeViewHold): at the hip where the weapon's hold has them,
	 * down the sights with the sight line through the eye.
	 */
	void PlaceView();
	/** The view's roll now, eased in and out of a slide. */
	float ViewRoll = 0.0f;
	/** The weapon in view the last two frames, how many of those are known, and the tremble summed since last asked, over so many frames. */
	FTransform WeaponInView[2];
	int32 WeaponInViewKnown = 0;
	FVector2D TrembleSum = FVector2D::ZeroVector;
	int32 TrembleFrames = 0;
	/** How far the first-person view is down (eased): the arms are then where the pose puts them, and the first-person body is drawn at the world's field of view, its legs lying on the floor in view. */
	float DownInView = 0.0f;
	/** Down, the view sits this far ahead of where the head has it (cm). */
	static constexpr float DownCameraAhead = 9.0f;
	/** Down: the camera, where the head has it, kept out of the floor and walls round it. */
	void KeepDownViewClear();
	/** The first-person body's own field of view, standing. */
	float FirstPersonFieldOfViewUp = 76.0f;
	/** GetDownBlend's, and the room for the legs. */
	float DownBlend = 0.0f;
	float DownLegRoom = CheeseStrikeDowned::LegLength;
	/** Down: where it aims from its body's facing (degrees), as the server has it, for the other machines. */
	UPROPERTY(Replicated)
	int8 DownAimYawSent = 0;
	/** Every frame: how far down it is, the room for the legs, the aim sent, and the eyes where the head is. */
	void UpdateDown(float DeltaSeconds);
	/** Where the camera sits in the first-person body's head bone: just under its eyes (from the body's eye bones), or where the template had it on the mannequin. */
	FVector CameraAtHead = DefaultCameraAtHead;
	static const FVector DefaultCameraAtHead;
	/** The camera from the eyes, in the head's space: 8 cm down and 1 cm back, as the template's camera sits from Belica's eyes. */
	static const FVector CameraFromEyes;
	/** Puts the camera just under the body's eyes, and hides the head (face, hair, eyes) in the first-person copy, which the camera sits inside. */
	void FitFirstPersonBody();
	/**
	 * Every body's mesh (CheeseStrikeBodies::All), and the material instances
	 * that dress each in each side's colours, by their paths: found as the
	 * game starts, so that a packaged game has them.
	 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> BodyMeshes;
	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInterface>> TeamLooks;
	/** The body it has here, and the side whose colours it wears. */
	int32 BodyIndex = INDEX_NONE;
	int32 BodyTeam = INDEX_NONE;
	/** CheeseStrikeBody's, while it is in use. */
	int32 ForcedBody = INDEX_NONE;
	int32 ForcedTeam = 0;
	/** Puts on the body its player's side has, on every machine (from the side the player state replicates), if it hasn't it already; a fallen body keeps the one it fell in. */
	void UpdateBody();
	/** Makes both meshes body `Index` in side `Team`'s colours, its own weapons hidden, the camera at its eyes. */
	void ApplyBody(int32 Index, int32 Team);
	/** Seconds since the state was last traced to the log. */
	float SinceTrace = 0.0f;
	/** Whether -StartAt has been applied. */
	bool bStartAtApplied = false;

	UFUNCTION()
	void OnRep_Health();
	/** Server only: it has just run out of health; `By` and `How` for a streak's kill (TakeBlast). */
	void Die(ACheeseStrikeCharacter* From, bool bHead, ACheeseStrikePlayerState* By = nullptr, const FString& How = FString());
	/** Server only: whether the match lets `From`, or a streak of `By`'s, hurt it (friendly fire is off in a team mode; nobody is hurt between rounds). */
	bool CanBeHurtBy(const ACheeseStrikeCharacter* From, const ACheeseStrikePlayerState* By) const;
	/** On every machine: the body falls, the view stays where the eyes were, and nothing more is done. */
	void ShowDeath();
	/** The server's clock. */
	float ServerNow() const;

	void ApplyStartAt();
	UFUNCTION(Server, Reliable)
	void ServerDie();
	UFUNCTION(Server, Reliable)
	void ServerProtect(float Seconds);
	/** Its own player's machine: it was just hurt by `By`, standing at `From`, for the HUD's damage arc. */
	UFUNCTION(Client, Unreliable)
	void ClientHurtFrom(FVector_NetQuantize From, ACheeseStrikeCharacter* By);
	/** Every machine: a landing that hurt, a thud and a grunt where it landed, louder the more it hurt. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHardLanding(float Damage);
	/** Who a fall's kill goes to: whoever hurt it lately and may, or nobody. */
	ACheeseStrikeCharacter* FallCreditedTo() const;
	UFUNCTION(Server, Reliable)
	void ServerStartAt(FVector_NetQuantize At, float Yaw);
	UFUNCTION(Client, Reliable)
	void ClientStartAt(FVector_NetQuantize At, float Yaw);

	/** The camera of CheeseStrikeOutside, while it is in use. */
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> OutsideCamera;
};
