class_name HostWatch
extends RefCounted
## Works out, from what a player hears, whether the host is still running the
## game. Two things feed it: the host's regular heartbeat, and the session
## server saying the session has closed, which a player asks it once the
## heartbeat stops.
##
## Kept free of any networking so the rules can be tested on their own; Net
## feeds it and acts on the answers.

const HEARTBEAT_INTERVAL := 0.25
const HEARTBEAT_TIMEOUT := 1.5
## How often to ask the session server about a silent host, in seconds.
const CHECK_INTERVAL := 0.5
## How many times running the session server must say the session is closed.
## More than once, because a host whose connection to it dropped is away from
## it for a moment while reconnecting.
const CLOSED_REPORTS := 2

var _closed := false
var _closed_reports := 0
var _heartbeat_age := 0.0


## Nothing has been heard from the host for a while. It may be suspended,
## stalled or cut off, and may yet come back.
func is_silent() -> bool:
	return _heartbeat_age > HEARTBEAT_TIMEOUT


## The host has left for good. A closed host page can't say goodbye and WebRTC
## takes several seconds to notice, while the session server sees the host
## drop at once; the silent heartbeat confirms it wasn't just the host's
## connection to that server.
func is_gone() -> bool:
	return _closed and is_silent()


func tick(delta: float) -> void:
	# Capped so one long frame here doesn't read as the host going quiet.
	_heartbeat_age += minf(delta, HEARTBEAT_INTERVAL)


func heartbeat_received() -> void:
	_heartbeat_age = 0.0
	_closed_reports = 0


## The session server's answer to whether the session is still open.
func session_reported(open: bool) -> void:
	_closed_reports = 0 if open else _closed_reports + 1
	if _closed_reports >= CLOSED_REPORTS:
		_closed = true
