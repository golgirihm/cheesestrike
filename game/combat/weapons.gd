class_name Weapons
## Every weapon in the game, by id.

## The sidearm a player has until they choose another, and whatever else
## they choose or find. It is weak, and never runs out.
const STARTING := "pistol"
## The main weapon a player has until they choose another.
const DEFAULT_MAIN := "blaster"

## The slots a weapon can be in (see WeaponStats.slot).
const MAIN := "main"
const SIDE := "side"
const POWER := "power"
## The weapons a loadout can have in each slot, in the order they are
## offered. Kept by hand, and checked against each weapon's own slot.
const MAINS := ["blaster", "shotgun", "grater"]
const SIDES := ["pistol"]

## In the order the number keys choose them.
const ORDER := ["pistol", "blaster", "shotgun", "grater", "launcher"]

## What is thrown rather than held: carried by everyone, beside their weapons.
const GRENADE := "grenade"
## How many grenades a player comes into play with, which is the most they
## can carry.
const GRENADES := 2

const _ALL := {
	"pistol": preload("res://combat/weapons/pistol.tres"),
	"blaster": preload("res://combat/weapons/blaster.tres"),
	"shotgun": preload("res://combat/weapons/shotgun.tres"),
	"grater": preload("res://combat/weapons/grater.tres"),
	"launcher": preload("res://combat/weapons/launcher.tres"),
	GRENADE: preload("res://combat/weapons/grenade.tres"),
}


## The weapon (or thing thrown) called `id`, or null if there is none.
static func find(id: Variant) -> WeaponStats:
	return _ALL.get(id) if id is String else null


static func ids() -> Array:
	return ORDER.duplicate()


## Every weapon in `slot`, in the usual order.
static func in_slot(slot: String) -> Array:
	return ORDER.filter(func(id: String) -> bool: return find(id).slot == slot)


## A loadout as chosen, put right: [main, side], with a main weapon that
## isn't one made none ("") and a sidearm that isn't one made the usual.
static func clean_loadout(main: Variant, side: Variant) -> Array:
	return [main if main is String and MAINS.has(main) else "", side if side is String and SIDES.has(side) else STARTING]


## The loadout this player has chosen in their settings, as [main, side].
static func chosen_loadout() -> Array:
	return clean_loadout(Settings.get_value("loadout_main"), Settings.get_value("loadout_side"))


## The weapon after (or with `step` -1, before) `current` among `owned`, in
## the usual order, coming round at the end.
static func cycle(owned: Array, current: String, step := 1) -> String:
	var held := ORDER.filter(func(id: String) -> bool: return owned.has(id))
	if held.is_empty():
		return current
	var at := held.find(current)
	if at == -1:
		return held[0]
	return held[posmod(at + step, held.size())]
