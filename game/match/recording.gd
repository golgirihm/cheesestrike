class_name Recording
extends RefCounted
## The last few seconds of a match, as the host saw it: where each player
## was and which way they faced, a few times a second, and the shots that
## were fired. From it the host cuts a clip to show a player how they were
## knocked out, and one of the best moment of the match.
##
## A clip is small enough to send: for each player in it, a track of
## numbers, six for each moment kept (seconds from the clip's start, x, y, z,
## yaw, pitch); and for each shot, when, who, and where it went from and to.

## How many seconds are kept, and how far apart the moments kept are.
const KEEP := 6.0
const INTERVAL := 0.1
## The most shots kept in one clip, and the most tracks.
const MAX_SHOTS := 40
const MAX_TRACKS := 4
const _STEP := 6

# Player id -> PackedFloat32Array of moments, each [time, x, y, z, yaw, pitch],
# with times on the host's clock.
var _tracks := {}
# Each [time, shooter, from, to].
var _shots: Array[Array] = []


## Player `id` was at `at` facing `facing` (yaw, pitch) at `time`. Kept only
## if long enough after the last moment kept for them.
func add(time: float, id: int, at: Vector3, facing := Vector2.ZERO) -> void:
	var track: PackedFloat32Array = _tracks.get(id, PackedFloat32Array())
	if not track.is_empty() and time - track[track.size() - _STEP] < INTERVAL:
		return
	track.append_array(PackedFloat32Array([time, at.x, at.y, at.z, facing.x, facing.y]))
	# Forget what is too old to be wanted.
	var old := 0
	while old + _STEP < track.size() and track[old] < time - KEEP:
		old += _STEP
	_tracks[id] = track.slice(old) if old > 0 else track


## `shooter` fired from `from` at `time`, and the shot ended at each of `ends`.
func shot(time: float, shooter: int, from: Vector3, ends: Array) -> void:
	# One line for a shot of many pellets is enough to show it.
	if not ends.is_empty() and ends[0] is Vector3:
		_shots.append([time, shooter, from, ends[0]])
	while not _shots.is_empty() and _shots[0][0] < time - KEEP:
		_shots.pop_front()


func forget(id: int) -> void:
	_tracks.erase(id)


func clear() -> void:
	_tracks.clear()
	_shots.clear()


## The `seconds` up to `until` as a clip of the players `ids`, with the
## shots any of them fired.
func clip(ids: Array, until: float, seconds: float) -> Dictionary:
	var from := until - seconds
	var tracks := {}
	for id: int in ids.slice(0, MAX_TRACKS):
		var kept := PackedFloat32Array()
		var track: PackedFloat32Array = _tracks.get(id, PackedFloat32Array())
		for start in range(0, track.size(), _STEP):
			if track[start] >= from - INTERVAL and track[start] <= until:
				kept.append(track[start] - from)
				kept.append_array(track.slice(start + 1, start + _STEP))
		if not kept.is_empty():
			tracks[id] = kept
	var shots := []
	for fired: Array in _shots:
		if fired[0] >= from and fired[0] <= until and ids.has(fired[1]):
			shots.append([fired[0] - from, fired[1], fired[2], fired[3]])
	return {"length": seconds, "tracks": tracks, "shots": shots.slice(-MAX_SHOTS)}


## Where the player whose `track` this is was `time` seconds into a clip:
## {"at", "yaw", "pitch"}, between the moments kept either side, or {} if
## the track has nothing.
static func pose(track: PackedFloat32Array, time: float) -> Dictionary:
	var count := track.size() / _STEP
	if count == 0:
		return {}
	var after := 0
	while after < count and track[after * _STEP] < time:
		after += 1
	var a := clampi(after - 1, 0, count - 1) * _STEP
	var b := clampi(after, 0, count - 1) * _STEP
	var span := track[b] - track[a]
	var share := clampf((time - track[a]) / span, 0.0, 1.0) if span > 0.0 else 0.0
	return {
		"at": Vector3(track[a + 1], track[a + 2], track[a + 3]).lerp(Vector3(track[b + 1], track[b + 2], track[b + 3]), share),
		"yaw": lerp_angle(track[a + 4], track[b + 4], share),
		"pitch": lerpf(track[a + 5], track[b + 5], share),
	}


## A clip as it arrived from another machine, with anything that isn't one
## left out; {} if there is nothing of it left to show.
static func clean(data: Variant) -> Dictionary:
	if not data is Dictionary or not (data.get("length") is float or data.get("length") is int) or not data.get("tracks") is Dictionary:
		return {}
	var length := float(data.length)
	if not is_finite(length) or length <= 0.0 or length > KEEP:
		return {}
	var tracks := {}
	for id: Variant in data.tracks:
		var track: Variant = data.tracks[id]
		if not id is int or not track is PackedFloat32Array or track.is_empty() or track.size() % _STEP != 0 or track.size() > _STEP * 200:
			continue
		var sound := true
		for number: float in track:
			sound = sound and is_finite(number)
		if sound and tracks.size() < MAX_TRACKS:
			tracks[id] = track
	if tracks.is_empty():
		return {}
	var shots := []
	var given: Variant = data.get("shots")
	if given is Array:
		for fired: Variant in given.slice(0, MAX_SHOTS):
			if fired is Array and fired.size() == 4 and (fired[0] is float or fired[0] is int) and fired[1] is int and fired[2] is Vector3 and fired[3] is Vector3 and fired[2].is_finite() and fired[3].is_finite() and is_finite(float(fired[0])):
				shots.append([float(fired[0]), fired[1], fired[2], fired[3]])
	return {"length": length, "tracks": tracks, "shots": shots}
