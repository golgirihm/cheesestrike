class_name PositionHistory
extends RefCounted
## Where one player has been over the last moment, as the host saw them, so
## that a shot can be checked against where its target was when the shooter
## saw them rather than where they are by the time the shot arrives.

## How far back to remember, in seconds.
const KEEP := 1.0

var _times := PackedFloat64Array()
var _positions := PackedVector3Array()
# Yaw and pitch, alongside each position.
var _facings := PackedVector2Array()


## The player was at `at`, facing `facing` (yaw, pitch), at time `time`
## (seconds, the host's clock).
func push(time: float, at: Vector3, facing := Vector2.ZERO) -> void:
	if not _times.is_empty() and time <= _times[_times.size() - 1]:
		return
	_times.append(time)
	_positions.append(at)
	_facings.append(facing)
	while _times.size() > 2 and _times[0] < time - KEEP:
		_times.remove_at(0)
		_positions.remove_at(0)
		_facings.remove_at(0)


func is_empty() -> bool:
	return _times.is_empty()


## Where the player was at `time`: between the two records either side of it,
## the oldest kept if it is older than all, or the newest if it is newer.
func at(time: float) -> Vector3:
	if _times.is_empty():
		return Vector3.ZERO
	var last := _times.size() - 1
	if time <= _times[0]:
		return _positions[0]
	if time >= _times[last]:
		return _positions[last]
	var after := _times.bsearch(time)
	var weight := (time - _times[after - 1]) / (_times[after] - _times[after - 1])
	return _positions[after - 1].lerp(_positions[after], weight)


## How the player faced at `time` (yaw, pitch), found as at() finds where.
func facing_at(time: float) -> Vector2:
	if _times.is_empty():
		return Vector2.ZERO
	var last := _times.size() - 1
	if time <= _times[0]:
		return _facings[0]
	if time >= _times[last]:
		return _facings[last]
	var after := _times.bsearch(time)
	var weight := (time - _times[after - 1]) / (_times[after] - _times[after - 1])
	var before := _facings[after - 1]
	return Vector2(lerp_angle(before.x, _facings[after].x, weight), lerpf(before.y, _facings[after].y, weight))


func clear() -> void:
	_times.clear()
	_positions.clear()
	_facings.clear()
