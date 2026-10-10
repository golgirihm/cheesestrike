class_name HostWatch
extends RefCounted
## Works out, from what a player hears, whether the host is still running the
## game. Two things feed it: the host's regular heartbeat, and the signaling
## server saying the host dropped.
##
## Kept free of any networking so the rules can be tested on their own; Net
## feeds it and acts on the answers.

const HEARTBEAT_INTERVAL := 0.25
const HEARTBEAT_TIMEOUT := 1.5

var _closed := false
var _heartbeat_age := 0.0


## The host isn't running the game right now, but may come back.
func is_paused() -> bool:
	return _is_silent()


## The host has left for good. A closed host page can't say goodbye and WebRTC
## takes several seconds to notice, while the signaling server sees the host
## drop at once; the silent heartbeat confirms it wasn't just signaling.
func is_gone() -> bool:
	return _closed and _is_silent()


func tick(delta: float) -> void:
	# Capped so one long frame here doesn't read as the host going quiet.
	_heartbeat_age += minf(delta, HEARTBEAT_INTERVAL)


func heartbeat_received() -> void:
	_heartbeat_age = 0.0


func signaling_closed() -> void:
	_closed = true


func _is_silent() -> bool:
	return _heartbeat_age > HEARTBEAT_TIMEOUT
