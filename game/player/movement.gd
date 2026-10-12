class_name Movement
## How players move: the numbers their own machine moves them by, and the
## host's check on them (see MovementCheck) allows for.

const WALK_SPEED := 6.0
## Running, with the run key held, is this much faster.
const RUN_FACTOR := 1.5
## Aiming down the sights slows a player to this share of walking.
const AIM_FACTOR := 0.6
## Gravity is strong (see the project's physics settings), so a jump is quick:
## this still lifts a player about a metre and a half.
const JUMP_VELOCITY := 7.8
## Being hit slows a player to FLINCH_SLOW of their speed, easing back to all
## of it over FLINCH_TIME seconds, and they can't run until it has.
const FLINCH_TIME := 0.7
const FLINCH_SLOW := 0.4


## The share of their speed a player hit `flinch_left` seconds short of
## FLINCH_TIME ago still has.
static func flinch_factor(flinch_left: float) -> float:
	return 1.0 - (1.0 - FLINCH_SLOW) * clampf(flinch_left / FLINCH_TIME, 0.0, 1.0)


## How fast a player moves across the ground. `boost` is anything else that
## changes it, such as an Overload.
static func speed(running: bool, aiming: bool, flinch_left := 0.0, boost := 1.0) -> float:
	var factor := boost * flinch_factor(flinch_left)
	if aiming:
		factor *= AIM_FACTOR
	elif running and flinch_left <= 0.0:
		factor *= RUN_FACTOR
	return WALK_SPEED * factor
