class_name Recoil
## How a weapon kicks as it is fired. Each shot of a run of fire goes a little
## higher than the last, and wanders from side to side, along a pattern that
## is always the same for a weapon, so that a player can learn it and pull
## against it. The first shot of a run goes exactly where it is aimed.
##
## The shooter's view climbs by the same amount, so shots still land where
## the crosshair is. The host works the kick out for itself, from its own
## count of the shooter's shots (see ShotRules), and applies it to the aim
## the shooter reports: a changed game can't leave it out.

## Aiming down the sights steadies a weapon to this share of its kick.
const AIM_FACTOR := 0.6


## How far shot number `index` of a run of fire from `weapon` (0 for the
## first) is thrown off, in radians, as (to the right, upward).
static func offset(weapon: WeaponStats, index: int, aimed := false) -> Vector2:
	if index <= 0:
		return Vector2.ZERO
	var steady := AIM_FACTOR if aimed else 1.0
	# The climb slows as it goes, and never passes `recoil_cap` shots' worth.
	var cap := maxf(weapon.recoil_cap, 1.0)
	var climb := weapon.recoil_up * cap * (1.0 - exp(-index / cap))
	var wander := weapon.recoil_side * sin(index * 1.3) * minf(index / 3.0, 1.0)
	return Vector2(deg_to_rad(wander), deg_to_rad(climb)) * steady


## `direction` (normalized) thrown off by `by`, as offset() gives it.
static func apply(direction: Vector3, by: Vector2) -> Vector3:
	if by == Vector2.ZERO:
		return direction
	var right := direction.cross(Vector3.UP)
	right = right.normalized() if right.length() > 0.001 else Vector3.RIGHT
	return direction.rotated(right, by.y).rotated(Vector3.UP, -by.x).normalized()
