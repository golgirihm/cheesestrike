class_name Match
extends Node
## The host's side of the rules, and everyone's copy of what it decided.
##
## The host keeps the MatchState and sends all of it to every player whenever it
## changes, at most SYNC_INTERVAL apart. Players ask the host for things (a new
## name, a shot) by RPC, and the host decides.
##
## A shot is fired on the shooter's machine, which shows it at once and tells
## the host where it started and which way it went. The host checks it could
## have been fired (ShotRules), works out what it hit against where it sees the
## players (HitScan), and applies the damage.

signal state_changed
## This player's shot hit `victim` (on this machine only).
signal hit_confirmed(victim: int, dealt: int, knocked_out: bool, head: bool)
## This player was hurt by `attacker`, who fired from `from` (on this machine only).
signal hurt(attacker: int, from: Vector3, dealt: int)
## Another player fired a shot whose pellets ended at `ends`, for effects.
signal shot_shown(shooter: int, ends: Array)
## Someone knocked someone out, for the kill feed. Names are as they were then.
signal knocked_out(attacker: String, victim: String, weapon: String, head: bool)

const SYNC_INTERVAL := 0.1
## Physics layer of the arena's walls, floor and cover, which stop shots.
const WORLD_MASK := 1

## Everyone's copy of the host's decisions. Replaced, not changed, on players
## other than the host, so hold on to it only briefly.
var state := MatchState.new()

@onready var world := get_parent() as Node3D

# Seconds since this node started, as the host's clock for ShotRules.
var _clock := 0.0
# The host's ShotRules for each player, by id.
var _rules := {}
# Names asked for by players the host hasn't seen join yet, by player id.
var _pending_names := {}
var _dirty := false
var _since_sync := 0.0


func start() -> void:
	# A player other than the host keeps whatever the host has already sent.
	if is_host():
		state = MatchState.new()
		_pending_names.clear()
		_rules.clear()
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
	_rules.erase(id)
	_changed()


## Tells the host what this player wants to be called.
func introduce(wanted: String) -> void:
	if is_host():
		_rename(1, wanted)
	elif multiplayer.multiplayer_peer != null:
		_introduce.rpc_id(1, wanted)


## This player fires the weapon in their hands from `origin` along `direction`.
## `seed` decides where any spread sends each pellet.
func fire(origin: Vector3, direction: Vector3, seed: int) -> void:
	if is_host():
		_shot(1, origin, direction, seed)
	elif multiplayer.multiplayer_peer != null:
		_request_shot.rpc_id(1, origin, direction, seed)


## This player has started reloading.
func reload() -> void:
	if is_host():
		_reload(1)
	elif multiplayer.multiplayer_peer != null:
		_request_reload.rpc_id(1)


## This player's own record, or null before the host has sent it.
func own_record() -> PlayerRecord:
	return state.player(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null


func _process(delta: float) -> void:
	_clock += delta
	# Everyone runs the clocks, so countdowns show smoothly between updates;
	# only the host acts on them.
	var due := state.tick(delta)
	if is_host():
		for id in due:
			_respawn(id)
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


func _rules_for(id: int) -> ShotRules:
	if not _rules.has(id):
		_rules[id] = ShotRules.new()
	return _rules[id]


## The host checks and carries out a shot that `shooter` reports.
func _shot(shooter: int, origin: Vector3, direction: Vector3, seed: int) -> void:
	var record := state.player(shooter)
	var shooter_node: Node3D = world.player_node(shooter)
	if record == null or not record.alive or shooter_node == null:
		return
	var weapon := Weapons.find(record.weapon)
	if weapon == null or not origin.is_finite() or not direction.is_finite() or direction.length() < 0.5:
		return
	if not ShotRules.origin_believable(origin, shooter_node.head.global_position):
		return
	if not _rules_for(shooter).allow_shot(weapon, _clock):
		return
	if record.is_protected():
		record.protected_for = 0.0
		_changed()

	var targets := {}
	for node: Node3D in world.players.get_children():
		var id := node.name.to_int()
		var target := state.player(id)
		if id != shooter and target != null and target.alive and not node.is_queued_for_deletion():
			targets[id] = node.global_position

	var damage_to := {}
	var headshot := {}
	var ends := []
	for pellet in HitScan.pellet_directions(direction, weapon.pellets, weapon.spread_degrees, seed):
		var reach := _distance_to_wall(origin, pellet, weapon.max_range)
		var hit := HitScan.first_hit(origin, pellet, reach, targets)
		if hit.is_empty():
			ends.append(origin + pellet * reach)
		else:
			ends.append(hit.point)
			var amount := weapon.damage * (weapon.headshot_multiplier if hit.head else 1.0)
			damage_to[hit.id] = damage_to.get(hit.id, 0.0) + amount
			headshot[hit.id] = headshot.get(hit.id, false) or hit.head
	_show_shot_to_others(shooter, ends)
	for victim: int in damage_to:
		var result := state.damage(shooter, victim, damage_to[victim])
		if result.dealt > 0:
			_changed()
			_tell_hit(shooter, victim, result.dealt, result.knocked_out, headshot[victim])
			_tell_hurt(victim, shooter, origin, result.dealt)
		if result.knocked_out:
			_announce_knockout(state.name_of(shooter), state.name_of(victim), weapon.id, headshot[victim])


func _show_shot_to_others(shooter: int, ends: Array) -> void:
	for peer in multiplayer.get_peers():
		if peer != shooter:
			_shot_seen.rpc_id(peer, shooter, ends)
	if shooter != multiplayer.get_unique_id():
		shot_shown.emit(shooter, ends)


func _tell_hit(shooter: int, victim: int, dealt: int, knocked_out: bool, head: bool) -> void:
	if shooter == multiplayer.get_unique_id():
		hit_confirmed.emit(victim, dealt, knocked_out, head)
	elif multiplayer.get_peers().has(shooter):
		_hit.rpc_id(shooter, victim, dealt, knocked_out, head)


func _tell_hurt(victim: int, attacker: int, from: Vector3, dealt: int) -> void:
	if victim == multiplayer.get_unique_id():
		hurt.emit(attacker, from, dealt)
	elif multiplayer.get_peers().has(victim):
		_hurt.rpc_id(victim, attacker, from, dealt)


func _announce_knockout(attacker: String, victim: String, weapon: String, head: bool) -> void:
	_knockout_seen.rpc(attacker, victim, weapon, head)
	knocked_out.emit(attacker, victim, weapon, head)


@rpc("authority", "call_remote", "reliable")
func _knockout_seen(attacker: String, victim: String, weapon: String, head: bool) -> void:
	knocked_out.emit(attacker, victim, weapon, head)


@rpc("authority", "call_remote", "unreliable")
func _shot_seen(shooter: int, ends: Array) -> void:
	shot_shown.emit(shooter, ends)


@rpc("authority", "call_remote", "reliable")
func _hit(victim: int, dealt: int, knocked_out: bool, head: bool) -> void:
	hit_confirmed.emit(victim, dealt, knocked_out, head)


@rpc("authority", "call_remote", "reliable")
func _hurt(attacker: int, from: Vector3, dealt: int) -> void:
	hurt.emit(attacker, from, dealt)


## How far a shot from `origin` along `direction` travels before the arena
## stops it, up to `max_range`.
func _distance_to_wall(origin: Vector3, direction: Vector3, max_range: float) -> float:
	var query := PhysicsRayQueryParameters3D.create(origin, origin + direction * max_range, WORLD_MASK)
	var hit := world.get_world_3d().direct_space_state.intersect_ray(query)
	return origin.distance_to(hit.position) if not hit.is_empty() else max_range


func _reload(id: int) -> void:
	var record := state.player(id)
	var weapon := Weapons.find(record.weapon) if record != null else null
	if weapon != null:
		_rules_for(id).reload(weapon, _clock)


## The host brings a knocked-out player back, at the spot farthest from
## everyone else.
func _respawn(id: int) -> void:
	var others: Array[Vector3] = []
	for node: Node3D in world.players.get_children():
		var other := state.player(node.name.to_int())
		if other != null and other.id != id and other.alive:
			others.append(node.global_position)
	var spots: Array[Vector3] = world.spawn_positions()
	var at := spots[SpawnChooser.farthest(spots, others)]
	state.respawn(id)
	_rules_for(id).refill()
	_changed()
	if id == multiplayer.get_unique_id():
		_respawned(at)
	elif multiplayer.get_peers().has(id):
		_respawned.rpc_id(id, at)


## Tells a player where they have come back. Players move themselves, so the
## host can only ask.
@rpc("authority", "call_remote", "reliable")
func _respawned(at: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.respawn_at(at)


@rpc("any_peer", "call_remote", "reliable")
func _request_shot(origin: Variant, direction: Variant, seed: Variant) -> void:
	if is_host() and origin is Vector3 and direction is Vector3 and seed is int:
		_shot(multiplayer.get_remote_sender_id(), origin, direction, seed)


@rpc("any_peer", "call_remote", "reliable")
func _request_reload() -> void:
	if is_host():
		_reload(multiplayer.get_remote_sender_id())


@rpc("any_peer", "call_remote", "reliable")
func _introduce(wanted: Variant) -> void:
	if is_host() and wanted is String:
		_rename(multiplayer.get_remote_sender_id(), wanted)


@rpc("authority", "call_remote", "reliable")
func _receive_state(data: Variant) -> void:
	if data is Dictionary:
		state = MatchState.from_dict(data)
		state_changed.emit()
