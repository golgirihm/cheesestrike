class_name Pings
extends Node3D
## Pings: a player points at something and presses, and their teammates see
## a marker there: on a place, on a pickup, or on an opponent. The host
## passes each on to whoever should see it, and no more than one a second
## from anyone.
##
## Where there are no teams a ping is the pinger's own: there is nobody to
## tell without telling an opponent.

const PLACE := "place"
const PICKUP := "pickup"
const ENEMY := "enemy"
const KINDS := [PLACE, PICKUP, ENEMY]
const COLOURS := {PLACE: Color(0.45, 0.85, 1.0), PICKUP: Color(0.5, 1.0, 0.55), ENEMY: Color(1.0, 0.35, 0.3)}
const WORDS := {PLACE: "Here", PICKUP: "Pickup here", ENEMY: "Enemy here"}
## Seconds a ping is shown for, and seconds a player waits between pings.
const LIFE := 6.0
const GAP := 1.0
## How far a player can point, and how near a pickup the point must be to
## be a ping on it, in metres.
const REACH := 150.0
const PICKUP_WITHIN := 2.0
## Further than this from the middle of the arena is nowhere a ping can be.
const BOUNDS := 400.0

var world: Node3D
# The pings showing: {"by", "at", "kind", "left", "node"}.
var _showing: Array[Dictionary] = []


func _init(of: Node3D = null) -> void:
	world = of
	name = "Pings"


func _ready() -> void:
	if world != null:
		world.match_.pinged.connect(add)


## What a ping on `at` is a ping on: an opponent if it was one that was
## pointed at (`on_opponent`), a pickup if one lies near (`pads` is where
## they are), and otherwise the place.
static func kind_at(at: Vector3, on_opponent: bool, pads: Array) -> String:
	if on_opponent:
		return ENEMY
	for pad: Vector3 in pads:
		if pad.distance_to(at) <= PICKUP_WITHIN:
			return PICKUP
	return PLACE


## Whether the player with record `viewer` sees a ping by the one with
## record `pinger`, in a match with `teams` or without.
static func seen_by(pinger: PlayerRecord, viewer: PlayerRecord, teams: bool) -> bool:
	if pinger == null or viewer == null:
		return false
	if pinger == viewer:
		return true
	return teams and pinger.team != MatchState.NO_TEAM and pinger.team == viewer.team


## Whether what was sent as a ping is one: a place within the arena and a
## known kind.
static func is_ping(at: Variant, kind: Variant) -> bool:
	return at is Vector3 and at.is_finite() and at.length() <= BOUNDS and kind is String and KINDS.has(kind)


## Whether a player whose last ping was `since` seconds ago may ping again.
static func may_ping(since: float) -> bool:
	return since >= GAP


## Player `by` pinged `at`, as a ping of `kind`: a marker is put there, in
## place of any they had, with a sound.
func add(by: int, at: Vector3, kind: String) -> void:
	for ping in _showing:
		if ping.by == by:
			ping.left = 0.0
	var marker := Label3D.new()
	marker.text = "▼"
	marker.font_size = 96
	marker.outline_size = 16
	marker.modulate = COLOURS.get(kind, COLOURS[PLACE])
	marker.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	marker.no_depth_test = true
	marker.fixed_size = true
	marker.pixel_size = 0.0006
	add_child(marker)
	marker.global_position = at + Vector3.UP * 1.2
	_showing.append({"by": by, "at": at, "kind": kind, "left": LIFE, "node": marker})
	Sounds.play("ping", -6.0)


## The pings showing: [{"by", "at", "kind", "colour"}].
func shown() -> Array:
	return _showing.filter(func(ping: Dictionary) -> bool: return ping.left > 0.0).map(func(ping: Dictionary) -> Dictionary:
		return {"by": ping.by, "at": ping.at, "kind": ping.kind, "colour": COLOURS[ping.kind]})


## Clears every ping away.
func clear() -> void:
	for ping in _showing:
		ping.node.queue_free()
	_showing.clear()


func _process(delta: float) -> void:
	for ping in _showing:
		ping.left -= delta
		# Fading over its last second.
		ping.node.modulate.a = clampf(ping.left, 0.0, 1.0)
		if ping.left <= 0.0:
			ping.node.queue_free()
	_showing = _showing.filter(func(ping: Dictionary) -> bool: return ping.left > 0.0)
