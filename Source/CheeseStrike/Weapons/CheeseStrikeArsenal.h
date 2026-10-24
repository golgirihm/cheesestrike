// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CheeseStrikeShot.h"

/**
 * Every weapon in the game, as plain numbers (FWeaponStats), and the lists
 * the game is played by: what a loadout chooses from, and Gun Game's ladder.
 * The numbers are the design's table (docs/unreal/design.md, "Weapons"),
 * its lengths divided by three for life size; projectile weapons and the
 * Twin Curds are still to come.
 */
namespace CheeseStrikeWeapons
{
	/** Primaries. The Cheddar Blaster (Blaster(), in CheeseStrikeShot.h) is the automatic rifle. */
	/** The Colby Carbine: three-round bursts, a click a burst; the weak primary everyone comes in with. */
	const FWeaponStats& Carbine();
	/** The Parmesan Grater: a submachine gun, fast and close. */
	const FWeaponStats& Grater();
	/** The Munster Machine Gun: a long belt, slow to draw and heavy to carry. */
	const FWeaponStats& Munster();
	/** The Swiss Shotgun: nine pellets a shot. */
	const FWeaponStats& Shotgun();
	/** The Dual Burratas: two cheese pistols, one in each hand, firing in turn, a shot a pull, fast; a long magazine and a long reload. */
	const FWeaponStats& Burratas();
	/** The Roquefort Rifle: a scoped rifle, a head shot a kill. */
	const FWeaponStats& Roquefort();
	/** The Curd Pistol: the weak sidearm everyone carries, with rounds without end. */
	const FWeaponStats& Pistol();
	/** The Cheese Knife: a short swipe across the aim, deadly in the back. */
	const FWeaponStats& Knife();
}

namespace CheeseStrikeArsenal
{
	/** Every weapon, in the order a picker lists them: the primaries, the sidearm, the knife. */
	const TArray<const FWeaponStats*>& All();
	/** The weapon whose id is `Id` (any case), or null. */
	const FWeaponStats* Find(FName Id);
	/** The ids of the weapons that go in `Slot`, in order. */
	TArray<FName> InSlot(EWeaponSlot Slot);
	/** The primary everyone comes in with (the Colby Carbine, the weakest: the maps' stands hold the rest), the sidearm and the knife. */
	FName DefaultPrimary();
	FName Sidearm();
	FName Knife();
	/** The ids, joined for a message: "blaster, carbine, ...". */
	FString ListIds(EWeaponSlot Slot);

	/**
	 * Gun Game's ladder: each kill moves a player to the next weapon; the
	 * hardest to finish with come last, and the last of all is the knife
	 * (docs/unreal/design.md, "Modes").
	 */
	const TArray<FName>& GunGameLadder();
	/** The weapon a Gun Game player with `Kills` kills carries: the last for any more. */
	FName GunGameWeapon(int32 Kills);
}
