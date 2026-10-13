class_name Weapons
## Every weapon in the game, by id.

## The sidearm every player starts with, and keeps when knocked out. It is
## weak: the rest are found on the arena's pads.
const STARTING := "pistol"

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
