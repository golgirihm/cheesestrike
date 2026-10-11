class_name InterpolationBuffer
extends RefCounted
## Smooths another player's movement. Their position and facing arrive in
## steps, some sooner than others; drawing them a moment in the past, between
## the two updates either side of that moment, hides both.
##
## A snapshot is [x, y, z, yaw, pitch]. Times are this machine's own, in
## seconds, when each snapshot arrived.

## How far behind the newest update others are drawn.
const DELAY := 0.1
## How far past the newest update to carry on in a straight line before
## holding still, when updates stop.
const MAX_EXTRAPOLATION := 0.1
const CAPACITY := 32

var _times := PackedFloat64Array()
var _snapshots: Array[PackedFloat32Array] = []


func push(time: float, snapshot: PackedFloat32Array) -> void:
	if snapshot.size() < 5:
		return
	# Out of order or repeated: an older update has nothing to add.
	if not _times.is_empty() and time <= _times[_times.size() - 1]:
		return
	_times.append(time)
	_snapshots.append(snapshot)
	while _times.size() > CAPACITY:
		_times.remove_at(0)
		_snapshots.remove_at(0)


func clear() -> void:
	_times.clear()
	_snapshots.clear()


func is_empty() -> bool:
	return _times.is_empty()


## The snapshot to draw at `time`: between the two either side of it, or
## carried on briefly from the last two past the newest, or the oldest before
## the first. Empty if nothing has arrived.
func sample(time: float) -> PackedFloat32Array:
	if _times.is_empty():
		return PackedFloat32Array()
	var last := _times.size() - 1
	if time <= _times[0] or last == 0:
		return _snapshots[0] if time <= _times[0] else _snapshots[last]
	if time >= _times[last]:
		var ahead := minf(time - _times[last], MAX_EXTRAPOLATION)
		var span := _times[last] - _times[last - 1]
		return _blend(_snapshots[last - 1], _snapshots[last], 1.0 + ahead / span) if span > 0.0 else _snapshots[last]
	var after := _times.bsearch(time)
	var before := after - 1
	var weight := (time - _times[before]) / (_times[after] - _times[before])
	return _blend(_snapshots[before], _snapshots[after], weight)


static func _blend(a: PackedFloat32Array, b: PackedFloat32Array, weight: float) -> PackedFloat32Array:
	return PackedFloat32Array([
		lerpf(a[0], b[0], weight),
		lerpf(a[1], b[1], weight),
		lerpf(a[2], b[2], weight),
		lerp_angle(a[3], b[3], weight),
		lerpf(a[4], b[4], weight),
	])
