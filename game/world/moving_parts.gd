class_name MovingParts
extends RefCounted
## The parts of an arena that hurt or move, all decided by the host: hazards
## that hurt whoever stands in them, doors that open for anyone who comes
## near, and lifts that go up and down by the clock.
##
## An arena lists its own as Marker3Ds: under "Hazards", each with a `kind`
## and a `size` (how far it reaches across, as a Vector3 of which x and z
## count); under "Doors", each with the `size` of the door and the `slide`
## that takes it out of the way when open; under "Lifts", each at the low
## end of its travel with the `size` of its platform and how far it goes up
## as its `rise`. Scenery draws them and makes the doors and platforms
## solid; the world moves them.
##
## Kept free of nodes: the host tells it where everyone stands.

## A floor of grater teeth, and a vat of fondue: what each is called when it
## knocks a player out, the damage a second it does, and the share of their
## speed a player wading through it keeps (besides what being hurt costs
## them, as any hurt does).
const GRATER := "the_grater"
const FONDUE := "fondue"
const HAZARD_NAMES := {GRATER: "The Grater", FONDUE: "Fondue"}
const DAMAGE := {GRATER: 30.0, FONDUE: 45.0}
const SLOW := {GRATER: 1.0, FONDUE: 0.8}
## Seconds between one hurt from a hazard and the next.
const HURT_EVERY := 0.5
## How near a door's middle a player must come for it to open, and seconds
## it stays open after the last has gone.
const DOOR_REACH := 6.0
const DOOR_HOLD := 1.5
## How fast a door slides, and a lift moves, in metres a second; and seconds
## a lift waits at each end.
const DOOR_SPEED := 8.0
const LIFT_SPEED := 2.5
const LIFT_WAIT := 3.0

# Player id -> seconds they have stood in a hazard since it last hurt them.
var _in_hazard := {}
# For each door, seconds since anyone was near it.
var _door_empty: Array[float] = []


## What an arena has, read from its scene: {"hazards": [{position, size,
## kind}], "doors": [{position, size, slide}], "lifts": [{position, size,
## rise}]}.
static func of(map: Node) -> Dictionary:
	var found := {"hazards": [], "doors": [], "lifts": []}
	for kind: String in ["Hazards", "Doors", "Lifts"]:
		var holder := map.get_node_or_null(kind)
		if holder == null:
			continue
		for marker: Node3D in holder.get_children():
			var part := {"position": marker.global_position if marker.is_inside_tree() else marker.position, "size": marker.get_meta("size", Vector3(4, 1, 4))}
			if kind == "Hazards":
				part["kind"] = String(marker.get_meta("kind", GRATER))
			elif kind == "Doors":
				part["slide"] = marker.get_meta("slide", Vector3.DOWN * 5.0)
			else:
				part["rise"] = float(marker.get_meta("rise", 3.0))
			found[kind.to_lower()].append(part)
	return found


## The one of `hazards` a player whose feet are at `feet` is standing in,
## or {}: within its reach across, and on it rather than in the air over it.
static func hazard_at(hazards: Array, feet: Vector3) -> Dictionary:
	for hazard: Dictionary in hazards:
		var off: Vector3 = feet - hazard.position
		if absf(off.x) <= hazard.size.x / 2.0 and absf(off.z) <= hazard.size.z / 2.0 and off.y >= -0.6 and off.y <= 0.8:
			return hazard
	return {}


## The share of their speed a player whose feet are at `feet` keeps.
static func speed_factor(hazards: Array, feet: Vector3) -> float:
	var hazard := hazard_at(hazards, feet)
	return SLOW.get(hazard.get("kind", ""), 1.0) if not hazard.is_empty() else 1.0


## Moves on by `delta` seconds. `standing` maps each player's id to {"at":
## their feet, "alive"}. Returns who is hurt now, each {"id", "kind",
## "amount", "at"}: every HURT_EVERY seconds a player has stood in a hazard.
func hurt(delta: float, hazards: Array, standing: Dictionary) -> Array[Dictionary]:
	var hurts: Array[Dictionary] = []
	for id: int in _in_hazard.keys():
		if not standing.has(id):
			_in_hazard.erase(id)
	for id: int in standing:
		var player: Dictionary = standing[id]
		var hazard := hazard_at(hazards, player.at) if player.alive else {}
		if hazard.is_empty():
			_in_hazard.erase(id)
			continue
		# The first hurt comes at once, on stepping in.
		var stood: float = _in_hazard.get(id, HURT_EVERY) + delta
		if stood >= HURT_EVERY:
			stood -= HURT_EVERY
			hurts.append({"id": id, "kind": hazard.kind, "amount": DAMAGE[hazard.kind] * HURT_EVERY, "at": hazard.position})
		_in_hazard[id] = stood
	return hurts


## Which of `doors` are open, after `delta` more seconds: open while any
## player in play is near, and for a moment after. `open` is how they were,
## and is returned changed where they have.
func doors_open(delta: float, doors: Array, standing: Dictionary, open: Array) -> Array:
	var now := []
	_door_empty.resize(doors.size())
	for index in doors.size():
		var door: Dictionary = doors[index]
		var near := false
		for player: Dictionary in standing.values():
			var off: Vector3 = player.at - door.position
			near = near or (player.alive and Vector2(off.x, off.z).length() <= DOOR_REACH and absf(off.y) <= 6.0)
		_door_empty[index] = 0.0 if near else _door_empty[index] + delta
		var was: bool = index < open.size() and open[index]
		now.append(near or (was and _door_empty[index] < DOOR_HOLD))
	return now


## How far up a lift that rises `rise` is, `time` seconds into the session:
## it waits at the bottom, goes up, waits at the top and comes down, for ever.
static func lift_height(time: float, rise: float) -> float:
	if rise <= 0.0:
		return 0.0
	var travel := rise / LIFT_SPEED
	var into := fmod(maxf(time, 0.0), (LIFT_WAIT + travel) * 2.0)
	if into < LIFT_WAIT:
		return 0.0
	into -= LIFT_WAIT
	if into < travel:
		return into * LIFT_SPEED
	into -= travel
	if into < LIFT_WAIT:
		return rise
	return rise - (into - LIFT_WAIT) * LIFT_SPEED
