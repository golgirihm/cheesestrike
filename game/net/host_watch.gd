class_name HostWatch
extends RefCounted
## Works out, from what a player hears, whether the host is still running the
## game. Three things feed it: the host's regular heartbeat, the connection to
## the host dropping, and the session server saying whether the session is
## still open, which a player asks it once either of the others gives cause.
##
## Kept free of any networking so the rules can be tested on their own; Net
## feeds it and acts on the answers.

const HEARTBEAT_INTERVAL := 0.25
const HEARTBEAT_TIMEOUT := 1.5
## How often to ask the session server about a silent host, in seconds.
const CHECK_INTERVAL := 0.5
## How many times running the session server must say the session is closed.
## More than once, because a host whose connection to it dropped is away from
## it for a moment while reconnecting. After the connection to the host has
## dropped, it must say the session is open as many times running before only
## the connection is taken to have failed.
const CLOSED_REPORTS := 2
## How long, once the connection to the host has dropped, to wait for the
## session server to say the host has left, in seconds. Without its word by
## then, only the connection is taken to have failed.
const DROP_WAIT := 3.0

var _closed := false
var _closed_reports := 0
var _heartbeat_age := 0.0
var _dropped := false
var _dropped_age := 0.0
var _open_reports := 0


## Nothing has been heard from the host for a while. It may be suspended,
## stalled or cut off, and may yet come back.
func is_silent() -> bool:
	return _dropped or _heartbeat_age > HEARTBEAT_TIMEOUT


## The host has left for good. A closed host page can't say goodbye and WebRTC
## takes several seconds to notice, while the session server sees the host
## drop at once; the silent heartbeat confirms it wasn't just the host's
## connection to that server.
func is_gone() -> bool:
	return _closed and is_silent()


## The connection to the host has failed while the host itself may still be
## running the session, so trying it again is worth it. A dropped connection is
## this unless the session server says otherwise: a host closing its page or
## leaving drops it too, sometimes before anything else is heard.
func is_lost() -> bool:
	return _dropped and not is_gone() and (_open_reports >= CLOSED_REPORTS or _dropped_age >= DROP_WAIT)


func tick(delta: float) -> void:
	# Capped so one long frame here doesn't read as the host going quiet.
	_heartbeat_age += minf(delta, HEARTBEAT_INTERVAL)
	if _dropped:
		_dropped_age += minf(delta, HEARTBEAT_INTERVAL)


## The connection to the host has dropped, and nothing more will come over it.
func connection_dropped() -> void:
	_dropped = true
	_open_reports = 0


func heartbeat_received() -> void:
	_heartbeat_age = 0.0
	_closed_reports = 0
	_open_reports = 0


## The session server's answer to whether the session is still open.
func session_reported(open: bool) -> void:
	# Asked as many times as for a closed one, as the session server may
	# not yet have seen the host go.
	_open_reports = _open_reports + 1 if open else 0
	_closed_reports = 0 if open else _closed_reports + 1
	if _closed_reports >= CLOSED_REPORTS:
		_closed = true
