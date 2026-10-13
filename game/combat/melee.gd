class_name Melee
## A strike with the cheese knife every player carries: short reach, on its
## own key, whatever weapon is in hand. It hurts badly, and from behind it
## knocks a player out at a stroke. The player lunges as they strike.
##
## Players strike on their own machines and tell the host, which decides
## whom it reached (see Match), as with a shot.

## What the kill feed calls it, and the id it goes by where a weapon's would.
const ID := "knife"
const NAME := "Cheese Knife"
## How far from the striker's eye the blade reaches, in metres.
const REACH := 4.5
## The strike sweeps this many degrees either side of where it is aimed.
const SWEEP := 16.0
const DAMAGE := 55.0
## A strike from behind does this instead: more than anyone's health.
const BACKSTAB_DAMAGE := 500.0
## Seconds between strikes.
const INTERVAL := 0.75
## How long the swing takes to see, and how fast the lunge is, in metres a
## second.
const SWING_TIME := 0.25
const LUNGE := 7.0


## The directions a strike aimed along `direction` (normalized) is tested
## in: straight ahead first, then fanned out to each side.
static func sweep(direction: Vector3) -> Array[Vector3]:
	var fan: Array[Vector3] = [direction]
	for degrees: float in [SWEEP / 2.0, SWEEP]:
		for side: float in [-1.0, 1.0]:
			fan.append(direction.rotated(Vector3.UP, deg_to_rad(degrees) * side))
	return fan


## Whether a strike travelling along `direction` lands in the back of a
## player turned by `yaw`: from within a quarter turn of straight behind.
static func from_behind(direction: Vector3, yaw: float) -> bool:
	var facing := Vector3(-sin(yaw), 0.0, -cos(yaw))
	var along := Vector3(direction.x, 0.0, direction.z)
	return along.length() > 0.001 and along.normalized().dot(facing) > 0.5


## The damage of a strike along `direction` on a player turned by `yaw`.
static func damage(direction: Vector3, yaw: float) -> float:
	return BACKSTAB_DAMAGE if from_behind(direction, yaw) else DAMAGE


## How far through its swing the knife is, `since` seconds after a strike:
## from 0, across the view, to 1, or 1 when at rest.
static func swing(since: float) -> float:
	return clampf(since / SWING_TIME, 0.0, 1.0)
