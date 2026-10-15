extends Node
## Session transport. Web builds connect peer-to-peer over WebRTC, with the
## session server brokering the handshake; native builds use ENet directly,
## which is what local development and headless testing run on.
##
## Either way the result is a host (peer 1) with clients attached, exposed
## through the regular high-level multiplayer API.
##
## The session server (signaling/core.py has its protocol) is used sparingly,
## since holding a connection to it open is what it costs to run. A host keeps
## one WebSocket for as long as its session is open, and takes the session
## back on a new one whenever that drops. A player joining holds one only
## until they are connected to the host. Everything else is a plain HTTP
## request: the session list, and asking whether a session whose host has
## gone quiet is still open.

signal session_started
signal session_failed(reason: String)
## `reason` is empty when this player chose to leave.
signal session_ended(reason: String)
signal sessions_listed(sessions: Array)
## The session list couldn't be had: the session server didn't answer, or
## answered with something that isn't a list. `reason` says so, for a player.
signal sessions_unreachable(reason: String)
signal host_silent_changed(silent: bool)

const ENET_PORT := 7777
const MAX_PLAYERS := 16
const SIGNALING_PORT := 9080
const JOIN_TIMEOUT := 30.0
## How often a host says it is still there, in seconds. Whatever carries the
## connection closes one that has been quiet for long; AWS allows ten minutes.
const HOST_PING_INTERVAL := 240.0
## How long a host waits before reconnecting to the session server, in
## seconds: at first, and at most, doubling in between.
const RESUME_DELAY := 1.0
const RESUME_DELAY_MAX := 30.0
const ICE_SERVERS := [{"urls": ["stun:stun.l.google.com:19302"]}]
## How long hosting or joining waits for the session server to answer at all,
## in seconds, before saying it can't be reached. A server that is down
## usually refuses at once; one on a machine that has gone may never answer.
const SERVER_WAIT := 8.0
## What a developer runs to start the session server, said in development
## builds when it can't be reached.
const START_SERVER := "python signaling/server.py"
## How many times in a row the session list is asked for before a player is
## told the session server can't be reached.
const LIST_TRIES := 2

var use_webrtc := OS.has_feature("web")
var in_session := false
## Whether the session is one of this machine alone (see play_alone).
var alone := false
var room_code := ""
## The session this player last joined, as given to join(), kept after
## leaving so they can rejoin it.
var last_joined := ""
## True while nothing is being heard from the host; see HostWatch. The game
## holds still until the host is heard from again or the session ends.
var host_silent := false

var _signaling_url := "ws://localhost:%d" % SIGNALING_PORT
var _http_url := "http://localhost:%d" % SIGNALING_PORT
var _list_request: HTTPRequest
var _status_request: HTTPRequest
var _status_timer := 0.0
## What a host needs to take its session back on a new connection.
var _host_token := ""
var _host_settings: Dictionary = {}
var _host_private := false
var _ping_timer := 0.0
var _resume_delay := RESUME_DELAY
var _resume_timer := 0.0
var _resuming := false
var _joining := JoiningPeers.new()
var _ws: WebSocketPeer
# Seconds the connection to the session server has been opening.
var _ws_opening := 0.0
# How many times running the session list hasn't come.
var _list_failures := 0
var _outbox: Array[String] = []
var _rtc: WebRTCMultiplayerPeer
var _connecting := false
var _join_deadline := 0.0
var _host_watch := HostWatch.new()
var _heartbeat_timer := 0.0
var _visibility_callback: JavaScriptObject


func _ready() -> void:
	multiplayer.connected_to_server.connect(_start)
	multiplayer.connection_failed.connect(_fail.bind("Couldn't connect to the host."))
	multiplayer.server_disconnected.connect(leave.bind("The connection to the host was lost."))
	multiplayer.peer_connected.connect(_send_player_count.unbind(1))
	multiplayer.peer_disconnected.connect(_send_player_count.unbind(1))
	_list_request = HTTPRequest.new()
	_list_request.timeout = 10.0
	_list_request.request_completed.connect(_on_sessions_listed)
	add_child(_list_request)
	_status_request = HTTPRequest.new()
	_status_request.timeout = 5.0
	_status_request.request_completed.connect(_on_session_reported)
	add_child(_status_request)
	if OS.has_feature("web"):
		var urls := _web_backend_urls()
		_signaling_url = urls.signal
		_http_url = urls.http
		WebPage.install()
		_visibility_callback = JavaScriptBridge.create_callback(_on_visibility_changed)
		JavaScriptBridge.get_interface("document").addEventListener("visibilitychange", _visibility_callback)


## Starts a session. A `private` one is left out of the session list, so only
## players given its code or a link can join.
func host(settings := MatchSettings.new(), private := false) -> void:
	last_joined = ""
	_connecting = true
	if use_webrtc:
		_host_settings = settings.advertised()
		_host_private = private
		_send({"type": "host", "settings": _host_settings, "private": private})
		return
	var enet := ENetMultiplayerPeer.new()
	if enet.create_server(ENET_PORT, settings.max_players - 1) != OK:
		_fail("Couldn't open port %d." % ENET_PORT)
		return
	multiplayer.multiplayer_peer = enet
	room_code = "port %d" % ENET_PORT
	_start()


## Starts a session of one, on this machine alone, with nobody told and
## nothing for anyone to join: for the practice range.
func play_alone() -> void:
	_reset()
	multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	alone = true
	_start()


## `target` is a session code on WebRTC, or an address on ENet.
func join(target: String) -> void:
	target = target.strip_edges()
	last_joined = target
	_connecting = true
	_join_deadline = Time.get_unix_time_from_system() + JOIN_TIMEOUT
	if use_webrtc:
		_send({"type": "join", "code": target.to_upper()})
		return
	if target.is_empty():
		target = "127.0.0.1"
	var enet := ENetMultiplayerPeer.new()
	if enet.create_client(target, ENET_PORT) != OK:
		_fail("Couldn't connect to %s." % target)
		return
	multiplayer.multiplayer_peer = enet
	room_code = target


func leave(reason := "") -> void:
	var was_in_session := in_session
	_reset()
	if was_in_session:
		session_ended.emit(reason)


## Asks for the session list, which arrives as `sessions_listed`.
func request_sessions() -> void:
	if use_webrtc and _list_request.get_http_client_status() == HTTPClient.STATUS_DISCONNECTED:
		if _list_request.request(_http_url + "/sessions") != OK:
			sessions_unreachable.emit(unreachable(_http_url))


func _process(delta: float) -> void:
	if in_session:
		if multiplayer.is_server():
			_heartbeat_timer += delta
			if _heartbeat_timer >= HostWatch.HEARTBEAT_INTERVAL:
				_heartbeat_timer = 0.0
				_heartbeat.rpc()
			if use_webrtc:
				_keep_session_listed(delta)
				_drop_players_who_never_arrived(delta)
		else:
			_host_watch.tick(delta)
			if _host_watch.is_gone():
				leave("The host closed the session.")
				return
			if use_webrtc:
				_ask_about_silent_host(delta)
		_update_host_silent()
	if _connecting and _join_deadline > 0.0 and Time.get_unix_time_from_system() > _join_deadline:
		_fail("Timed out connecting to the host.")
	if _ws == null:
		return
	_ws.poll()
	var state := _ws.get_ready_state()
	if state == WebSocketPeer.STATE_CONNECTING:
		_ws_opening += delta
		if _connecting and _ws_opening >= SERVER_WAIT:
			_fail(unreachable(_signaling_url))
			return
	if state == WebSocketPeer.STATE_OPEN:
		for text in _outbox:
			_ws.send_text(text)
		_outbox.clear()
	if state == WebSocketPeer.STATE_OPEN or state == WebSocketPeer.STATE_CLOSED:
		# Also when closed: the server's last message and its closing of the
		# connection can both arrive between two frames, and that message is
		# the one saying the host has gone.
		while _ws != null and _ws.get_available_packet_count() > 0:
			var msg: Variant = JSON.parse_string(_ws.get_packet().get_string_from_utf8())
			if msg is Dictionary:
				_handle_signal(msg)
	if _ws != null and state == WebSocketPeer.STATE_CLOSED:
		_ws = null
		_outbox.clear()
		# An established session keeps running without the session server; a
		# host's just can't take new joiners until it has reconnected.
		if _connecting:
			_fail(unreachable(_signaling_url))


## What a player is told when the session server at `url` can't be reached:
## that it is needed, for what, and what can still be played without it. In
## a development build, how to start it too, as there it is most often only
## that it isn't running.
static func unreachable(url: String, development := OS.is_debug_build()) -> String:
	var said := "Can't reach the session server at %s. Hosting and joining need it; practice and the tutorial work without it." % url
	if development:
		said += "\nIs it running? Start it with: %s" % START_SERVER
	return said


func _start() -> void:
	_connecting = false
	_join_deadline = 0.0
	in_session = true
	if use_webrtc and not multiplayer.is_server():
		# Connected to the host, so the session server's part is over.
		_close_signaling()
	WebPage.set_wake_lock(true)
	WebPage.set_keep_alive(true)
	session_started.emit()


func _fail(reason: String) -> void:
	_reset()
	session_failed.emit(reason)


func _reset() -> void:
	WebPage.set_wake_lock(false)
	WebPage.set_keep_alive(false)
	multiplayer.multiplayer_peer = null
	_rtc = null
	_close_signaling()
	_status_request.cancel_request()
	_status_timer = 0.0
	_host_token = ""
	_ping_timer = 0.0
	_resume_delay = RESUME_DELAY
	_resume_timer = 0.0
	_resuming = false
	_joining = JoiningPeers.new()
	_connecting = false
	_join_deadline = 0.0
	in_session = false
	alone = false
	room_code = ""
	_host_watch = HostWatch.new()
	_heartbeat_timer = 0.0
	_update_host_silent()


## A hidden page may keep simulating (see WebPage), but drawing it is wasted work.
func _on_visibility_changed(_args: Array) -> void:
	RenderingServer.render_loop_enabled = not JavaScriptBridge.eval("document.hidden")


@rpc("authority", "call_remote", "unreliable")
func _heartbeat() -> void:
	_host_watch.heartbeat_received()


func _update_host_silent() -> void:
	var silent := _host_watch.is_silent()
	if silent != host_silent:
		host_silent = silent
		host_silent_changed.emit(silent)


func _send(msg: Dictionary) -> void:
	if _ws == null:
		_ws = WebSocketPeer.new()
		_ws_opening = 0.0
		if _ws.connect_to_url(_signaling_url) != OK:
			_ws = null
			# A session already under way carries on without the server.
			if not in_session:
				_fail(unreachable(_signaling_url))
			return
	_outbox.append(JSON.stringify(msg))


func _close_signaling() -> void:
	if _ws != null:
		_ws.close()
		_ws = null
	_outbox.clear()


## For a host: keeps the session server's record of the session alive, and
## takes the session back when the connection to it has dropped, as each one
## does in the end. Nothing in the match waits on this.
func _keep_session_listed(delta: float) -> void:
	if _host_token.is_empty():
		return
	if _ws != null:
		_ping_timer += delta
		if _ping_timer >= HOST_PING_INTERVAL:
			_ping_timer = 0.0
			_send({"type": "ping"})
		return
	_resume_timer += delta
	if _resume_timer < _resume_delay:
		return
	_resume_timer = 0.0
	_resume_delay = minf(_resume_delay * 2.0, RESUME_DELAY_MAX)
	_resuming = true
	_send({
		"type": "resume",
		"code": room_code,
		"token": _host_token,
		"settings": _host_settings,
		"private": _host_private,
		"players": multiplayer.get_peers().size() + 1,
	})


## For a host: forgets the half-made connections to players who started
## joining and never arrived; see JoiningPeers.
func _drop_players_who_never_arrived(delta: float) -> void:
	if _rtc == null:
		return
	for id in multiplayer.get_peers():
		_joining.connected(id)
	for id in _joining.tick(delta):
		if _rtc.has_peer(id):
			_rtc.remove_peer(id)


## For a host: tells the session server how many are playing, which is what the
## session list shows. It can't count them itself, since players leave it once
## they are connected here.
func _send_player_count() -> void:
	if in_session and use_webrtc and multiplayer.is_server() and _ws != null:
		_send({"type": "update", "players": multiplayer.get_peers().size() + 1})


## For a player whose host has gone quiet: asks the session server whether the
## session is still open, so a host that has left for good is told apart from
## one that is only stalled.
func _ask_about_silent_host(delta: float) -> void:
	if not _host_watch.is_silent():
		_status_timer = HostWatch.CHECK_INTERVAL  # So the first question goes at once.
		return
	_status_timer += delta
	if _status_timer < HostWatch.CHECK_INTERVAL or _status_request.get_http_client_status() != HTTPClient.STATUS_DISCONNECTED:
		return
	_status_timer = 0.0
	_status_request.request("%s/sessions/%s" % [_http_url, room_code])


func _on_sessions_listed(result: int, response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	var answer: Variant = null
	# Something in the way may answer with a page of its own, which isn't
	# worth an error in the log.
	var json := JSON.new()
	if result == HTTPRequest.RESULT_SUCCESS and response_code == 200 and json.parse(body.get_string_from_utf8()) == OK:
		answer = json.data
	if answer is Dictionary and answer.get("sessions") is Array:
		_list_failures = 0
		sessions_listed.emit(answer.sessions)
		return
	# Once may be nothing: the page still starting up and too busy to hear
	# back in time, or a moment without a connection. It is asked again at
	# once, and only if that fails too is the player told.
	_list_failures += 1
	if _list_failures < LIST_TRIES:
		request_sessions.call_deferred()
		return
	_list_failures = 0
	sessions_unreachable.emit(unreachable(_http_url))


func _on_session_reported(result: int, response_code: int, _headers: PackedStringArray, body: PackedByteArray) -> void:
	# No answer says nothing either way: this player's own connection may be
	# what has failed.
	if result != HTTPRequest.RESULT_SUCCESS or response_code != 200 or not in_session:
		return
	var answer: Variant = JSON.parse_string(body.get_string_from_utf8())
	if answer is Dictionary and answer.get("open") is bool:
		_host_watch.session_reported(answer.open)


func _handle_signal(msg: Dictionary) -> void:
	var from := int(msg.get("from", 0))
	match msg.get("type"):
		"hosted":
			room_code = msg.code
			_host_token = str(msg.get("token", ""))
			_rtc = WebRTCMultiplayerPeer.new()
			_rtc.create_server()
			multiplayer.multiplayer_peer = _rtc
			_start()
		"joined":
			room_code = msg.code
			_rtc = WebRTCMultiplayerPeer.new()
			_rtc.create_client(int(msg.id))
			multiplayer.multiplayer_peer = _rtc
			_add_rtc_peer(1, true)
		"peer_joined":
			_add_rtc_peer(int(msg.id), false)
			_joining.started(int(msg.id))
		"resumed":
			_resuming = false
			_resume_delay = RESUME_DELAY
			_ping_timer = 0.0
		"peer_left":
			_joining.left_server(int(msg.id))
		"offer", "answer":
			if _rtc != null and _rtc.has_peer(from):
				_rtc.get_peer(from).connection.set_remote_description(msg.type, msg.sdp)
		"candidate":
			if _rtc != null and _rtc.has_peer(from):
				_rtc.get_peer(from).connection.add_ice_candidate(msg.mid, int(msg.index), msg.sdp)
		"closed":
			# Only players still joining are connected to hear this.
			if _connecting:
				_fail("The host closed the session.")
		"error":
			if in_session:
				# The match carries on; only the session server is unhappy. A
				# host refused its session back stops asking for it.
				push_warning("Session server: %s" % msg.get("message", "error"))
				if _resuming:
					_host_token = ""
			else:
				_fail(msg.get("message", "Session server error."))


func _add_rtc_peer(id: int, make_offer: bool) -> void:
	if _rtc == null:
		return
	var pc := WebRTCPeerConnection.new()
	pc.initialize({"iceServers": ICE_SERVERS})
	pc.session_description_created.connect(_on_description_created.bind(id))
	pc.ice_candidate_created.connect(_on_ice_candidate_created.bind(id))
	_rtc.add_peer(pc, id)
	if make_offer:
		pc.create_offer()


func _on_description_created(type: String, sdp: String, id: int) -> void:
	if _rtc == null or not _rtc.has_peer(id):
		return
	_rtc.get_peer(id).connection.set_local_description(type, sdp)
	_send({"type": type, "to": id, "sdp": sdp})


func _on_ice_candidate_created(mid: String, index: int, sdp: String, id: int) -> void:
	_send({"type": "candidate", "to": id, "mid": mid, "index": index, "sdp": sdp})


## Where the session server is, from what the page says; see BackendUrls.
func _web_backend_urls() -> Dictionary:
	var override: Variant = JavaScriptBridge.eval("new URLSearchParams(location.search).get('signal')")
	var configured: Variant = JSON.parse_string(str(JavaScriptBridge.eval("JSON.stringify(window.cheeseStrikeConfig || {})")))
	return BackendUrls.choose(
		override if override is String else "",
		configured if configured is Dictionary else {},
		JavaScriptBridge.eval("location.protocol") == "https:",
		str(JavaScriptBridge.eval("location.hostname")),
	)
