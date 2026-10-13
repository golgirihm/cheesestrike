class_name CaptureRules
extends RefCounted
## Capture the cheese wheel. Each team has a base with a wheel of cheese on
## it. A player who reaches the other team's wheel picks it up and carries
## it on their back, slowed by its weight; brought to their own base while
## their own wheel is at home, it scores. A carrier who is knocked out drops
## the wheel where they stood: the other team takes it up again by touching
## it, and its own team sends it home by touching it, or by leaving it
## alone long enough.
##
## Kept free of nodes so the rules can be tested alone: the host tells it
## where everyone stands, and acts on what it says happened.

const HOME := 0
const CARRIED := 1
const DROPPED := 2
## How close a player's feet must come to a wheel to take it or send it
## home, and to their base to score with one. Further than two players
## standing against each other are apart, so that nobody can stand on a
## wheel to keep it.
const REACH := 3.0
const BASE_REACH := 3.5
## Seconds a dropped wheel lies before it goes home by itself.
const RETURN_AFTER := 20.0
## The share of their usual speed a carrier moves at.
const CARRY_SPEED := 0.8

const TAKEN := "taken"
const FELL := "dropped"
const RETURNED := "returned"
const CAPTURED := "captured"

## Each team's wheel, Cheddar's first: {"status", "carrier": the id of
## whoever has it (0 for nobody), "at": where it is, "left": seconds until a
## dropped one goes home}.
var wheels: Array[Dictionary] = []


func _init() -> void:
	reset([Vector3.ZERO, Vector3.ZERO])


## Puts both wheels at home, on `bases` (Cheddar's first).
func reset(bases: Array) -> void:
	wheels.clear()
	for team in 2:
		wheels.append({"status": HOME, "carrier": 0, "at": bases[team], "left": 0.0})


## Which team's wheel player `id` is carrying, or -1.
func carried_by(id: int) -> int:
	for team in wheels.size():
		if wheels[team].status == CARRIED and wheels[team].carrier == id:
			return team
	return -1


## Moves on by `delta` seconds. `bases` are the teams' bases, Cheddar's
## first; `standing` maps each player's id to {"at": their feet, "team",
## "alive"}. Returns what happened, in order: each {"type": TAKEN, FELL,
## RETURNED or CAPTURED, "wheel": the team whose wheel it was, "by": the
## player's id, or 0 for nobody}.
func update(delta: float, bases: Array, standing: Dictionary) -> Array[Dictionary]:
	var events: Array[Dictionary] = []
	var ids := standing.keys()
	ids.sort()
	for team in wheels.size():
		var wheel: Dictionary = wheels[team]
		if wheel.status == HOME:
			wheel.at = bases[team]
		elif wheel.status == CARRIED:
			var carrier: Variant = standing.get(wheel.carrier)
			if carrier == null or not carrier.alive:
				# Where it was last seen.
				events.append({"type": FELL, "wheel": team, "by": wheel.carrier})
				wheel.status = DROPPED
				wheel.carrier = 0
				wheel.left = RETURN_AFTER
			else:
				wheel.at = carrier.at
				# Home with it, to a base whose own wheel is there.
				if _near(carrier.at, bases[carrier.team], BASE_REACH) and wheels[carrier.team].status == HOME:
					events.append({"type": CAPTURED, "wheel": team, "by": wheel.carrier})
					_send_home(wheel, bases[team])
			continue
		elif wheel.status == DROPPED:
			wheel.left -= delta
			if wheel.left <= 0.0:
				events.append({"type": RETURNED, "wheel": team, "by": 0})
				_send_home(wheel, bases[team])
				continue
		for id: int in ids:
			var player: Dictionary = standing[id]
			if not player.alive or player.team < 0 or not _near(player.at, wheel.at, REACH):
				continue
			if player.team == team:
				if wheel.status == DROPPED:
					events.append({"type": RETURNED, "wheel": team, "by": id})
					_send_home(wheel, bases[team])
					break
			elif carried_by(id) == -1:
				events.append({"type": TAKEN, "wheel": team, "by": id})
				wheel.status = CARRIED
				wheel.carrier = id
				wheel.at = player.at
				break
	return events


func _send_home(wheel: Dictionary, base: Vector3) -> void:
	wheel.status = HOME
	wheel.carrier = 0
	wheel.at = base
	wheel.left = 0.0


static func _near(a: Vector3, b: Vector3, reach: float) -> bool:
	return Vector2(a.x - b.x, a.z - b.z).length() <= reach and absf(a.y - b.y) <= 3.0


## What is sent of it, and the rules that makes, with nonsense left out.
func to_array() -> Array:
	return wheels.map(func(wheel: Dictionary) -> Array: return [wheel.status, wheel.carrier, wheel.at, wheel.left])


static func from_array(data: Variant) -> CaptureRules:
	var rules := CaptureRules.new()
	if not data is Array or data.size() != 2:
		return rules
	for team in 2:
		var entry: Variant = data[team]
		if not entry is Array or entry.size() != 4 or not entry[0] is int or not entry[1] is int or not entry[2] is Vector3 or not entry[2].is_finite():
			continue
		if not [HOME, CARRIED, DROPPED].has(entry[0]) or not (entry[3] is float or entry[3] is int) or not is_finite(float(entry[3])):
			continue
		rules.wheels[team] = {"status": entry[0], "carrier": entry[1], "at": entry[2], "left": clampf(float(entry[3]), 0.0, RETURN_AFTER)}
	return rules
