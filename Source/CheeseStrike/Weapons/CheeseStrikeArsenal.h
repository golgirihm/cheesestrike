// Copyright (c) 2026 Hamid Golgiri. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CheeseStrikeShot.h"

/**
 * Every weapon in the game, as plain numbers (FWeaponStats), and the lists
 * the game is played by: what a loadout chooses from, and Gun Game's ladder.
 * The numbers are the design's table (docs/unreal/design.md, "Weapons"),
 * its lengths divided by three for life size. Wave 14 brought projectiles
 * (the BabyBomb's wheels, the Cheese Grater's shards: ACheeseStrikeProjectile)
 * and a melee primary (the Cheese Stick).
 */
namespace CheeseStrikeWeapons
{
	/** Primaries. The Cheddar Blaster (Blaster(), in CheeseStrikeShot.h) is the automatic rifle. */
	/** The Colby Carbine: three-round bursts, a click a burst; the weak primary everyone comes in with. */
	const FWeaponStats& Carbine();
	/** The Cheese Grater (the Parmesan Grater's id, `grater`): a needler, homing shards; sixteen in one player burst for a kill. */
	const FWeaponStats& Grater();
	/** The Munster Twin SMGs: two submachine guns, one in each hand, automatic, firing in turn, a belt of 60 each, no sights. */
	const FWeaponStats& Munster();
	/** The Swiss Shotgun: twelve pellets a shot, a kill up to about 4 m, weak past 8. */
	const FWeaponStats& Shotgun();
	/** The Dual Burratas: two cheese pistols, one in each hand, firing in turn, a shot a pull, fast; a long magazine and a long reload. */
	const FWeaponStats& Burratas();
	/** The Roquefort Rifle: a scoped rifle, a head shot a kill. */
	const FWeaponStats& Roquefort();
	/** The BabyBomb: a power weapon, a launcher of red-waxed wheels that burst, its shooter not spared. */
	const FWeaponStats& BabyBomb();
	/** The Cheese Stick: a melee power weapon, a slow swing that fells everyone in a wide arc in front. */
	const FWeaponStats& Stick();
	/** The Curd Pistol: the weak sidearm everyone carries, with rounds without end. */
	const FWeaponStats& Pistol();
	/** The Cheese Knife: a short swipe across the aim, deadly in the back. */
	const FWeaponStats& Knife();
	/** The Gouda Gatling: the Gouda Overload's minigun of molten cheese (bSpecial), spun up, 200 rounds and no reload. */
	const FWeaponStats& Gatling();
}

namespace CheeseStrikeArsenal
{
	/** Every weapon, in the order a picker lists them: the primaries, the sidearm, the knife; then the streaks' own (bSpecial), which no picker lists. */
	const TArray<const FWeaponStats*>& All();
	/** The weapon whose id is `Id` (any case), or null. */
	const FWeaponStats* Find(FName Id);
	/** The ids of the weapons that go in `Slot` that a player may pick, in order: never a special one. */
	TArray<FName> InSlot(EWeaponSlot Slot);
	/** Whether `Id` is a weapon a player may pick for `Slot` (a loadout, a stand): one of InSlot's. */
	bool IsPickable(FName Id, EWeaponSlot Slot);
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
