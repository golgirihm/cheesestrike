// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheeseStrikeShot.h"
#include "CheeseStrikeWeapon.generated.h"

class ACheeseStrikeCharacter;
class UAnimInstance;
class UAnimSequence;
struct FCheeseStrikeAnimSet;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FWeaponLook;

/**
 * A weapon a character carries (UCheeseStrikeInventory holds one in each
 * slot): one of the arsenal's (CheeseStrikeArsenal), chosen as it is
 * spawned. It has two meshes, one only its owner sees in their first-person
 * view and one everyone else sees on the body, shown while it is in hand,
 * and fires by its FWeaponStats.
 *
 * A shot is fired on the shooter's machine, which shows it at once, kicks
 * the view and sends the server where it was aimed, with the seed its
 * spread (and a shotgun's pellets) come from. The server checks the shot
 * could have been fired (the weapon in hand and drawn, the rate, the
 * magazine, where it came from), puts every other player back to where the
 * shooter saw them (FHitHistory), decides what it hit, applies the damage,
 * and tells the shooter whether it hit. Ammunition is the server's count;
 * the shooter's own count runs ahead of it and is put right as it arrives.
 *
 * The knife strikes instead: a swipe swept across the aim, decided on the
 * server in the same way; it is shown on both bodies by the arms
 * (FCheeseStrikeKnifeHold, FCheeseStrikeViewHold), from SwipePhase.
 *
 * Its shots are heard from where they were fired, fading and growing duller
 * with distance (CheeseStrikeGunfire); its own player hears them whole.
 *
 * How it looks, sounds and is held is its FWeaponLook
 * (CheeseStrikeWeaponLooks): a skinned model (Lyra's rifle, shotgun and
 * pistol) or a plain one (the Fab guns, the knife) in the hand, its cheese
 * materials, its sights, its muzzle's flash, where the free hand holds it.
 */
UCLASS()
class CHEESESTRIKE_API ACheeseStrikeWeapon : public AActor
{
	GENERATED_BODY()

public:
	ACheeseStrikeWeapon();

	/** Which weapon of the arsenal it is: before it is spawned (SpawnActorDeferred), on the server. */
	void SetWeapon(FName Id);
	FName GetWeaponId() const { return WeaponId; }
	const FWeaponStats& GetStats() const { return *Stats; }
	/** The numbers it fires by now: its own, or worse in the hand of someone down (CheeseStrikeModes::DownedWeapon). */
	const FWeaponStats& FiringStats() const;
	int32 GetMagazine() const { return Magazine; }
	/** Rounds in reserve; below zero, there is no end to them. */
	int32 GetReserve() const { return Reserve; }
	/** Whether it never runs out of rounds to reload with: its stats say so (the Curd Pistol), or a mode made it so (Gun Game). */
	bool HasEndlessReserve() const { return Reserve < 0; }
	/** What the HUD shows of its rounds: "12 / 72", or "8" for one with rounds without end, or nothing for the knife. */
	FString DescribeRounds() const;
	bool IsReloading() const;
	/** For the log: seconds until the reload under way ends, by its end time and by its timer (-1 for none). */
	FString DescribeReload() const;
	bool IsAimed() const { return bAimed; }
	bool IsFiring() const { return bTriggerHeld; }
	/** Whether it is still coming up after being drawn, and how far up it is (0 to 1). */
	bool IsDrawing() const;
	float DrawnFraction() const;
	/** Whether a pull of the trigger now would fire: drawn, not reloading, the last shot's interval past, and a round or a blade. */
	bool IsReady() const;
	/** Whether it has nothing left: no round in the magazine or in reserve. The knife never has. */
	bool IsDry() const;
	/** How far through its swipe the knife is, 0 to 1, as this machine shows it; below 0 when it isn't swiping. */
	float SwipePhase() const;
	/** Which shot of the run of fire the next would be. */
	int32 NextShotIndex() const;
	/** The most the next shot may go off the aim, in degrees: half its cone. */
	float CurrentSpread() const;
	/** Shots (or strokes) fired by this machine's owner, and hits the server confirmed, for tests and the trace. */
	int32 ShotsFired = 0;
	int32 HitsConfirmed = 0;
	float LastHitDamage = 0.0f;
	bool bLastHitHead = false;
	/** Seconds since the server last confirmed a hit, for the HUD's marker. */
	float SinceHit() const;
	/** Seconds since it last fired (or struck), as this machine knows: its own player's machine and the server. */
	float SinceShot() const;

	/** How it is held, which picks the body's animations. */
	ECheeseStrikeHold GetHold() const;
	/** Which way its business end (muzzle, blade) points in its model's own space. */
	FVector GetModelAlong() const;
	/** Where `Body`'s left hand closes round this weapon, in the space of the bone the weapon rides (cm); false if it is held in one hand. */
	bool LeftHandHoldFor(const USkeletalMeshComponent* Body, FVector& OutHold) const;
	/** The model drawn in its player's view, or on the body: the skinned or the plain one, whichever it has. */
	UPrimitiveComponent* GetModel(bool bFirstPerson) const;
	/** Where the eye goes down its sights, in its model's own space, if its look says (else the rifle's way, from its bounds). */
	bool GetSight(FVector& Out) const;

	/** Puts the weapon in `Character`'s hands, on both meshes (hidden until it is the one in hand). */
	void Hold(ACheeseStrikeCharacter* Character);
	/**
	 * Puts its models in its holder's hands, as their body holds it and where
	 * its look has them, with the flash at the muzzle: as it is held, as its
	 * look changes, and after the body changes (ACheeseStrikeCharacter::ApplyBody).
	 */
	void FitToHands();
	/** Shows it in hand on this machine, or hides it away. */
	void SetInHand(bool bInHand);
	bool IsInHand() const { return bInHand; }
	/** It is drawn at `Now`, server time: it fires once its draw time is past. A time long gone puts it in hand at once. */
	void BeginDraw(float Now);
	/** It is put away: the trigger and the sights let go, any reload under way given up. */
	void PutAway();
	/** Asks the server to put this in hand, as its holder's request number `Request`. */
	UFUNCTION(Server, Reliable)
	void ServerDraw(uint8 Request);
	/**
	 * Attaches a weapon mesh to `Body`'s right hand: on the mannequin's
	 * weapon_r bone, or, for a body without one (Belica), on hand_r where
	 * the mannequin's weapon_r would be.
	 */
	static void PutInHand(USceneComponent* Rifle, USkeletalMeshComponent* Body);
	/**
	 * Where a thing held `InFromBone` in `Body`'s bone `FromBone` sits in its bone `ToBone` when held the mirror way
	 * across the body (left for right), by the reference pose: the second gun's grip in the left hand.
	 */
	static FTransform MirroredGrip(const USkeletalMeshComponent* Body, FName FromBone, const FTransform& InFromBone, FName ToBone);
	/** The second gun, in its player's view or on the body, if it has one in hand. */
	UStaticMeshComponent* GetOffHandModel(bool bFirstPerson) const { return bFirstPerson ? FirstPersonOffHand : ThirdPersonOffHand; }
	/** Hides any weapon a body's mesh was modelled holding (Belica's own guns), so only ours shows. */
	static void HideBuiltInWeapons(USkeletalMeshComponent* Body);

	void StartFiring();
	void StopFiring();
	void StartAiming();
	void StopAiming();
	void StartReload();
	/** Server only: adds `Rounds` to the reserve, up to the most it can hold. */
	void AddReserve(int32 Rounds);
	/** Server only: its magazine and reserve as they are given (a weapon taken from the floor, with the rounds it lay with), each within what it can hold. */
	void SetRounds(int32 InMagazine, int32 InReserve);
	/** Server only: gives it rounds without end to reload with (Gun Game's weapons); it still reloads. */
	void MakeReserveEndless();

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }
	/** Lays `Overlay` over its model on the body and `ViewOverlay` over the one in its holder's view (the ghostly look of a protected holder), or takes them off with null. */
	void SetOverlay(UMaterialInterface* Overlay, UMaterialInterface* ViewOverlay);
	/** For the log: where its first-person and body models are from `Eye`, and whether they are drawn. */
	FString DescribeModel(const FVector& Eye) const;
	/** Shows the weapon on the body to its own player as well (CheeseStrikeOutside), or keeps it from them as in play. */
	void SetSeenFromOutside(bool bOutside);

	//~ AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

protected:
	/** Fires one shot (or strikes once) on the owner's machine, if the weapon can. */
	void FireOnce();
	/** Fires shot number `InBurst` of a burst (0 the first, or the only one): the kick, the show and the word to the server. */
	void FireShot(int32 InBurst);
	/** The rest of a burst, after its first shot. */
	void FireBurstShot();
	/** Swings the knife, on the owner's machine. */
	void Strike(float Now);
	/** Tells the server a shot was fired from `Origin` along `AimDirection` (before spread), shot `Index` of a run, spread from `Seed`, at the server's time `ClientTime` as the shooter had it. */
	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, int32 Seed, int32 Index, bool bWasAimed, float ClientTime);
	/** Tells the server the knife was swung from `Origin` along `AimDirection`, at `ClientTime`. */
	UFUNCTION(Server, Reliable)
	void ServerStrike(FVector_NetQuantize Origin, FVector_NetQuantizeNormal AimDirection, float ClientTime);
	/** Tells the shooter the server found their shot hit, for `Damage`, on the head or not. */
	UFUNCTION(Client, Reliable)
	void ClientHit(float Damage, bool bHead);
	UFUNCTION(Server, Reliable)
	void ServerReload();
	void FinishReload();
	/** Ends a reload under way without filling the magazine (the weapon is put away). */
	void CancelReload();
	/** Shows a shot on this machine: the body's and the rifle's firing animations, the muzzle's flash, and the sound. */
	void ShowShot();
	/** Shows a draw on this machine: the body's animation for bringing it up, and its sound. */
	void ShowDraw();
	/** Lights the muzzle for a moment. */
	void Flash();
	/** Shows a reload on this machine: the body's and the rifle's animations, and the sounds along the way. */
	void ShowReload();
	/** Tells every other machine of a shot or a reload the server allowed, to show it. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastReload();
	/** Plays each body's own copy of an animation of the set (`Which` picks it), at `PlayRate` (from `RateFor` its length, if given). */
	void PlayOnBodies(TObjectPtr<UAnimSequence> FCheeseStrikeAnimSet::* Which, TFunction<float(const UAnimSequence*)> RateFor = nullptr);
	/** Plays one of its sounds: the reload's and the dry click from the weapon, as heard there. */
	void PlaySound(USoundBase* Sound) const;
	/** Plays a shot's sound: whole for its own player, from where it was fired for everyone else. */
	void PlayShotSound(USoundBase* Sound) const;
	/** Whether the server can allow a shot now: in hand and drawn, the rate, and the magazine. */
	bool ServerAllowsShot(float Now);
	/** Whether a shot reported from `Origin` at `When` came from about where the server had the shooter's eyes. */
	bool ServerTrustsOrigin(const FVector& Origin, float When) const;
	/** Puts the meshes, sounds and look of the weapon it is, from its id. */
	void ApplyWeapon();

	/**
	 * The server's trace of one ray of a shot: the nearest player along it
	 * (where they were at `When`) within `Range`, or else what of the arena
	 * stopped it. Returns whether it hit a player, with how far, whether on
	 * the head, and which way they faced.
	 */
	struct FRayHit
	{
		ACheeseStrikeCharacter* Victim = nullptr;
		bool bHead = false;
		float Distance = 0.0f;
		float VictimYaw = 0.0f;
		bool bWall = false;
		FHitResult Wall;
	};
	FRayHit TraceRay(const FVector& Origin, const FVector& Direction, float Range, float When) const;

	/** Which weapon it is: sent once, as it is made. */
	UPROPERTY(Replicated)
	FName WeaponId;
	/** The weapon's numbers. */
	const FWeaponStats* Stats = nullptr;
	/** Its numbers as someone down fires it, made when asked for. */
	mutable FWeaponStats DownedStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;
	/** The same for a weapon whose model is a plain mesh (the knife), in place of the skinned ones. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FirstPersonModel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ThirdPersonModel;
	/**
	 * The second gun of a weapon with two (the Dual Burratas, FWeaponStats::bDual): the same model in the left hand, in
	 * its player's view and on the body, held as the right hand holds the first, mirrored across the body
	 * (MirroredGrip). The two fire in turn, each flashing at its own muzzle.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FirstPersonOffHand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ThirdPersonOffHand;
	/** Shots shown on this machine: which of two guns fires the next (CheeseStrikeWeapons::DualHand). */
	int32 ShotsShown = 0;
	/** Puts the flash at the muzzle of the right gun or (`bLeft`) the left one. */
	void PutFlashOn(bool bLeft);
	/** The muzzle's flash, in its player's view and on the body, and its light. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> FirstPersonFlash;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ThirdPersonFlash;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> FlashLight;

	/** The iron sights of a gun without a scope or the rail's dot: a front post on the muzzle and a rear notch, on the first-person model. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> IronSightFront;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> IronSightRear;
	/** Makes the sights (or hides them, for a gun that has its own) for the weapon in hand. */
	void PlaceIronSights();
	/** The look it has. */
	const FWeaponLook* Look = nullptr;
	/** When the flash goes out (world time). */
	float FlashUntil = 0.0f;

	/** What its look names, loaded as it becomes that weapon: its model's own animations, and its sounds (Lyra's; Content/LICENSES.md). */
	UPROPERTY()
	TObjectPtr<UAnimSequence> ModelFire;
	UPROPERTY()
	TObjectPtr<UAnimSequence> ModelReload;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> FireSounds;
	UPROPERTY()
	TObjectPtr<USoundBase> DryFireSound;
	UPROPERTY()
	TObjectPtr<USoundBase> ClipOutSound;
	UPROPERTY()
	TObjectPtr<USoundBase> ClipInSound;
	UPROPERTY()
	TObjectPtr<USoundBase> BoltSound;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> HitSounds;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> DrawSounds;
	float ShotVolume = 1.0f;
	float ShotPitch = 1.0f;
	FTimerHandle ClipInTimer;
	FTimerHandle BoltTimer;

	/** Rounds in the magazine and in reserve: the server's count, sent to the owner. */
	UPROPERTY(Replicated)
	int32 Magazine = 0;
	UPROPERTY(Replicated)
	int32 Reserve = 0;
	/** The server's time the reload under way ends, or 0. */
	UPROPERTY(Replicated)
	float ReloadEndsAt = 0.0f;

	/** Whether it is the one in hand on this machine, and the server time it was drawn. */
	bool bInHand = false;
	float DrawnAt = -1000.0f;
	/** The owner's side of firing. */
	bool bTriggerHeld = false;
	bool bAimed = false;
	FTimerHandle FireTimer;
	FTimerHandle BurstTimer;
	/** A pull of the trigger while the weapon was coming up, to fire once it is. */
	FTimerHandle PendingShotTimer;
	FTimerHandle ReloadTimer;
	/** When the last shot was fired here, in server time, which of its run it was, and which of its burst; -1 for none. */
	float LastShotAt = -1000.0f;
	int32 LastShotIndex = -1;
	int32 LastInBurst = 0;
	/** Shots of the burst under way still to come. */
	int32 BurstLeft = 0;
	/** The world time this machine last showed a stroke, and the server's time of its last stroke. */
	float StrokeShownAt = -1000.0f;
	float ServerLastStrike = -1000.0f;
	float LastHitAt = -1000.0f;
	/** The server's side: when the shots it allowed were, for the rate, and the last of them. */
	TArray<float> ServerShotTimes;
	float ServerLastShotAt = -1000.0f;

	UPROPERTY()
	TObjectPtr<ACheeseStrikeCharacter> Holder;
};
