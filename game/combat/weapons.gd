class_name Weapons
## Every weapon in the game, by id.

const STARTING := "blaster"

const _ALL := {
	"blaster": preload("res://combat/weapons/blaster.tres"),
}


## The weapon called `id`, or null if there is none.
static func find(id: Variant) -> WeaponStats:
	return _ALL.get(id) if id is String else null


static func ids() -> Array:
	return _ALL.keys()
