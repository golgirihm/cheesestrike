class_name Splash
## How a burst, such as a Gouda Launcher wheel's, hurts and shoves the players
## around it.

## Damage to a player `distance` metres from the middle of a burst: all of
## `full` at the middle, falling evenly to none at `radius`.
static func damage_at(distance: float, full: float, radius: float) -> float:
	if radius <= 0.0 or distance >= radius:
		return 0.0
	return full * (1.0 - maxf(distance, 0.0) / radius)


## The shove given to a player whose middle is at `target` by a burst at `at`:
## away from it, a little upwards so it lifts them, and weaker further out.
static func push(at: Vector3, target: Vector3, strength: float, radius: float) -> Vector3:
	var away := target - at
	var distance := away.length()
	if radius <= 0.0 or distance >= radius:
		return Vector3.ZERO
	var direction := away / distance if distance > 0.001 else Vector3.UP
	direction = (direction + Vector3.UP * 0.5).normalized()
	return direction * strength * (1.0 - distance / radius)
