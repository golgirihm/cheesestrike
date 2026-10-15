class_name MovementCheck
extends RefCounted
## The host's check on one player's movement. Players move themselves and send
## where they are, so a modified game could run faster, rise like a rocket,
## pass through walls or hang in the air. This says whether each update could
## have happened; the host puts the player back if not.
##
## Updates arrive unevenly, so there is some slack in time and distance, and
## a shove from a burst allows more for a moment. The slack is a reserve,
## not an allowance on every update: a player has only as far to go as the
## time that has really passed gives them, plus what is left of the reserve.
## Without that, a game sending many small updates close together could
## spend the slack on each, and go several times as fast as anyone can.

## Fastest a player can move across the ground, at the start of a slide from
## a run in an Overload, with a margin.
const MAX_SPEED := Movement.WALK_SPEED * Movement.RUN_FACTOR * Movement.SLIDE_BOOST * Overload.SPEED_BOOST * 1.15
const MAX_RISE_SPEED := Movement.JUMP_VELOCITY * 1.15
## Slack for updates bunched up on the network.
const SLACK_TIME := 0.15
## How long a player's unused time is kept for them. Updates held up on the
## way arrive together, each with almost no time since the last, and need
## the time that went by while they were held up; but no more than this.
const KEPT_TIME := 1.0
const SLACK_DISTANCE := 0.6
## How long after a shove extra speed is allowed, and how much.
const PUSH_WINDOW := 1.5
## Longest a player may stay well above the ground: a burst can throw them
## high, but nothing keeps them up.
const MAX_AIRTIME := 3.5
## How close a player must come back to where they were sent, after being
## moved at once.
const PLACE_TOLERANCE := 2.0

## How much faster than usual the host's rules let players move, and how
## much longer they let them stay in the air.
var speed_scale := 1.0
var air_scale := 1.0

var _last_time := -1.0
var _last_position := Vector3.ZERO
var _last_teleports := 0
var _push_until := -1.0
var _push_speed := 0.0
var _airborne_since := -1.0
# How far the player may yet go across the ground, and up, before the next
# update: what time has allowed them and they haven't used.
var _reach := 0.0
var _lift := 0.0
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
	var elapsed := maxf(time - _last_time, 0.0)
	var shoved := _push_speed if time < _push_until else 0.0
	var speed := MAX_SPEED * maxf(speed_scale, 1.0) + shoved
	var rise_speed := MAX_RISE_SPEED + shoved
	# What the time since the last update allows, on top of what was left,
	# and never more than a moment's worth kept from before.
	var reach := minf(_reach, kept(speed)) + speed * elapsed
	var lift := minf(_lift, kept(rise_speed)) + rise_speed * elapsed
	var across := Vector2(at.x - _last_position.x, at.z - _last_position.z).length()
	if across > reach:
		return false
	var rise := at.y - _last_position.y
	if rise > lift:
		return false
	if blocked.call(_last_position, at):
		return false
	if not on_ground and _airborne_since >= 0.0 and time - _airborne_since > MAX_AIRTIME * maxf(air_scale, 1.0):
		return false
	_reach = reach - across
	_lift = lift - maxf(rise, 0.0)
	_accept(time, at, teleports, on_ground)
	return true


## The most a player moving at `speed` can have in hand: the slack for
## updates bunched up on the way.
## The most a player moving at `speed` can keep of the time they didn't use.
static func kept(speed: float) -> float:
	return speed * KEPT_TIME + SLACK_DISTANCE


static func reserve(speed: float) -> float:
	return speed * SLACK_TIME + SLACK_DISTANCE


func _accept(time: float, at: Vector3, teleports: int, on_ground: bool) -> void:
	# From the first update, and after being moved at once, the reserve is
	# full.
	if _last_time < 0.0 or teleports != _last_teleports:
		_reach = reserve(MAX_SPEED * maxf(speed_scale, 1.0))
		_lift = reserve(MAX_RISE_SPEED)
	_last_time = time
	_last_position = at
	_last_teleports = teleports
	if on_ground:
		_airborne_since = -1.0
	elif _airborne_since < 0.0:
		_airborne_since = time
