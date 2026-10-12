class_name MovementCheck
extends RefCounted
## The host's check on one player's movement. Players move themselves and send
## where they are, so a modified game could run faster, rise like a rocket,
## pass through walls or hang in the air. This says whether each update could
## have happened; the host puts the player back if not.
##
## Updates arrive unevenly, so speed is judged with some slack in time and
## distance, and a shove from a burst allows more for a moment.

## Fastest a player can move across the ground, running in an Overload, with
## a margin.
const MAX_SPEED := Movement.WALK_SPEED * Movement.RUN_FACTOR * Overload.SPEED_BOOST * 1.15
const MAX_RISE_SPEED := Movement.JUMP_VELOCITY * 1.15
## Slack for updates bunched up on the network.
const SLACK_TIME := 0.15
const SLACK_DISTANCE := 0.6
## How long after a shove extra speed is allowed, and how much.
const PUSH_WINDOW := 1.5
## Longest a player may stay well above the ground: a burst can throw them
## high, but nothing keeps them up.
const MAX_AIRTIME := 3.5
## How close a player must come back to where they were sent, after being
## moved at once.
const PLACE_TOLERANCE := 2.0

var _last_time := -1.0
var _last_position := Vector3.ZERO
var _last_teleports := 0
var _push_until := -1.0
var _push_speed := 0.0
var _airborne_since := -1.0
# Where the host has sent the player, if it is waiting for them to arrive.
var _expected: Variant = null


## Where the player was last seen doing something possible.
func last_position() -> Vector3:
	return _last_position


## The host has sent the player to `at` (on coming back, or being put back),
## which their next jump to a new place should match.
func expect_move(at: Vector3) -> void:
	_expected = at


## A burst shoved the player at `time` by `impulse`.
func pushed(time: float, impulse: Vector3) -> void:
	_push_until = time + PUSH_WINDOW
	_push_speed = maxf(_push_speed if time < _push_until else 0.0, impulse.length())


## Whether the update placing the player at `at` at time `time` (seconds, the
## host's clock), having been moved at once `teleports` times, could have
## happened. `blocked` is a Callable(from, to) telling whether the arena
## stands between two points, and `on_ground` whether the player is standing.
func check(time: float, at: Vector3, teleports: int, blocked: Callable, on_ground: bool) -> bool:
	if not at.is_finite():
		return false
	if _last_time < 0.0:
		_accept(time, at, teleports, on_ground)
		return true
	if teleports != _last_teleports:
		if _expected == null or (at as Vector3).distance_to(_expected) > PLACE_TOLERANCE:
			return false
		_expected = null
		_accept(time, at, teleports, true)
		return true
	var elapsed := maxf(time - _last_time, 0.0) + SLACK_TIME
	var shoved := _push_speed if time < _push_until else 0.0
	var across := Vector2(at.x - _last_position.x, at.z - _last_position.z).length()
	if across > (MAX_SPEED + shoved) * elapsed + SLACK_DISTANCE:
		return false
	var rise := at.y - _last_position.y
	if rise > (MAX_RISE_SPEED + shoved) * elapsed + SLACK_DISTANCE:
		return false
	if blocked.call(_last_position, at):
		return false
	if not on_ground and _airborne_since >= 0.0 and time - _airborne_since > MAX_AIRTIME:
		return false
	_accept(time, at, teleports, on_ground)
	return true


func _accept(time: float, at: Vector3, teleports: int, on_ground: bool) -> void:
	_last_time = time
	_last_position = at
	_last_teleports = teleports
	if on_ground:
		_airborne_since = -1.0
	elif _airborne_since < 0.0:
		_airborne_since = time
