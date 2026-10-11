extends Node
## The host's side of the rules, and everyone's copy of what it decided.
##
## The host keeps the MatchState and sends all of it to every player whenever it
## changes, at most SYNC_INTERVAL apart. Players ask the host for things (a new
## name, and later a shot) by RPC, and the host decides.

signal state_changed

const SYNC_INTERVAL := 0.1

## Everyone's copy of the host's decisions. Replaced, not changed, on players
## other than the host, so hold on to it only briefly.
var state := MatchState.new()

# Names asked for by players the host hasn't seen join yet, by player id.
var _pending_names := {}
var _dirty := false
var _since_sync := 0.0


func start() -> void:
	# A player other than the host keeps whatever the host has already sent.
	if is_host():
		state = MatchState.new()
		_pending_names.clear()
		state_changed.emit()
	else:
		introduce(Settings.get_value("name"))


func stop() -> void:
	state = MatchState.new()
	_dirty = false
	state_changed.emit()


func is_host() -> bool:
	return multiplayer.multiplayer_peer != null and multiplayer.is_server()


## Host only: a player has joined the session.
func add_player(id: int) -> void:
	state.add_player(id)
	if id == multiplayer.get_unique_id():
		state.rename(id, Settings.get_value("name"))
	elif _pending_names.has(id):
		state.rename(id, _pending_names[id])
	_pending_names.erase(id)
	_changed()


## Host only: a player has left.
func remove_player(id: int) -> void:
	state.remove_player(id)
	_pending_names.erase(id)
	_changed()


## Tells the host what this player wants to be called.
func introduce(wanted: String) -> void:
	if is_host():
		_rename(1, wanted)
	elif multiplayer.multiplayer_peer != null:
		_introduce.rpc_id(1, wanted)


func _process(delta: float) -> void:
	_since_sync += delta
	if _dirty and _since_sync >= SYNC_INTERVAL and is_host():
		_dirty = false
		_since_sync = 0.0
		_receive_state.rpc(state.to_dict())


func _changed() -> void:
	_dirty = true
	state_changed.emit()


func _rename(id: int, wanted: String) -> void:
	if state.player(id) == null:
		_pending_names[id] = wanted
	elif state.rename(id, wanted):
		_changed()


@rpc("any_peer", "call_remote", "reliable")
func _introduce(wanted: Variant) -> void:
	if is_host() and wanted is String:
		_rename(multiplayer.get_remote_sender_id(), wanted)


@rpc("authority", "call_remote", "reliable")
func _receive_state(data: Variant) -> void:
	if data is Dictionary:
		state = MatchState.from_dict(data)
		state_changed.emit()
