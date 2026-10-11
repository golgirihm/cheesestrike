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
## Player `id` set off their Overload (on everyone's machine).
signal overloaded(id: int)
## Player `id` collected a pickup of `kind` (on everyone's machine).
signal picked_up(id: int, kind: String)
## A projectile was launched, or burst, for effects.
signal projectile_launched(id: int, weapon: String, from: Vector3, velocity: Vector3)
signal projectile_burst(id: int, weapon: String, at: Vector3)

const SYNC_INTERVAL := 0.1
## The host sends the state at least this often, even unchanged, so that
## everyone's clocks stay in step with its own.
const RESYNC_INTERVAL := 2.0
## Physics layer of the arena's walls, floor and cover, which stop shots.
const WORLD_MASK := 1
## Height of a player's middle above their feet, where bursts aim their harm.
const MIDDLE := 0.9
## The most a shot is checked back in time. Beyond it, a shooter on a slow
## connection has to lead their target.
const MAX_REWIND := 0.3
## Seconds between the host's pings to each player.
const PING_INTERVAL := 1.0
## A ping that changes by less than this many milliseconds isn't news.
const PING_CHANGE := 5

## Everyone's copy of the host's decisions. Replaced, not changed, on players
## other than the host, so hold on to it only briefly.
var state := MatchState.new()

@onready var world := get_parent() as Node3D

# Seconds since this node started, as the host's clock for ShotRules.
var _clock := 0.0
# The host's ShotRules for each player, by id.
var _rules := {}
# Where each player has lately been, as the host saw them, by id.
var _histories := {}
# The host's check on each player's movement, by id, and when it last put
# each back.
var _checks := {}
var _put_back_at := {}
## How many times the host has put a player back, for the browser tests.
var put_backs := 0
# The host's measure of each player's round trip, by id.
var _pings := {}
var _since_ping := 0.0
# The host's projectiles in flight, by id.
var _projectiles := {}
var _next_projectile := 1
# Names asked for by players the host hasn't seen join yet, by player id.
var _pending_names := {}
var _dirty := false
var _since_sync := 0.0
# Whether this machine has the host's decisions yet: at once on the host, and
# with the first state that arrives elsewhere.
var _has_state := false


## Begins the session's matches; on the host, with `settings`.
func start(settings := MatchSettings.new()) -> void:
	# A player other than the host keeps whatever the host has already sent.
	if is_host():
		state = MatchState.new(settings)
		_has_state = true
		state.set_pickup_count(world.pickup_pads().size() if world != null else 0)
		_pending_names.clear()
		_rules.clear()
		_projectiles.clear()
		_histories.clear()
		_checks.clear()
		_put_back_at.clear()
		state_changed.emit()
	else:
		introduce(Settings.get_value("name"))


func stop() -> void:
	state = MatchState.new()
	_has_state = false
	_dirty = false
	state_changed.emit()


## Whether the host's decisions have reached this machine yet.
func has_state() -> bool:
	return _has_state or is_host()


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
	_pings.erase(id)
	_histories.erase(id)
	_checks.erase(id)
	_put_back_at.erase(id)
	_changed()


## Tells the host what this player wants to be called.
func introduce(wanted: String) -> void:
	if is_host():
		_rename(1, wanted)
	elif multiplayer.multiplayer_peer != null:
		_introduce.rpc_id(1, wanted)


## This player fires `weapon` from `origin` along `direction`. `seed` decides
## where any spread sends each pellet.
func fire(origin: Vector3, direction: Vector3, seed: int, weapon := Weapons.STARTING) -> void:
	if is_host():
		_shot(1, origin, direction, seed, weapon)
	elif multiplayer.multiplayer_peer != null:
		_request_shot.rpc_id(1, origin, direction, seed, weapon)


## This player has started reloading `weapon`.
func reload(weapon := Weapons.STARTING) -> void:
	if is_host():
		_reload(1, weapon)
	elif multiplayer.multiplayer_peer != null:
		_request_reload.rpc_id(1, weapon)


## Player `id`'s round trip to the host in seconds, as the host last measured
## it; 0 for the host itself or before any.
func round_trip(id: int) -> float:
	return _pings[id].seconds() if _pings.has(id) else 0.0


@rpc("authority", "call_remote", "unreliable")
func _ping(sent: float) -> void:
	_pong.rpc_id(1, sent)


@rpc("any_peer", "call_remote", "unreliable")
func _pong(sent: Variant) -> void:
	if not is_host() or not (sent is float) or sent > _clock:
		return
	var id := multiplayer.get_remote_sender_id()
	if not _pings.has(id):
		_pings[id] = PingTracker.new()
	_pings[id].add(_clock - sent)
	var record := state.player(id)
	if record != null and absi(record.ping - _pings[id].milliseconds()) >= PING_CHANGE:
		record.ping = _pings[id].milliseconds()
		_changed()


## This player sets off their Overload, if it is ready.
func set_off_overload() -> void:
	if is_host():
		_overload(1)
	elif multiplayer.multiplayer_peer != null:
		_request_overload.rpc_id(1)


func _overload(id: int) -> void:
	if state.set_off_overload(id):
		_changed()
		_overload_seen.rpc(id)
		overloaded.emit(id)


@rpc("any_peer", "call_remote", "reliable")
func _request_overload() -> void:
	if is_host():
		_overload(multiplayer.get_remote_sender_id())


@rpc("authority", "call_remote", "reliable")
func _overload_seen(id: int) -> void:
	overloaded.emit(id)


## This player has taken `weapon` in hand, so others see it.
func select_weapon(weapon: String) -> void:
	if is_host():
		_select(1, weapon)
	elif multiplayer.multiplayer_peer != null:
		_request_select.rpc_id(1, weapon)


## This player's own record, or null before the host has sent it.
func own_record() -> PlayerRecord:
	return state.player(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null


func _process(delta: float) -> void:
	_clock += delta
	# Everyone runs the clocks, so countdowns show smoothly between updates;
	# only the host acts on them.
	var was_playing := state.is_playing()
	var due := state.tick(delta)
	if is_host():
		_since_ping += delta
		if _since_ping >= PING_INTERVAL:
			_since_ping = 0.0
			for peer in multiplayer.get_peers():
				_ping.rpc_id(peer, _clock)
		for id in due:
			_respawn(id)
		if was_playing != state.is_playing():
			_changed()
		if state.match_due():
			_new_match()
	_since_sync += delta
	if _since_sync >= RESYNC_INTERVAL:
		_dirty = true
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


## The host checks and carries out a shot from `weapon` that `shooter`
## reports; by default, the weapon the host last saw in their hands.
func _shot(shooter: int, origin: Vector3, direction: Vector3, seed: int, weapon_id := "") -> void:
	var record := state.player(shooter)
	var shooter_node: Node3D = world.player_node(shooter)
	if record == null or not record.alive or shooter_node == null:
		return
	if weapon_id.is_empty():
		weapon_id = record.weapon
	var weapon := Weapons.find(weapon_id)
	if weapon == null or not record.weapons.has(weapon_id):
		return
	if not origin.is_finite() or not direction.is_finite() or direction.length() < 0.5:
		return
	if not ShotRules.origin_believable(origin, shooter_node.head.global_position):
		return
	if not _rules_for(shooter).allow_shot(weapon, _clock):
		return
	if record.is_protected():
		record.protected_for = 0.0
		_changed()
	if weapon.fires_projectile():
		_launch(shooter, weapon, origin, direction)
		return

	var targets := _targets_except(shooter, now() - rewind_for(shooter))
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
		_harm(shooter, victim, damage_to[victim], origin, weapon.id, headshot[victim])


## Feet of every player in play other than `except`, by id: where they are,
## or, given a time, where the host saw them then.
func _targets_except(except: int, then := -1.0) -> Dictionary:
	var targets := {}
	for node: Node3D in world.players.get_children():
		var id := node.name.to_int()
		var target := state.player(id)
		if id != except and target != null and target.alive and not node.is_queued_for_deletion():
			var history: PositionHistory = _histories.get(id)
			targets[id] = history.at(then) if then >= 0.0 and history != null and not history.is_empty() else node.global_position
	return targets


## The host's clock for positions and shots, in seconds.
static func now() -> float:
	return Time.get_ticks_usec() / 1000000.0


## How far back to look for where `shooter`'s targets were when they fired.
## They saw the others a moment behind (InterpolationBuffer.DELAY), as others'
## movement came to them through the host, and their shot took half a round
## trip to arrive: a round trip and the delay in all. The host's own view has
## only the delay.
func rewind_for(shooter: int) -> float:
	return minf(round_trip(shooter) + InterpolationBuffer.DELAY, MAX_REWIND)


## Shortest time between two put-backs of the same player, so that a player
## whose updates are all rejected isn't flooded with them.
const PUT_BACK_INTERVAL := 0.5


func _check_for(id: int) -> MovementCheck:
	if not _checks.has(id):
		_checks[id] = MovementCheck.new()
	return _checks[id]


## Host only: player `id`'s update arrived at `time` placing them at `at`,
## after being moved at once `teleports` times. Movement that couldn't have
## happened is ignored, and the player is put back where they last were.
func on_moved(time: float, at: Vector3, teleports: int, id: int) -> void:
	if not is_host():
		return
	var space := world.get_world_3d().direct_space_state
	var waist := Vector3.UP * MIDDLE
	var blocked := func(from: Vector3, to: Vector3) -> bool:
		return not space.intersect_ray(PhysicsRayQueryParameters3D.create(from + waist, to + waist, WORLD_MASK)).is_empty()
	var ground := PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.1, at + Vector3.DOWN * 0.6, WORLD_MASK)
	var on_ground := not space.intersect_ray(ground).is_empty()
	var check := _check_for(id)
	if check.check(time, at, teleports, blocked, on_ground):
		record_position(time, at, id)
		return
	if time - _put_back_at.get(id, -INF) < PUT_BACK_INTERVAL:
		return
	_put_back_at[id] = time
	put_backs += 1
	var back := check.last_position()
	check.expect_move(back)
	if multiplayer.get_peers().has(id):
		_put_back.rpc_id(id, back)


## The host put this player back where they last were.
@rpc("authority", "call_remote", "reliable")
func _put_back(at: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.put_back(at)


## Player `id` has moved at once to somewhere new; where they were before
## says nothing about where they are, so it is forgotten.
func forget_positions(id: int) -> void:
	_histories.erase(id)


## Host only: player `id` was at `at` at time `time` on the host's clock.
func record_position(time: float, at: Vector3, id: int) -> void:
	if not is_host():
		return
	if not _histories.has(id):
		_histories[id] = PositionHistory.new()
	_histories[id].push(time, at)


## `attacker` hurts `victim` for `amount` with `weapon`, from `from`, and
## everyone who needs to know is told.
func _harm(attacker: int, victim: int, amount: float, from: Vector3, weapon: String, head: bool) -> void:
	var result := state.damage(attacker, victim, amount)
	if result.dealt > 0:
		_changed()
		_tell_hit(attacker, victim, result.dealt, result.knocked_out, head)
		_tell_hurt(victim, attacker, from, result.dealt)
	if result.knocked_out:
		_announce_knockout(state.name_of(attacker), state.name_of(victim), weapon, head)


func _launch(shooter: int, weapon: WeaponStats, origin: Vector3, direction: Vector3) -> void:
	var projectile := Projectile.new(_next_projectile, shooter, weapon, origin, direction)
	_next_projectile += 1
	_projectiles[projectile.id] = projectile
	_projectile_seen.rpc(projectile.id, weapon.id, origin, projectile.velocity)
	projectile_launched.emit(projectile.id, weapon.id, origin, projectile.velocity)


func _physics_process(delta: float) -> void:
	if not is_host():
		return
	# The host's own player moves here, so its history is kept here too.
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		record_position(now(), own.global_position, own.name.to_int())
	_collect_pickups()
	if _projectiles.is_empty():
		return
	var wall := func(from: Vector3, to: Vector3) -> float:
		var query := PhysicsRayQueryParameters3D.create(from, to, WORLD_MASK)
		var hit := world.get_world_3d().direct_space_state.intersect_ray(query)
		return from.distance_to(hit.position) if not hit.is_empty() else INF
	for id: int in _projectiles.keys():
		var projectile: Projectile = _projectiles[id]
		var burst: Variant = projectile.advance(delta, wall, _targets_except(projectile.shooter))
		if burst != null:
			_projectiles.erase(id)
			_burst(projectile, burst)


## The host hands each pickup that is there to the first player in play
## standing on its pad who has a use for it.
func _collect_pickups() -> void:
	var pads: Array = world.pickup_pads()
	for index in mini(pads.size(), state.pickups.size()):
		if not state.pickup_ready(index):
			continue
		var pad: Dictionary = pads[index]
		for node: Node3D in world.players.get_children():
			var id := node.name.to_int()
			if node.is_queued_for_deletion() or not Pickups.within_reach(pad.position, node.global_position):
				continue
			var outcome := state.collect(index, pad.kind, id)
			if outcome.is_empty():
				continue
			if outcome == "refill":
				_rules_for(id).refill(pad.kind)
				_tell_refill(id, pad.kind)
			_changed()
			_pickup_seen.rpc(id, pad.kind)
			picked_up.emit(id, pad.kind)
			break


func _tell_refill(id: int, weapon: String) -> void:
	if id == multiplayer.get_unique_id():
		_refilled(weapon)
	elif multiplayer.get_peers().has(id):
		_refilled.rpc_id(id, weapon)


## A pickup refilled a weapon this player already carried.
@rpc("authority", "call_remote", "reliable")
func _refilled(weapon: String) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null and own.arsenal.has(weapon):
		own.arsenal[weapon].refill()


@rpc("authority", "call_remote", "reliable")
func _pickup_seen(id: int, kind: String) -> void:
	picked_up.emit(id, kind)


## A projectile bursts at `at`, hurting and shoving everyone in reach who
## isn't behind a wall from it. Its shooter is shoved but not hurt.
func _burst(projectile: Projectile, at: Vector3) -> void:
	_burst_seen.rpc(projectile.id, projectile.weapon.id, at)
	projectile_burst.emit(projectile.id, projectile.weapon.id, at)
	var weapon := projectile.weapon
	var space := world.get_world_3d().direct_space_state
	# A burst against a wall is at its face; sight lines are checked from a
	# hand's width back along the way it came, so the wall itself counts.
	var eye := at - projectile.velocity.normalized() * 0.1 if projectile.velocity.length() > 0.001 else at
	for node: Node3D in world.players.get_children():
		var id := node.name.to_int()
		var record := state.player(id)
		if record == null or not record.alive or node.is_queued_for_deletion():
			continue
		var middle := node.global_position + Vector3.UP * MIDDLE
		if at.distance_to(middle) >= weapon.splash_radius:
			continue
		var query := PhysicsRayQueryParameters3D.create(eye, middle, WORLD_MASK)
		if not space.intersect_ray(query).is_empty():
			continue
		_tell_push(id, Splash.push(at, middle, weapon.knockback, weapon.splash_radius))
		if id != projectile.shooter:
			_harm(projectile.shooter, id, Splash.damage_at(at.distance_to(middle), weapon.splash_damage, weapon.splash_radius), at, weapon.id, false)


func _tell_push(id: int, push: Vector3) -> void:
	if push == Vector3.ZERO:
		return
	_check_for(id).pushed(now(), push)
	if id == multiplayer.get_unique_id():
		_pushed(push)
	elif multiplayer.get_peers().has(id):
		_pushed.rpc_id(id, push)


## A burst shoved this player. Players move themselves, so the host can only
## tell them.
@rpc("authority", "call_remote", "reliable")
func _pushed(push: Vector3) -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null:
		own.push(push)


@rpc("authority", "call_remote", "reliable")
func _projectile_seen(id: int, weapon: String, from: Vector3, velocity: Vector3) -> void:
	projectile_launched.emit(id, weapon, from, velocity)


@rpc("authority", "call_remote", "reliable")
func _burst_seen(id: int, weapon: String, at: Vector3) -> void:
	projectile_burst.emit(id, weapon, at)


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


func _reload(id: int, weapon_id: String) -> void:
	var record := state.player(id)
	var weapon := Weapons.find(weapon_id)
	if record != null and weapon != null and record.weapons.has(weapon_id):
		_rules_for(id).reload(weapon, _clock)


func _select(id: int, weapon: String) -> void:
	var record := state.player(id)
	if record != null and record.weapons.has(weapon) and record.weapon != weapon:
		record.weapon = weapon
		_changed()


## The host brings a knocked-out player back, at the spot farthest from the
## players who could hurt them.
func _respawn(id: int) -> void:
	var others: Array[Vector3] = []
	for node: Node3D in world.players.get_children():
		var other := state.player(node.name.to_int())
		if other != null and other.id != id and other.alive and not state.are_allies(id, other.id):
			others.append(node.global_position)
	var spots: Array[Vector3] = world.spawn_positions()
	_bring_back(id, spots[SpawnChooser.farthest(spots, others)])


## The host starts the next match, with everyone back in play at a spot of
## their own.
func _new_match() -> void:
	state.start_match()
	var spots: Array[Vector3] = world.spawn_positions()
	spots.shuffle()
	var index := 0
	for id: int in state.players:
		_bring_back(id, spots[index % spots.size()])
		index += 1
	_changed()


func _bring_back(id: int, at: Vector3) -> void:
	forget_positions(id)
	_check_for(id).expect_move(at)
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
func _request_shot(origin: Variant, direction: Variant, seed: Variant, weapon: Variant) -> void:
	if is_host() and origin is Vector3 and direction is Vector3 and seed is int and weapon is String:
		_shot(multiplayer.get_remote_sender_id(), origin, direction, seed, weapon)


@rpc("any_peer", "call_remote", "reliable")
func _request_reload(weapon: Variant) -> void:
	if is_host() and weapon is String:
		_reload(multiplayer.get_remote_sender_id(), weapon)


@rpc("any_peer", "call_remote", "reliable")
func _request_select(weapon: Variant) -> void:
	if is_host() and weapon is String:
		_select(multiplayer.get_remote_sender_id(), weapon)


@rpc("any_peer", "call_remote", "reliable")
func _introduce(wanted: Variant) -> void:
	if is_host() and wanted is String:
		_rename(multiplayer.get_remote_sender_id(), wanted)


@rpc("authority", "call_remote", "reliable")
func _receive_state(data: Variant) -> void:
	if data is Dictionary:
		state = MatchState.from_dict(data)
		_has_state = true
		state_changed.emit()
