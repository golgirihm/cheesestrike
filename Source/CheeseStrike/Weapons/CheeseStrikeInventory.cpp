// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#include "CheeseStrikeInventory.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "CheeseStrike.h"
#include "CheeseStrikeArsenal.h"
#include "CheeseStrikeCharacter.h"
#include "CheeseStrikePickup.h"
#include "CheeseStrikeWeapon.h"

namespace
{
	TAutoConsoleVariable<FString> CVarSpawnPrimary(TEXT("CheeseStrike.SpawnPrimary"), TEXT(""),
		TEXT("The primary everyone comes in with who hasn't chosen one, in place of the Colby Carbine (the play tests carry the Cheddar Blaster, as they were written); empty for the carbine."),
		ECVF_Cheat);
}

FCheeseStrikeLoadout FCheeseStrikeLoadout::Usual(FName Primary)
{
	const FWeaponStats* Chosen = CheeseStrikeArsenal::Find(Primary);
	FCheeseStrikeLoadout Loadout;
	Loadout.Primary = Chosen != nullptr && Chosen->Slot == EWeaponSlot::Primary ? Chosen->Id : CheeseStrikeArsenal::DefaultPrimary();
	Loadout.Sidearm = CheeseStrikeArsenal::Sidearm();
	Loadout.Knife = CheeseStrikeArsenal::Knife();
	return Loadout;
}

FCheeseStrikeLoadout FCheeseStrikeLoadout::Only(FName Weapon)
{
	FCheeseStrikeLoadout Loadout;
	if (const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon))
	{
		switch (Stats->Slot)
		{
		case EWeaponSlot::Primary: Loadout.Primary = Stats->Id; break;
		case EWeaponSlot::Sidearm: Loadout.Sidearm = Stats->Id; break;
		case EWeaponSlot::Knife: Loadout.Knife = Stats->Id; break;
		}
	}
	return Loadout;
}

FName FCheeseStrikeLoadout::For(EWeaponSlot Slot) const
{
	switch (Slot)
	{
	case EWeaponSlot::Primary: return Primary;
	case EWeaponSlot::Sidearm: return Sidearm;
	case EWeaponSlot::Knife: return Knife;
	}
	return NAME_None;
}

UCheeseStrikeInventory::UCheeseStrikeInventory()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	Weapons.SetNum(NumWeaponSlots);
}

void UCheeseStrikeInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCheeseStrikeInventory, Weapons);
	DOREPLIFETIME(UCheeseStrikeInventory, HeldSlot);
	DOREPLIFETIME_CONDITION(UCheeseStrikeInventory, AllowedSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCheeseStrikeInventory, Answered, COND_OwnerOnly);
}

ACheeseStrikeCharacter* UCheeseStrikeInventory::Holder() const
{
	return Cast<ACheeseStrikeCharacter>(GetOwner());
}

ACheeseStrikeWeapon* UCheeseStrikeInventory::At(uint8 Slot) const
{
	return Weapons.IsValidIndex(Slot) ? Weapons[Slot].Get() : nullptr;
}

float UCheeseStrikeInventory::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
	return State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
}

bool UCheeseStrikeInventory::IsOwnerClient() const
{
	const ACheeseStrikeCharacter* Character = Holder();
	return Character != nullptr && !Character->HasAuthority() && Character->IsLocallyControlled();
}

ACheeseStrikeWeapon* UCheeseStrikeInventory::GetHeld() const
{
	return At(ShownSlot);
}

bool UCheeseStrikeInventory::GetHeldSlot(EWeaponSlot& Slot) const
{
	if (At(ShownSlot) == nullptr)
	{
		return false;
	}
	Slot = static_cast<EWeaponSlot>(ShownSlot);
	return true;
}

ACheeseStrikeWeapon* UCheeseStrikeInventory::GetInSlot(EWeaponSlot Slot) const
{
	return At(static_cast<uint8>(Slot));
}

bool UCheeseStrikeInventory::CanDraw(EWeaponSlot Slot) const
{
	return GetInSlot(Slot) != nullptr && IsAllowed(Slot);
}

bool UCheeseStrikeInventory::GetLastSlot(EWeaponSlot& Slot) const
{
	if (LastSlot >= NumWeaponSlots)
	{
		return false;
	}
	Slot = static_cast<EWeaponSlot>(LastSlot);
	return true;
}

void UCheeseStrikeInventory::Show(uint8 Slot, bool bAtOnce)
{
	ACheeseStrikeWeapon* Before = At(ShownSlot);
	ACheeseStrikeWeapon* After = At(Slot);
	if (Before != After)
	{
		if (Before != nullptr)
		{
			Before->PutAway();
		}
		if (After != nullptr)
		{
			After->BeginDraw(bAtOnce ? -1000.0f : ServerNow());
		}
	}
	ShownSlot = After != nullptr ? Slot : NoSlot;
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		if (ACheeseStrikeWeapon* Weapon = Weapons[Index])
		{
			Weapon->SetInHand(Index == ShownSlot);
		}
	}
}

void UCheeseStrikeInventory::SetHeld(uint8 Slot, bool bAtOnce)
{
	HeldSlot = At(Slot) != nullptr ? Slot : NoSlot;
	Show(HeldSlot, bAtOnce);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UCheeseStrikeInventory::Refresh()
{
	if (GetOwner() == nullptr)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		Show(HeldSlot, false);
		return;
	}
	if (IsOwnerClient())
	{
		// New weapons, all of them: what was asked for the old ones no longer matters.
		bool bReplaced = Seen.Num() != Weapons.Num();
		for (int32 Index = 0; !bReplaced && Index < Weapons.Num(); ++Index)
		{
			bReplaced = Seen[Index].Get() != Weapons[Index].Get();
		}
		if (bReplaced)
		{
			Seen.Reset();
			for (const TObjectPtr<ACheeseStrikeWeapon>& Weapon : Weapons)
			{
				Seen.Add(Weapon.Get());
			}
			Asked = Answered;
			LastSlot = NoSlot;
		}
		// Until the server has answered every change asked of it, the player sees their own.
		if (Asked == Answered)
		{
			Show(HeldSlot, ShownSlot != HeldSlot);
		}
		else
		{
			Show(ShownSlot, false);
		}
		return;
	}
	// Someone else's: a change is seen being drawn; the first weapon is simply there.
	Show(HeldSlot, ShownSlot == NoSlot);
}

void UCheeseStrikeInventory::OnRep_Carried()
{
	Refresh();
}

void UCheeseStrikeInventory::Draw(EWeaponSlot Slot)
{
	ACheeseStrikeCharacter* Character = Holder();
	const uint8 Index = static_cast<uint8>(Slot);
	if (Character == nullptr || !Character->IsLocallyControlled() || !Character->CanAct() || !CanDraw(Slot) || Index == ShownSlot)
	{
		return;
	}
	ACheeseStrikeWeapon* Weapon = At(Index);
	LastSlot = ShownSlot;
	if (Character->HasAuthority())
	{
		SetHeld(Index, false);
	}
	else
	{
		Show(Index, false);
		++Asked;
		Weapon->ServerDraw(Asked);
	}
	UE_LOG(LogCheeseStrike, Verbose, TEXT("%s draws the %s"), *Character->DisplayName(), *Weapon->GetStats().DisplayName.ToString());
}

void UCheeseStrikeInventory::DrawNext(int32 Step)
{
	if (Step == 0)
	{
		return;
	}
	const int32 From = ShownSlot < NumWeaponSlots ? ShownSlot : 0;
	for (int32 Tried = 1; Tried < NumWeaponSlots; ++Tried)
	{
		const int32 Slot = ((From + Tried * FMath::Sign(Step)) % NumWeaponSlots + NumWeaponSlots) % NumWeaponSlots;
		if (CanDraw(static_cast<EWeaponSlot>(Slot)))
		{
			Draw(static_cast<EWeaponSlot>(Slot));
			return;
		}
	}
}

void UCheeseStrikeInventory::DrawLast()
{
	EWeaponSlot Slot;
	if (GetLastSlot(Slot) && CanDraw(Slot))
	{
		Draw(Slot);
	}
	else
	{
		// Nothing before: the next, as a quick change does in most games.
		DrawNext(1);
	}
}

void UCheeseStrikeInventory::DrawAsked(ACheeseStrikeWeapon* Weapon, uint8 Request)
{
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || !Character->HasAuthority())
	{
		return;
	}
	Answered = Request;
	const int32 Index = Weapons.IndexOfByKey(Weapon);
	if (Index == INDEX_NONE || !IsAllowed(static_cast<EWeaponSlot>(Index)) || !Character->CanAct())
	{
		UE_LOG(LogCheeseStrike, Display, TEXT("%s may not draw %s now"), *Character->DisplayName(), Weapon != nullptr ? *Weapon->GetStats().DisplayName.ToString() : TEXT("that"));
		Character->ForceNetUpdate();
		return;
	}
	LastSlot = HeldSlot;
	if (Index != HeldSlot)
	{
		SetHeld(static_cast<uint8>(Index), false);
	}
	else
	{
		Character->ForceNetUpdate();
	}
}

void UCheeseStrikeInventory::DestroyWeapons()
{
	for (TObjectPtr<ACheeseStrikeWeapon>& Weapon : Weapons)
	{
		if (Weapon != nullptr)
		{
			Weapon->Destroy();
		}
		Weapon = nullptr;
	}
	HeldSlot = NoSlot;
	ShownSlot = NoSlot;
	LastSlot = NoSlot;
}

void UCheeseStrikeInventory::GiveInternal(const FCheeseStrikeLoadout& Loadout, bool bIsSpawnLoadout)
{
	ACheeseStrikeCharacter* Character = Holder();
	UWorld* World = GetWorld();
	if (Character == nullptr || World == nullptr || !Character->HasAuthority())
	{
		return;
	}
	DestroyWeapons();
	Weapons.SetNum(NumWeaponSlots);
	uint8 First = NoSlot;
	for (uint8 Slot = 0; Slot < NumWeaponSlots; ++Slot)
	{
		const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Loadout.For(static_cast<EWeaponSlot>(Slot)));
		if (Stats == nullptr || Stats->Slot != static_cast<EWeaponSlot>(Slot))
		{
			continue;
		}
		// Owned by the character, so that its player's machine may fire it, and spawned as that weapon.
		const FTransform Where = Character->GetActorTransform();
		ACheeseStrikeWeapon* Weapon = World->SpawnActorDeferred<ACheeseStrikeWeapon>(ACheeseStrikeWeapon::StaticClass(), Where, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Weapon == nullptr)
		{
			continue;
		}
		Weapon->SetWeapon(Stats->Id);
		UGameplayStatics::FinishSpawningActor(Weapon, Where);
		Weapons[Slot] = Weapon;
		if (First == NoSlot)
		{
			First = Slot;
		}
	}
	AllowedSlots = EverySlot;
	bSpawnLoadout = bIsSpawnLoadout;
	SetHeld(First, true);
	UE_LOG(LogCheeseStrike, Display, TEXT("%s carries %s, %s and %s"), *Character->DisplayName(),
		At(0) != nullptr ? *At(0)->GetStats().DisplayName.ToString() : TEXT("no primary"),
		At(1) != nullptr ? *At(1)->GetStats().DisplayName.ToString() : TEXT("no sidearm"),
		At(2) != nullptr ? *At(2)->GetStats().DisplayName.ToString() : TEXT("no knife"));
}

void UCheeseStrikeInventory::Give(const FCheeseStrikeLoadout& Loadout)
{
	GiveInternal(Loadout, false);
}

void UCheeseStrikeInventory::GiveOnly(FName Weapon)
{
	GiveInternal(FCheeseStrikeLoadout::Only(Weapon), false);
}

void UCheeseStrikeInventory::GiveSpawnLoadout(AController* Controller)
{
	const UCheeseStrikeLoadouts* Loadouts = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UCheeseStrikeLoadouts>() : nullptr;
	GiveInternal(FCheeseStrikeLoadout::Usual(Loadouts != nullptr ? Loadouts->PrimaryFor(Controller) : NAME_None), true);
}

bool UCheeseStrikeInventory::TakePickup(ACheeseStrikePickup* Pickup)
{
	using CheeseStrikePickups::ETake;
	ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr || Pickup == nullptr || Pickup->IsActorBeingDestroyed() || !Character->HasAuthority()
		|| !Pickup->IsInReach(Character, CheeseStrikePickups::PressReach + CheeseStrikePickups::ServerSlack))
	{
		return false;
	}
	const ETake How = Pickup->HowFor(Character);
	if (How == ETake::None)
	{
		return false;
	}
	ACheeseStrikeWeapon* Carried = At(static_cast<uint8>(EWeaponSlot::Primary));
	if (How == ETake::TopUp && Carried != nullptr)
	{
		Carried->AddReserve(CheeseStrikePickups::TopUpRounds(Pickup->IsDropped(), Carried->GetStats().Reserve, Pickup->GetDroppedMagazine(), Pickup->GetDroppedReserve()));
	}
	else
	{
		// The one carried goes down on the floor, its rounds with it, for anyone to take.
		if (Carried != nullptr)
		{
			ACheeseStrikePickup::Drop(Character, Carried->GetWeaponId(), Carried->GetMagazine(), Carried->GetReserve());
		}
		SwapPrimary(Pickup->GetWeapon());
		if (ACheeseStrikeWeapon* Fresh = At(static_cast<uint8>(EWeaponSlot::Primary)); Fresh != nullptr && Pickup->IsDropped())
		{
			Fresh->SetRounds(Pickup->GetDroppedMagazine(), Pickup->GetDroppedReserve());
		}
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s takes the %s from %s%s"), *Character->DisplayName(), *Pickup->GetWeaponName().ToString(), Pickup->IsDropped() ? TEXT("the floor") : *Pickup->GetName(),
		How == ETake::TopUp ? TEXT(", for its rounds") : TEXT(""));
	Pickup->Take();
	return true;
}

void UCheeseStrikeInventory::SwapPrimary(FName Weapon)
{
	ACheeseStrikeCharacter* Character = Holder();
	UWorld* World = GetWorld();
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
	if (Character == nullptr || World == nullptr || !Character->HasAuthority() || Stats == nullptr || Stats->Slot != EWeaponSlot::Primary)
	{
		return;
	}
	const uint8 Slot = static_cast<uint8>(EWeaponSlot::Primary);
	Weapons.SetNum(NumWeaponSlots);
	// The one carried is gone, put away first if it was in hand, so that the new one is drawn.
	if (ACheeseStrikeWeapon* Old = At(Slot))
	{
		if (ShownSlot == Slot)
		{
			Old->PutAway();
			ShownSlot = NoSlot;
		}
		Old->Destroy();
		Weapons[Slot] = nullptr;
	}
	const FTransform Where = Character->GetActorTransform();
	ACheeseStrikeWeapon* Fresh = World->SpawnActorDeferred<ACheeseStrikeWeapon>(ACheeseStrikeWeapon::StaticClass(), Where, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Fresh == nullptr)
	{
		return;
	}
	Fresh->SetWeapon(Stats->Id);
	UGameplayStatics::FinishSpawningActor(Fresh, Where);
	Weapons[Slot] = Fresh;
	bSpawnLoadout = false;
	if (HeldSlot != Slot)
	{
		LastSlot = HeldSlot;
	}
	// In hand at once, here and on every machine (whose own copy of the slot has a new weapon in it, never drawn).
	SetHeld(Slot, true);
}

bool UCheeseStrikeInventory::PressForPickup()
{
	ACheeseStrikeCharacter* Character = Holder();
	ACheeseStrikePickup* Offered = Character != nullptr && Character->IsLocallyControlled() ? ACheeseStrikePickup::OfferedTo(Character) : nullptr;
	if (Offered == nullptr)
	{
		return false;
	}
	if (Character->HasAuthority())
	{
		TakePickup(Offered);
	}
	else
	{
		ServerTakePickup(Offered);
	}
	return true;
}

void UCheeseStrikeInventory::ServerTakePickup_Implementation(ACheeseStrikePickup* Pickup)
{
	TakePickup(Pickup);
}

void UCheeseStrikeInventory::RestrictTo(EWeaponSlot Slot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return;
	}
	AllowedSlots = 1 << static_cast<uint8>(Slot);
	bSpawnLoadout = false;
	HoldNow(Slot);
}

void UCheeseStrikeInventory::LiftRestriction()
{
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		AllowedSlots = EverySlot;
	}
}

void UCheeseStrikeInventory::HoldNow(EWeaponSlot Slot)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority() || GetInSlot(Slot) == nullptr)
	{
		return;
	}
	if (HeldSlot != static_cast<uint8>(Slot))
	{
		LastSlot = HeldSlot;
	}
	SetHeld(static_cast<uint8>(Slot), true);
}

void UCheeseStrikeInventory::ServerChoosePrimary_Implementation(FName Primary)
{
	ACheeseStrikeCharacter* Character = Holder();
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Primary);
	UCheeseStrikeLoadouts* Loadouts = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UCheeseStrikeLoadouts>() : nullptr;
	if (Character == nullptr || Stats == nullptr || Stats->Slot != EWeaponSlot::Primary || Loadouts == nullptr)
	{
		return;
	}
	Loadouts->Choose(Character->GetController(), Stats->Id);
	// Just come in, untouched by the mode and not yet fighting: the choice is carried at once.
	const ACheeseStrikeWeapon* Carried = At(static_cast<uint8>(EWeaponSlot::Primary));
	if (bSpawnLoadout && Character->IsProtected() && (Carried == nullptr || Carried->GetStats().Id != Stats->Id))
	{
		GiveInternal(FCheeseStrikeLoadout::Usual(Stats->Id), true);
	}
	UE_LOG(LogCheeseStrike, Display, TEXT("%s chose the %s"), *Character->DisplayName(), *Stats->DisplayName.ToString());
}

void UCheeseStrikeInventory::ServerArm_Implementation(FName Weapon)
{
#if !UE_BUILD_SHIPPING
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
	if (Stats == nullptr)
	{
		return;
	}
	if (Stats->Slot == EWeaponSlot::Primary)
	{
		Give(FCheeseStrikeLoadout::Usual(Stats->Id));
	}
	else
	{
		HoldNow(Stats->Slot);
	}
#endif
}

void UCheeseStrikeInventory::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// The player's choice of primary goes to the server once, as soon as this is their character.
	const ACheeseStrikeCharacter* Character = Holder();
	if (Character == nullptr)
	{
		return;
	}
	// Someone else's, here: nothing to tell from this machine.
	const bool bOwnPlayers = Character->IsLocallyControlled() && Character->IsPlayerControlled();
	if (Character->GetLocalRole() == ROLE_SimulatedProxy || (Character->HasAuthority() && Character->GetController() != nullptr && !bOwnPlayers))
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (!bToldChoice && bOwnPlayers)
	{
		bToldChoice = true;
		SetComponentTickEnabled(false);
		const UGameInstance* Game = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
		const UCheeseStrikeLoadoutChoice* Choice = Game != nullptr ? Game->GetSubsystem<UCheeseStrikeLoadoutChoice>() : nullptr;
		if (Choice != nullptr && !Choice->GetPrimary().IsNone())
		{
			ServerChoosePrimary(Choice->GetPrimary());
		}
	}
}

void UCheeseStrikeInventory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The weapons go with their character.
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		DestroyWeapons();
	}
	Super::EndPlay(EndPlayReason);
}

void UCheeseStrikeLoadouts::Choose(const AController* Controller, FName Primary)
{
	if (Controller != nullptr)
	{
		Chosen.Add(Controller, Primary);
	}
}

FName UCheeseStrikeLoadouts::PrimaryFor(const AController* Controller) const
{
	const FName* Found = Controller != nullptr ? Chosen.Find(Controller) : nullptr;
	if (Found != nullptr)
	{
		return *Found;
	}
	const FWeaponStats* Instead = CheeseStrikeArsenal::Find(FName(*CVarSpawnPrimary.GetValueOnGameThread()));
	return Instead != nullptr && Instead->Slot == EWeaponSlot::Primary ? Instead->Id : CheeseStrikeArsenal::DefaultPrimary();
}

bool UCheeseStrikeLoadouts::HasChosen(const AController* Controller) const
{
	return Controller != nullptr && Chosen.Contains(Controller);
}

bool UCheeseStrikeLoadoutChoice::Choose(FName Weapon, UWorld* World)
{
	const FWeaponStats* Stats = CheeseStrikeArsenal::Find(Weapon);
	if (Stats == nullptr || Stats->Slot != EWeaponSlot::Primary)
	{
		return false;
	}
	Primary = Stats->Id;
	// Told now, if there is a character to tell it through; otherwise the next one tells it as it comes.
	APlayerController* Player = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (ACheeseStrikeCharacter* Character = Player != nullptr ? Cast<ACheeseStrikeCharacter>(Player->GetPawn()) : nullptr)
	{
		if (UCheeseStrikeInventory* Inventory = Character->GetInventory())
		{
			Inventory->ServerChoosePrimary(Primary);
		}
	}
	return true;
}

namespace
{
	/** `CheeseStrikeLoadout <weapon>`: chooses the primary for the next character (or this one, just come in). */
	void ChooseLoadout(const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* Game = World != nullptr ? World->GetGameInstance() : nullptr;
		UCheeseStrikeLoadoutChoice* Choice = Game != nullptr ? Game->GetSubsystem<UCheeseStrikeLoadoutChoice>() : nullptr;
		if (Choice == nullptr)
		{
			return;
		}
		if (Args.Num() == 0)
		{
			UE_LOG(LogCheeseStrike, Display, TEXT("CheeseStrikeLoadout: carrying the %s; choose one of %s"), *Choice->GetPrimary().ToString(), *CheeseStrikeArsenal::ListIds(EWeaponSlot::Primary));
			return;
		}
		if (!Choice->Choose(FName(*Args[0]), World))
		{
			UE_LOG(LogCheeseStrike, Warning, TEXT("CheeseStrikeLoadout: no primary called %s; choose one of %s"), *Args[0], *CheeseStrikeArsenal::ListIds(EWeaponSlot::Primary));
			return;
		}
		UE_LOG(LogCheeseStrike, Display, TEXT("CheeseStrikeLoadout: the %s, from the next time you come in"), *Choice->GetPrimary().ToString());
	}

#if !UE_BUILD_SHIPPING
	/** `CheeseStrikeArm <weapon>` (development): that weapon in hand at once, a primary with the pistol and the knife. */
	void ArmWith(const TArray<FString>& Args, UWorld* World)
	{
		APlayerController* Player = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		ACheeseStrikeCharacter* Character = Player != nullptr ? Cast<ACheeseStrikeCharacter>(Player->GetPawn()) : nullptr;
		if (Character != nullptr && Args.Num() > 0)
		{
			Character->GetInventory()->ServerArm(FName(*Args[0]));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ArmCommand(
		TEXT("CheeseStrikeArm"),
		TEXT("CheeseStrikeArm <weapon>: that weapon in hand at once (development)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ArmWith));
#endif

	FAutoConsoleCommandWithWorldAndArgs LoadoutCommand(
		TEXT("CheeseStrikeLoadout"),
		TEXT("CheeseStrikeLoadout <weapon>: the primary to carry from the next time you come in (blaster, carbine, grater, munster, shotgun, roquefort)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ChooseLoadout));
}
