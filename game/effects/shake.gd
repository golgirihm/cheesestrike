class_name Shake
extends RefCounted
## Shaking the view when something jolts the player, such as being hit or a
## burst going off nearby. Each jolt adds to a "trauma" that dies away; the
## view tips by the square of it, so small jolts barely show and big ones do.
## Players can turn it down or off in the settings.

## Trauma lost each second.
const DECAY := 1.6
## How far the view tips at full trauma, up and down and sideways.
const MAX_PITCH := deg_to_rad(1.6)
const MAX_ROLL := deg_to_rad(2.4)

var trauma := 0.0
var _time := 0.0


## Adds a jolt of `amount`, from 0 (none) to 1 (as hard as any).
func add(amount: float) -> void:
	trauma = clampf(trauma + maxf(amount, 0.0), 0.0, 1.0)


## Moves on by `delta` seconds and returns how far the view tips now, as
## (pitch, roll) in radians, scaled by `strength` (the setting, 0 to 1).
func tick(delta: float, strength: float) -> Vector2:
	_time += delta
	trauma = maxf(trauma - DECAY * delta, 0.0)
	var amount := trauma * trauma * clampf(strength, 0.0, 1.0)
	return Vector2(sin(_time * 37.0) * MAX_PITCH, sin(_time * 29.0 + 1.3) * MAX_ROLL) * amount


## The jolt from being hurt for `dealt` damage.
static func from_damage(dealt: int) -> float:
	return clampf(dealt / 60.0, 0.15, 0.6)


## The jolt from a burst at `distance` metres, reaching to `reach`.
static func from_burst(distance: float, reach: float) -> float:
	if reach <= 0.0 or distance >= reach:
		return 0.0
	return 0.7 * (1.0 - distance / reach)
