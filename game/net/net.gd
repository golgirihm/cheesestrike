extends Node
## Session transport. Web builds connect peer-to-peer over WebRTC, with the
## signaling server brokering the handshake; native builds use ENet directly,
## which is what local development and headless testing run on.
##
## Either way the result is a host (peer 1) with clients attached, exposed
## through the regular high-level multiplayer API.

signal session_started
signal session_failed(reason: String)
## `reason` is empty when this player chose to leave.
signal session_ended(reason: String)
signal sessions_listed(sessions: Array)
signal host_paused_changed(paused: bool)

const ENET_PORT := 7777
const MAX_PLAYERS := 16
const SIGNALING_PORT := 9080
const JOIN_TIMEOUT := 30.0
const ICE_SERVERS := [{"urls": ["stun:stun.l.google.com:19302"]}]

var use_webrtc := OS.has_feature("web")
var in_session := false
var room_code := ""
## True while the host isn't running the game; see HostWatch.
var host_paused := false

var _signaling_url := "ws://localhost:%d" % SIGNALING_PORT
var _ws: WebSocketPeer
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
	if OS.has_feature("web"):
		_signaling_url = _web_signaling_url()
		# Browsers stop running a hidden tab's game loop, but still deliver
		# this event, which is the host's one chance to tell the others.
		_visibility_callback = JavaScriptBridge.create_callback(_on_visibility_changed)
		JavaScriptBridge.get_interface("document").addEventListener("visibilitychange", _visibility_callback)


func host() -> void:
	_connecting = true
	if use_webrtc:
		_send({"type": "host"})
		return
	var enet := ENetMultiplayerPeer.new()
	if enet.create_server(ENET_PORT, MAX_PLAYERS - 1) != OK:
		_fail("Couldn't open port %d." % ENET_PORT)
		return
	multiplayer.multiplayer_peer = enet
	room_code = "port %d" % ENET_PORT
	_start()


## `target` is a session code on WebRTC, or an address on ENet.
func join(target: String) -> void:
	target = target.strip_edges()
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


func request_sessions() -> void:
	if use_webrtc:
		_send({"type": "list"})


func _process(delta: float) -> void:
	if in_session:
		if multiplayer.is_server():
			_heartbeat_timer += delta
			if _heartbeat_timer >= HostWatch.HEARTBEAT_INTERVAL:
				_heartbeat_timer = 0.0
				_heartbeat.rpc()
		else:
			_host_watch.tick(delta)
			if _host_watch.is_gone():
				leave("The host closed the session.")
				return
		_update_host_paused()
	if _connecting and _join_deadline > 0.0 and Time.get_unix_time_from_system() > _join_deadline:
		_fail("Timed out connecting to the host.")
	if _ws == null:
		return
	_ws.poll()
	match _ws.get_ready_state():
		WebSocketPeer.STATE_OPEN:
			for text in _outbox:
				_ws.send_text(text)
			_outbox.clear()
			while _ws != null and _ws.get_available_packet_count() > 0:
				var msg: Variant = JSON.parse_string(_ws.get_packet().get_string_from_utf8())
				if msg is Dictionary:
					_handle_signal(msg)
		WebSocketPeer.STATE_CLOSED:
			_ws = null
			_outbox.clear()
			# An established session keeps running without signaling; it just
			# can't take new joiners.
			if _connecting:
				_fail("Can't reach the session server at %s." % _signaling_url)


func _start() -> void:
	_connecting = false
	_join_deadline = 0.0
	in_session = true
	session_started.emit()


func _fail(reason: String) -> void:
	_reset()
	session_failed.emit(reason)


func _reset() -> void:
	multiplayer.multiplayer_peer = null
	_rtc = null
	if _ws != null:
		_ws.close()
		_ws = null
	_outbox.clear()
	_connecting = false
	_join_deadline = 0.0
	in_session = false
	room_code = ""
	_host_watch = HostWatch.new()
	_heartbeat_timer = 0.0
	_update_host_paused()


func _on_visibility_changed(_args: Array) -> void:
	if in_session and multiplayer.is_server():
		_set_host_hidden.rpc(JavaScriptBridge.eval("document.hidden"))


@rpc("authority", "call_local", "reliable")
func _set_host_hidden(hidden: bool) -> void:
	_host_watch.host_hidden_changed(hidden)
	_update_host_paused()


@rpc("authority", "call_remote", "unreliable")
func _heartbeat() -> void:
	_host_watch.heartbeat_received()


func _update_host_paused() -> void:
	var paused := _host_watch.is_paused()
	if paused != host_paused:
		host_paused = paused
		host_paused_changed.emit(paused)


func _send(msg: Dictionary) -> void:
	if _ws == null:
		_ws = WebSocketPeer.new()
		if _ws.connect_to_url(_signaling_url) != OK:
			_ws = null
			_fail("Can't reach the session server at %s." % _signaling_url)
			return
	_outbox.append(JSON.stringify(msg))


func _handle_signal(msg: Dictionary) -> void:
	var from := int(msg.get("from", 0))
	match msg.get("type"):
		"sessions":
			sessions_listed.emit(msg.get("sessions", []))
		"hosted":
			room_code = msg.code
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
		"peer_left":
			if _rtc != null and _rtc.has_peer(int(msg.id)):
				_rtc.remove_peer(int(msg.id))
		"offer", "answer":
			if _rtc != null and _rtc.has_peer(from):
				_rtc.get_peer(from).connection.set_remote_description(msg.type, msg.sdp)
		"candidate":
			if _rtc != null and _rtc.has_peer(from):
				_rtc.get_peer(from).connection.add_ice_candidate(msg.mid, int(msg.index), msg.sdp)
		"closed":
			_host_watch.signaling_closed()
		"error":
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


## Defaults to the signaling port on whatever host served the page, so a phone
## on the LAN reaches the dev machine. `?signal=wss://...` overrides it.
func _web_signaling_url() -> String:
	var override: Variant = JavaScriptBridge.eval("new URLSearchParams(location.search).get('signal')")
	if override is String and not override.is_empty():
		return override
	var scheme := "wss" if JavaScriptBridge.eval("location.protocol") == "https:" else "ws"
	return "%s://%s:%d" % [scheme, JavaScriptBridge.eval("location.hostname"), SIGNALING_PORT]
