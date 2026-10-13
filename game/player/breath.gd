class_name Breath
extends RefCounted
## The breath of a player looking through a scope. The view sways with it,
## slowly and never the same way twice in a row; holding it steadies the
## view for a few seconds, after which it has to be let out and got back.

## How far the view sways, in radians, and the share of that left while the
## breath is held.
const SWAY := 0.011
const HELD := 0.08
## Seconds a full breath can be held, and to get one back.
const LUNGS := 3.5
const RECOVERY := 4.0
## A breath run right out has to come back this far before another is held,
## and the view sways this much more until it has.
const WINDED_UNTIL := 0.4
const WINDED_SWAY := 1.6

## The breath left: 1 full, 0 none.
var air := 1.0
## Whether it is being held, and whether it ran right out and is coming back.
var holding := false
var winded := false
var _time := 0.0
var _amount := 1.0


## Moves on by `delta` seconds, with the player wanting to hold it or not.
func tick(delta: float, wanted: bool) -> void:
	_time += delta
	holding = wanted and not winded and air > 0.0
	if holding:
		air = maxf(air - delta / LUNGS, 0.0)
		if air <= 0.0:
			winded = true
			holding = false
	else:
		air = minf(air + delta / RECOVERY, 1.0)
		if winded and air >= WINDED_UNTIL:
			winded = false
	var target := HELD if holding else (WINDED_SWAY if winded else 1.0)
	_amount = move_toward(_amount, target, delta * 4.0)


## How far the view is off just now, as (to the right, upward) in radians.
func sway() -> Vector2:
	return sway_at(_time) * _amount


## The sway `time` seconds in, unsteadied: a slow figure that doesn't repeat
## for a long while.
static func sway_at(time: float) -> Vector2:
	return Vector2(sin(time * 0.9) + 0.4 * sin(time * 2.3 + 1.0), sin(time * 1.3 + 2.0) + 0.4 * cos(time * 1.9)) * (SWAY / 1.4)
