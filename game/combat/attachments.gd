class_name Attachments
## What a player can fit to the main weapon of their loadout: one thing, each
## a small gain for a small cost. A fitted weapon is the same weapon with a
## few of its numbers changed; the host works them out for itself from what
## the player chose.

const NONE := 0
## Closer and tighter down the sights; wilder from the hip.
const SIGHT := 1
## Less kick; a little slower on the feet.
const GRIP := 2
## More rounds in the magazine; longer to reload.
const MAGAZINE := 3
const KINDS := [NONE, SIGHT, GRIP, MAGAZINE]
## What each is called in the settings, in the order they are offered.
const IDS := ["none", "sight", "grip", "magazine"]
const NAMES := {NONE: "None", SIGHT: "Sight", GRIP: "Grip", MAGAZINE: "Long magazine"}
const NOTES := {
	NONE: "",
	SIGHT: "closer and tighter aimed, wilder from the hip",
	GRIP: "less kick, a little slower",
	MAGAZINE: "more rounds, a longer reload",
}
const SIGHT_ZOOM := 0.8
const SIGHT_SPREAD := 0.8
const SIGHT_HIP := 1.15
const GRIP_KICK := 0.75
const GRIP_SPEED := 0.95
const MAGAZINE_ROUNDS := 1.3
const MAGAZINE_RELOAD := 1.2

# Fitted weapons already made, by weapon id and attachment, so that the same
# one is always given.
static var _fitted := {}


static func is_kind(kind: Variant) -> bool:
	return kind is int and KINDS.has(kind)


## The attachment called `id` in the settings, or NONE.
static func from_id(id: Variant) -> int:
	return maxi(IDS.find(id), 0) if id is String else NONE


## What the settings say of attachment `kind`, such as "Grip (less kick, a
## little slower)".
static func text(kind: int) -> String:
	return NAMES[kind] if NOTES[kind].is_empty() else "%s (%s)" % [NAMES[kind], NOTES[kind]]


## `weapon` with attachment `kind` fitted: the weapon itself for none, or a
## copy with only what the attachment changes changed.
static func fitted(weapon: WeaponStats, kind: int) -> WeaponStats:
	if weapon == null or kind == NONE or not is_kind(kind):
		return weapon
	var key := "%s:%d" % [weapon.id, kind]
	if not _fitted.has(key):
		var made: WeaponStats = weapon.duplicate()
		match kind:
			SIGHT:
				made.aim_zoom = weapon.aim_zoom * SIGHT_ZOOM
				made.spread_degrees = weapon.spread_degrees * SIGHT_SPREAD
				made.hip_spread_degrees = weapon.hip_spread_degrees * SIGHT_HIP
			GRIP:
				made.recoil_up = weapon.recoil_up * GRIP_KICK
				made.recoil_side = weapon.recoil_side * GRIP_KICK
				made.carry_speed = weapon.carry_speed * GRIP_SPEED
			MAGAZINE:
				made.magazine = ceili(weapon.magazine * MAGAZINE_ROUNDS)
				made.reload_time = weapon.reload_time * MAGAZINE_RELOAD
		_fitted[key] = made
	return _fitted[key]
