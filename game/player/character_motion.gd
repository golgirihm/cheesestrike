class_name CharacterMotion
## What a character is doing, worked out from how its player is moving, and
## how it holds its limbs for each. A character is always in one state of
## its legs (standing, walking, running, crouched, sliding, in the air,
## landing) and may be doing one thing with its free arm on top of that
## (steadying a weapon to aim or reload, throwing, flinching).
##
## The character blends from one pose to the next rather than snapping, so
## each pose here is where the limbs are heading, not where they are. Only
## the limbs move: the head and torso stay put, and shots are tested against
## the legs as they stand, as in most games.

const IDLE := "idle"
const WALK := "walk"
const RUN := "run"
const CROUCH := "crouch"
const CROUCH_WALK := "crouch_walk"
const SLIDE := "slide"
const JUMP := "jump"
const FALL := "fall"
const LAND := "land"

const NONE := "none"
const AIM := "aim"
const FIRE := "fire"
const RELOAD := "reload"
const THROW := "throw"
const MELEE := "melee"
const HURT := "hurt"
## What the free arm is doing, the first that applies winning.
const ACTIONS := [HURT, THROW, MELEE, RELOAD, FIRE, AIM]
## Seconds each momentary action lasts. The rest last while they are so.
const ACTION_TIME := {HURT: 0.3, THROW: 0.45, MELEE: 0.3, FIRE: 0.15}

## Slower than this across the ground is standing still; faster than
## RUN_FROM of walking speed is running.
const MOVING := 0.5
const RUN_FROM := 1.2
## Seconds a landing shows for.
const LAND_TIME := 0.18
## Seconds the limbs take to get most of the way to a new pose.
const BLEND_TIME := 0.09

## Where the limbs head for in each state:
##   stride  how far the legs swing as they walk, as a share of the usual
##   tuck    how far the legs are drawn up, 0 to 1
##   reach   how far the legs are thrown forward, 0 to 1, as in a slide
##   splay   how far the legs are spread, in radians
##   lift    how far the free arm is raised out from the side, in radians
##   pump    how far the free arm swings with the stride, as a share
const POSES := {
	IDLE: {"stride": 0.0, "tuck": 0.0, "reach": 0.0, "splay": 0.0, "lift": 0.0, "pump": 0.0},
	WALK: {"stride": 1.0, "tuck": 0.0, "reach": 0.0, "splay": 0.0, "lift": 0.0, "pump": 0.6},
	RUN: {"stride": 1.3, "tuck": 0.0, "reach": 0.0, "splay": 0.0, "lift": 0.25, "pump": 1.2},
	CROUCH: {"stride": 0.0, "tuck": 0.0, "reach": 0.0, "splay": 0.3, "lift": 0.0, "pump": 0.0},
	CROUCH_WALK: {"stride": 0.7, "tuck": 0.0, "reach": 0.0, "splay": 0.3, "lift": 0.0, "pump": 0.3},
	SLIDE: {"stride": 0.0, "tuck": 0.0, "reach": 1.0, "splay": 0.1, "lift": 1.4, "pump": 0.0},
	JUMP: {"stride": 0.0, "tuck": 1.0, "reach": 0.0, "splay": 0.0, "lift": 0.9, "pump": 0.0},
	FALL: {"stride": 0.0, "tuck": 0.35, "reach": 0.0, "splay": 0.25, "lift": 1.9, "pump": 0.0},
	LAND: {"stride": 0.0, "tuck": 0.0, "reach": 0.0, "splay": 0.45, "lift": 0.5, "pump": 0.0},
}


## The state of a character moving across the ground at `speed` metres a
## second and upward at `rising` (below zero, falling), `airborne` or not,
## crouched by `crouch` (0 to 1), `sliding` or not, that last touched down
## `since_landing` seconds ago.
static func state(speed: float, rising: float, airborne: bool, crouch := 0.0, sliding := false, since_landing := INF) -> String:
	if airborne:
		return JUMP if rising > 0.0 else FALL
	if sliding:
		return SLIDE
	if since_landing < LAND_TIME:
		return LAND
	if crouch >= 0.5:
		return CROUCH_WALK if speed > MOVING else CROUCH
	if speed > Movement.WALK_SPEED * RUN_FROM:
		return RUN
	return WALK if speed > MOVING else IDLE


## What the free arm is doing, given what is going on: `doing` maps each of
## ACTIONS that is so to true.
static func action(doing: Dictionary) -> String:
	for kind: String in ACTIONS:
		if doing.get(kind, false):
			return kind
	return NONE


## How much of the way to a new pose the limbs get in `delta` seconds.
static func blend(delta: float) -> float:
	return 1.0 - exp(-delta / BLEND_TIME)


## Where the free arm is `elapsed` seconds into `action`, as its turn about
## the shoulder: (forward, above zero, or back, 0, out from the side), in
## radians. `rest` is where it would be otherwise.
static func arm(kind: String, elapsed: float, rest: Vector3) -> Vector3:
	match kind:
		AIM, RELOAD:
			# Across and forward, to steady the weapon.
			return Vector3(1.25, 0.0, 0.25)
		FIRE:
			return rest + Vector3(-0.12, 0.0, 0.0)
		HURT:
			return Vector3(-0.3, 0.0, -1.1)
		MELEE:
			return Vector3(-0.6, 0.0, -0.6)
		THROW:
			# Back over the shoulder, then through and down.
			var through := clampf(elapsed / ACTION_TIME[THROW], 0.0, 1.0)
			return Vector3(lerpf(-2.6, 1.0, smoothstep(0.2, 0.8, through)), 0.0, -0.3)
	return rest
