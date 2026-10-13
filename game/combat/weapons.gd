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
const MAINS := ["blaster", "shotgun", "grater", "carbine", "rifle", "lmg", "bow"]
const SIDES := ["pistol", "pistols"]

## In the order they are stepped through, and that the number keys choose
## among those carried. Added to at the end only: a weapon is sent as its
## place here.
const ORDER := ["pistol", "blaster", "shotgun", "grater", "launcher", "pistols", "carbine", "rifle", "lmg", "bow"]
## How many number keys choose weapons (the actions weapon_1 and up).
const KEYS := 5

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
	"pistols": preload("res://combat/weapons/pistols.tres"),
	"carbine": preload("res://combat/weapons/carbine.tres"),
	"rifle": preload("res://combat/weapons/rifle.tres"),
	"lmg": preload("res://combat/weapons/lmg.tres"),
	"bow": preload("res://combat/weapons/bow.tres"),
	GRENADE: preload("res://combat/weapons/grenade.tres"),
	# The other ways some weapons fire (see WeaponStats.alt).
	"shotgun_both": preload("res://combat/weapons/shotgun_both.tres"),
	"launcher_bounce": preload("res://combat/weapons/launcher_bounce.tres"),
	"blaster_charged": preload("res://combat/weapons/blaster_charged.tres"),
}


## The weapon (or thing thrown) called `id`, or null if there is none.
static func find(id: Variant) -> WeaponStats:
	return _ALL.get(id) if id is String else null


static func ids() -> Array:
	return ORDER.duplicate()


## The stats of the other way `weapon` fires, or null if it has only one.
static func alt_of(weapon: WeaponStats) -> WeaponStats:
	return find(weapon.alt) if weapon != null and not weapon.alt.is_empty() else null


## Every weapon in `slot`, in the usual order.
static func in_slot(slot: String) -> Array:
	return ORDER.filter(func(id: String) -> bool: return find(id).slot == slot)


## A loadout as chosen, put right: [main, side, attachment], with a main
## weapon that isn't one made none (""), a sidearm that isn't one made the
## usual, and an attachment (see Attachments) that isn't one made none.
static func clean_loadout(main: Variant, side: Variant, attachment: Variant = 0) -> Array:
	return [main if main is String and MAINS.has(main) else "", side if side is String and SIDES.has(side) else STARTING, attachment if Attachments.is_kind(attachment) else Attachments.NONE]


## The loadout this player has chosen in their settings, as [main, side,
## attachment].
static func chosen_loadout() -> Array:
	return clean_loadout(Settings.get_value("loadout_main"), Settings.get_value("loadout_side"), Attachments.from_id(Settings.get_value("loadout_attachment")))


## Which of `owned` number key `key` (0 for the first) chooses: they are
## counted off in the usual order, sidearm first. "" if there aren't so many.
static func at_key(owned: Array, key: int) -> String:
	var held := ORDER.filter(func(id: String) -> bool: return owned.has(id))
	return held[key] if key >= 0 and key < held.size() else ""


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
