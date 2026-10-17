class_name CharacterMotion
## What a character is doing, worked out from how its player is moving. A
## character is always in one state of its legs (standing, walking, running,
## crouched, sliding, in the air, landing) and may be doing one thing with its
## arms on top of that (aiming, reloading, firing, throwing, striking,
## flinching). The soldier plays an animation for each.

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
## What the arms are doing, the first that applies winning.
const ACTIONS := [HURT, THROW, MELEE, RELOAD, FIRE, AIM]
## Seconds each momentary action lasts. The rest last while they are so.
const ACTION_TIME := {HURT: 0.3, THROW: 0.45, MELEE: 0.3, FIRE: 0.15}

## Slower than this across the ground is standing still; faster than
## RUN_FROM of walking speed is running.
const MOVING := 0.5
const RUN_FROM := 1.2
## Seconds a landing shows for.
const LAND_TIME := 0.18
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


## What the arms are doing, given what is going on: `doing` maps each of
## ACTIONS that is so to true.
static func action(doing: Dictionary) -> String:
	for kind: String in ACTIONS:
		if doing.get(kind, false):
			return kind
	return NONE

