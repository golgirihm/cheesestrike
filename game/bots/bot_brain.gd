class_name BotBrain
extends RefCounted
## What a computer player decides to do, from what it can see. It is told
## where it is, who is in sight and where things are, and says which way to
## walk, where to look and whether to pull the trigger.
##
## It plays the way a person does, within its skill: it takes a moment to
## notice an enemy, its aim wanders round them and steadies the longer it
## tracks, it fires in bursts and sidesteps as it does, it goes for a wedge of
## cheese when it is hurt, and otherwise it heads for whatever the mode is
## about, or wanders.
##
## Kept free of nodes so its decisions can be tested: the driver (BotDriver)
## looks at the arena for it and moves the character as it says.

const EASY := "easy"
const NORMAL := "normal"
const HARD := "hard"
const SKILLS := [EASY, NORMAL, HARD]
const SKILL_NAMES := {EASY: "Easy", NORMAL: "Normal", HARD: "Hard"}
## For each skill: seconds before it fires at an enemy it has just seen;
## degrees its aim is off at first, and the share of that left once it has
## tracked them a while; shots in a burst, and seconds between bursts; and
## how fast it turns, in radians a second.
const LEVELS := {
	EASY: {"reaction": 0.75, "error": 7.0, "steady": 0.55, "burst": 3, "rest": 0.9, "turn": 3.0},
	NORMAL: {"reaction": 0.45, "error": 4.0, "steady": 0.4, "burst": 5, "rest": 0.55, "turn": 5.0},
	HARD: {"reaction": 0.25, "error": 2.0, "steady": 0.3, "burst": 8, "rest": 0.3, "turn": 8.0},
}
## Seconds of tracking an enemy over which its aim steadies.
const STEADY_TIME := 1.5
## How near a place counts as there.
const ARRIVED := 2.5
## Health below which it goes looking for some.
const HURT := 45
## How close it lets an enemy come before backing off, and the share of its
## weapon's range it likes to fight from.
const TOO_CLOSE := 6.0
const FIGHT_FROM := 0.45
## How far ahead it looks for something in its way, and the turns it tries
## to get round it, nearest first.
const LOOK_AHEAD := 3.0
const WAYS_ROUND := [PI / 4.0, -PI / 4.0, PI / 2.0, -PI / 2.0, PI * 0.75, -PI * 0.75]
## Seconds without getting anywhere after which it is stuck.
const STUCK_AFTER := 1.2

var skill := NORMAL
## Who it is fighting (0 for nobody) and for how long it has had them in sight.
var target := 0
var seen_for := 0.0
## Where it is heading when it has nothing better to do.
var goal: Variant = null
var _rng := RandomNumberGenerator.new()
var _burst_left := 0
var _rest_left := 0.0
var _strafe := 1.0
var _strafe_left := 0.0
var _wander := Vector2.ZERO
var _wander_left := 0.0
var _side := 1.0
var _stuck_for := 0.0
var _last_at := Vector3.INF
var _dodge_left := 0.0


func _init(with_skill := NORMAL, seed := 0) -> void:
	skill = with_skill if LEVELS.has(with_skill) else NORMAL
	_rng.seed = seed


## Decides what to do for the next `delta` seconds.
##
## `me`: {"at": feet, "health", "ammo": rounds in the magazine, "reloading",
## "range": the weapon's reach}.
## `enemies`: each {"id", "at": their feet, "visible"}.
## `places`: {"health": where there are wedges to take, "roam": places worth
## walking to, "objective": where the mode wants this player, or null}.
## `blocked`: Callable(from, to) telling whether the arena is in the way
## between two points at knee height.
##
## Returns {"move": the way to walk (level, a unit long, or zero), "look":
## the point to aim at (null to look where it walks), "fire", "jump",
## "reload", "run"}.
func think(delta: float, me: Dictionary, enemies: Array, places: Dictionary, blocked: Callable) -> Dictionary:
	var level: Dictionary = LEVELS[skill]
	var orders := {"move": Vector3.ZERO, "look": null, "fire": false, "jump": false, "reload": false, "run": false}
	var at: Vector3 = me.at
	_rest_left = maxf(_rest_left - delta, 0.0)
	_dodge_left = maxf(_dodge_left - delta, 0.0)

	var enemy := _choose(at, enemies)
	if enemy.is_empty():
		target = 0
		seen_for = 0.0
	else:
		seen_for = seen_for + delta if enemy.id == target else 0.0
		target = enemy.id

	var hurt: bool = me.health < HURT and not places.get("health", []).is_empty()
	var heading: Variant = null
	if hurt:
		heading = _nearest(at, places.health)
	elif enemy.is_empty():
		heading = places.get("objective")
		if heading == null:
			heading = _roam(at, places.get("roam", []))

	if not enemy.is_empty():
		var distance := at.distance_to(enemy.at)
		orders.look = _aim_point(delta, enemy.at, distance, level)
		if seen_for >= level.reaction and distance <= me.range and not me.reloading and me.ammo > 0:
			orders.fire = _trigger(level)
		if heading == null:
			orders.move = _fight_step(delta, at, enemy.at, distance, me.range)
	if heading != null:
		var way: Vector3 = heading - at
		way.y = 0.0
		if way.length() > ARRIVED * 0.4:
			orders.move = way.normalized()
		orders.run = enemy.is_empty()
	if me.ammo <= 0 and not me.reloading:
		orders.reload = true
	elif enemy.is_empty() and not me.reloading and me.get("magazine", 0) > 0 and me.ammo < me.magazine / 2:
		# A quiet moment is the time to top up.
		orders.reload = true

	if orders.move != Vector3.ZERO:
		var steered := _steer(at, orders.move, blocked)
		orders.jump = steered.jump
		orders.move = steered.move
	_notice_stuck(delta, at, orders)
	return orders


# The enemy to fight: whoever it already is, while they are in sight, or
# else the nearest in sight.
func _choose(at: Vector3, enemies: Array) -> Dictionary:
	var best := {}
	for enemy: Dictionary in enemies:
		if not enemy.visible:
			continue
		if enemy.id == target:
			return enemy
		if best.is_empty() or at.distance_to(enemy.at) < at.distance_to(best.at):
			best = enemy
	return best


# Where to point the weapon to hit someone standing at `feet`: at their
# middle, off by an error that wanders and shrinks as they are tracked.
func _aim_point(delta: float, feet: Vector3, distance: float, level: Dictionary) -> Vector3:
	_wander_left -= delta
	if _wander_left <= 0.0:
		_wander_left = _rng.randf_range(0.2, 0.45)
		_wander = Vector2(_rng.randf_range(-1.0, 1.0), _rng.randf_range(-0.6, 0.6))
	var off := aim_error(skill, seen_for)
	var reach := distance * tan(deg_to_rad(off))
	var middle := feet + Vector3.UP * BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE
	return middle + Vector3(_wander.x, _wander.y, _wander.x * 0.5) * reach


## How far off a bot of `skill` aims, in degrees, having tracked its enemy
## for `tracked` seconds.
static func aim_error(skill: String, tracked: float) -> float:
	var level: Dictionary = LEVELS.get(skill, LEVELS[NORMAL])
	return level.error * lerpf(1.0, level.steady, clampf(tracked / STEADY_TIME, 0.0, 1.0))


# Whether to fire this moment: in bursts, with a rest between.
func _trigger(level: Dictionary) -> bool:
	if _rest_left > 0.0:
		return false
	if _burst_left <= 0:
		_burst_left = level.burst
	_burst_left -= 1
	if _burst_left <= 0:
		_rest_left = level.rest
	return true


# The way to step in a fight: sideways, one way and then the other, closing
# in on an enemy too far off and backing away from one too near.
func _fight_step(delta: float, at: Vector3, enemy: Vector3, distance: float, weapon_range: float) -> Vector3:
	_strafe_left -= delta
	if _strafe_left <= 0.0:
		_strafe_left = _rng.randf_range(0.7, 1.8)
		_strafe = -_strafe
	var toward := enemy - at
	toward.y = 0.0
	if toward.length() < 0.01:
		return Vector3.ZERO
	toward = toward.normalized()
	var step := toward.cross(Vector3.UP) * _strafe
	var wanted := minf(weapon_range * FIGHT_FROM, 25.0)
	if distance > wanted:
		step += toward
	elif distance < TOO_CLOSE:
		step -= toward
	return step.normalized()


# Somewhere to walk to: where it was already going, until it gets there.
func _roam(at: Vector3, spots: Array) -> Variant:
	if goal != null and Vector2(at.x - goal.x, at.z - goal.z).length() <= ARRIVED:
		goal = null
	if goal == null and not spots.is_empty():
		goal = spots[_rng.randi_range(0, spots.size() - 1)]
	return goal


static func _nearest(at: Vector3, spots: Array) -> Vector3:
	var best: Vector3 = spots[0]
	for spot: Vector3 in spots:
		if at.distance_to(spot) < at.distance_to(best):
			best = spot
	return best


# The way to go to get along `wanted` with the arena as it is: straight on
# if nothing is in the way, or else the nearest way round, keeping to the
# side it last went. With no way round, it jumps.
func _steer(at: Vector3, wanted: Vector3, blocked: Callable) -> Dictionary:
	if _dodge_left > 0.0:
		wanted = wanted.rotated(Vector3.UP, _side * PI / 2.0)
	if not blocked.call(at, at + wanted * LOOK_AHEAD):
		return {"move": wanted, "jump": false}
	for turn: float in WAYS_ROUND:
		var way := wanted.rotated(Vector3.UP, turn * _side)
		if not blocked.call(at, at + way * LOOK_AHEAD):
			return {"move": way, "jump": false}
	return {"move": wanted, "jump": true}


# Getting nowhere while trying to: jump, go round the other way, and give
# up on wherever it was wandering to.
func _notice_stuck(delta: float, at: Vector3, orders: Dictionary) -> void:
	if orders.move == Vector3.ZERO or _last_at == Vector3.INF:
		_stuck_for = 0.0
		_last_at = at
		return
	_stuck_for += delta
	if _stuck_for < STUCK_AFTER:
		return
	if at.distance_to(_last_at) < 1.0:
		orders.jump = true
		_side = -_side
		_dodge_left = 0.8
		goal = null
	_stuck_for = 0.0
	_last_at = at
