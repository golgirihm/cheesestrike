class_name PingTracker
extends RefCounted
## A player's round trip to the host, from the host's regular pings, smoothed so
## that one slow reply doesn't swing it.

## How much each new measurement counts, against everything before it.
const WEIGHT := 0.3

var _seconds := -1.0


## A ping came back after `seconds`.
func add(seconds: float) -> void:
	if seconds < 0.0 or not is_finite(seconds):
		return
	_seconds = seconds if _seconds < 0.0 else lerpf(_seconds, seconds, WEIGHT)


func has_measured() -> bool:
	return _seconds >= 0.0


## The smoothed round trip in seconds, or 0 before any.
func seconds() -> float:
	return maxf(_seconds, 0.0)


func milliseconds() -> int:
	return roundi(seconds() * 1000.0)
