// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "CheeseStrikeShot.h"
#include "CheeseStrikeInventory.generated.h"

class ACheeseStrikeCharacter;
class ACheeseStrikeWeapon;
class AController;

/** What a character is given to carry: a weapon's id for each slot, or none. */
USTRUCT()
struct CHEESESTRIKE_API FCheeseStrikeLoadout
{
	GENERATED_BODY()

	UPROPERTY()
	FName Primary;
	UPROPERTY()
	FName Sidearm;
	UPROPERTY()
	FName Knife;

	/** The usual: `Primary` (the default one, if that isn't a primary), the Curd Pistol and the knife. */
	static FCheeseStrikeLoadout Usual(FName Primary = NAME_None);
	/** Only `Weapon`, in its own slot (Gun Game). */
	static FCheeseStrikeLoadout Only(FName Weapon);
	/** The weapon for `Slot`, or none. */
	FName For(EWeaponSlot Slot) const;
};

/**
 * What a character carries: a weapon in each slot (a primary, the sidearm,
 * the knife), one of them in hand. The server spawns the weapons and owns
 * them; every machine shows only the one in hand.
 *
 * Changing weapons (1, 2, 3, the wheel, Q for the last) happens at once on
 * the player's own machine, which asks the server through the weapon being
 * drawn, so that its shots that follow arrive after it; the server agrees
 * or puts the player right. A weapon fires only once its draw time has
 * passed, here and on the server alike. Ammunition is each weapon's own.
 *
 * The game's modes change what a character carries on the server: Give a
 * loadout, GiveOnly one weapon (Gun Game), RestrictTo one slot (Execution:
 * the sidearm), HoldNow to put a weapon in hand without its draw time.
 * Every character is given the usual loadout as it is possessed, with the
 * primary its player chose (UCheeseStrikeLoadouts), before the game mode's
 * SetPlayerDefaults, where a mode can give another.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeInventory : public UActorComponent
{
	GENERATED_BODY()

public:
	UCheeseStrikeInventory();

	/** No slot: nothing in hand. */
	static constexpr uint8 NoSlot = 0xFF;
	static constexpr uint8 EverySlot = (1 << NumWeaponSlots) - 1;

	// The game's modes, on the server.

	/** Takes away whatever is carried and gives `Loadout`, its first weapon (primary, sidearm, knife) in hand at once, every slot allowed. */
	void Give(const FCheeseStrikeLoadout& Loadout);
	/** Gives only `Weapon`, in hand at once: Gun Game's next weapon. */
	void GiveOnly(FName Weapon);
	/** Only the weapon in `Slot` may be drawn from now on, and it is put in hand at once: Execution's sidearm. */
	void RestrictTo(EWeaponSlot Slot);
	/** Every slot may be drawn again. */
	void LiftRestriction();
	/** Puts the weapon in `Slot` in hand at once, without its draw time, if it carries one there. */
	void HoldNow(EWeaponSlot Slot);
	/** Server: gives the usual loadout with the primary chosen for `Controller` (as the character is possessed). */
	void GiveSpawnLoadout(AController* Controller);

	// Reading, on any machine.

	/** The weapon in hand on this machine, or null. */
	ACheeseStrikeWeapon* GetHeld() const;
	/** Which slot is in hand on this machine, or false for none. */
	bool GetHeldSlot(EWeaponSlot& Slot) const;
	/** The weapon carried in `Slot`, or null. */
	ACheeseStrikeWeapon* GetInSlot(EWeaponSlot Slot) const;
	/** Whether `Slot` may be drawn: something is carried there and the mode allows it. */
	bool CanDraw(EWeaponSlot Slot) const;
	bool IsAllowed(EWeaponSlot Slot) const { return (AllowedSlots & (1 << static_cast<uint8>(Slot))) != 0; }
	/** The slot last in hand before this one, for Q, or false for none. */
	bool GetLastSlot(EWeaponSlot& Slot) const;

	// The player's (or bot's) own controls, on its own machine.

	/** Changes to the weapon in `Slot`, if it can. */
	void Draw(EWeaponSlot Slot);
	/** Changes to the next weapon it can draw `Step` slots on (1 the next, -1 the one before), going round. */
	void DrawNext(int32 Step);
	/** Changes back to the weapon in hand before this one. */
	void DrawLast();

	/** Server: the weapon `Weapon` asks, as the owner's request `Request`, to be put in hand. */
	void DrawAsked(ACheeseStrikeWeapon* Weapon, uint8 Request);
	/** Shows on this machine the weapon in hand and hides the rest: when something arrives or changes. */
	void Refresh();

	/** For looking at the weapons (development, `CheeseStrikeArm <weapon>`): the usual loadout with `Weapon` as its primary, or `Weapon` in hand if it is the sidearm or the knife. */
	UFUNCTION(Server, Reliable)
	void ServerArm(FName Weapon);

	/** Server: the player's choice of primary, from their machine, kept for their next character (or this one, still fresh). */
	UFUNCTION(Server, Reliable)
	void ServerChoosePrimary(FName Primary);

	//~ UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION()
	void OnRep_Carried();

	ACheeseStrikeCharacter* Holder() const;
	ACheeseStrikeWeapon* At(uint8 Slot) const;
	/** Whether this machine is the one its player plays on, apart from the server: it shows its own changes before the server has them. */
	bool IsOwnerClient() const;
	/** Puts the weapon in `Slot` in hand on this machine, the one before away; `bAtOnce` without its draw time. */
	void Show(uint8 Slot, bool bAtOnce);
	/** Server: puts `Slot` in hand, on the server and for everyone. */
	void SetHeld(uint8 Slot, bool bAtOnce);
	void DestroyWeapons();
	void GiveInternal(const FCheeseStrikeLoadout& Loadout, bool bSpawnLoadout);
	float ServerNow() const;

	/** The weapons carried, by slot (null for none): the server's. */
	UPROPERTY(ReplicatedUsing=OnRep_Carried)
	TArray<TObjectPtr<ACheeseStrikeWeapon>> Weapons;
	/** The slot in hand, as the server has it. */
	UPROPERTY(ReplicatedUsing=OnRep_Carried)
	uint8 HeldSlot = NoSlot;
	/** The slots that may be drawn, as bits. */
	UPROPERTY(Replicated)
	uint8 AllowedSlots = EverySlot;
	/** The last of its player's requests the server has answered: until it catches up with the player's last, the player's own machine shows its own choice. */
	UPROPERTY(ReplicatedUsing=OnRep_Carried)
	uint8 Answered = 0;

	/** The player's machine: the last request sent, and the slot before this one. */
	uint8 Asked = 0;
	uint8 LastSlot = NoSlot;
	/** The slot shown in hand on this machine. */
	uint8 ShownSlot = NoSlot;
	/** The weapons this machine last saw carried, to know when they are replaced. */
	TArray<TWeakObjectPtr<ACheeseStrikeWeapon>> Seen;
	/** Whether its player's choice of primary has been sent to the server. */
	bool bToldChoice = false;
	/** Server: whether what it carries is the loadout it was given on coming in, untouched by a mode, so that a choice made now can take its place. */
	bool bSpawnLoadout = false;
};

/**
 * The primaries the server has been told players chose, kept for their next
 * character; bots choose their own. For the world's life (a match).
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeLoadouts : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: `Controller`'s player chose `Primary`. */
	void Choose(const AController* Controller, FName Primary);
	/** The primary `Controller` chose, or the default. */
	FName PrimaryFor(const AController* Controller) const;
	bool HasChosen(const AController* Controller) const;

private:
	TMap<TWeakObjectPtr<const AController>, FName> Chosen;
};

/**
 * The primary this game's player has chosen, for as long as the game runs:
 * set by the console command `CheeseStrikeLoadout <weapon>` (and by the
 * menu's loadout page once there is one), and sent to the server for the
 * next character. Not saved between runs yet.
 */
UCLASS()
class CHEESESTRIKE_API UCheeseStrikeLoadoutChoice : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FName GetPrimary() const { return Primary; }
	/** Chooses `Weapon` as the primary, if it is one, and tells the server through the local player's character if there is one; false if it isn't a primary. */
	bool Choose(FName Weapon, UWorld* World);

private:
	FName Primary;
};
