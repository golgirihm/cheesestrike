extends Node3D
## The arena. The host spawns and removes a player node per connected peer
## under Players, and the MultiplayerSpawner mirrors those nodes on every client.

const PLAYER := preload("res://player/player.tscn")

@onready var players: Node3D = $Players
@onready var spawn_points: Node3D = $SpawnPoints
@onready var spawner: MultiplayerSpawner = $PlayerSpawner
@onready var menu_camera: Camera3D = $MenuCamera
@onready var match_: Node = $Match
@onready var effects: Node3D = $Effects

# Which starting spot each player holds. Only the host uses it.
var _spawn_slots: SpawnSlots


func _ready() -> void:
	spawner.spawn_function = _create_player
	match_.shot_shown.connect(_show_others_shot)
	match_.projectile_launched.connect(effects.launch_projectile)
	match_.projectile_burst.connect(effects.burst_projectile)


## Begins the session. `settings` are the host's choice, and only used there.
func start(settings := MatchSettings.new()) -> void:
	if not multiplayer.is_server():
		match_.start()
		return
	_spawn_slots = SpawnSlots.new(spawn_points.get_child_count())
	multiplayer.peer_connected.connect(_add_player)
	multiplayer.peer_disconnected.connect(_remove_player)
	match_.start(settings)
	_add_player(1)


func stop() -> void:
	if multiplayer.peer_connected.is_connected(_add_player):
		multiplayer.peer_connected.disconnect(_add_player)
		multiplayer.peer_disconnected.disconnect(_remove_player)
	for player in players.get_children():
		player.queue_free()
	match_.stop()
	menu_camera.make_current()


func _add_player(id: int) -> void:
	match_.add_player(id)
	spawner.spawn([id, _spawn_slots.assign(id)])
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _remove_player(id: int) -> void:
	_spawn_slots.release(id)
	match_.remove_player(id)
	var player := players.get_node_or_null(str(id))
	if player != null:
		player.queue_free()
		print("player left: %d" % id)


## Physics layers a shot stops at, for showing it: the arena and players.
const SHOT_VIEW_MASK := 3


## This machine's own player fired: show the shot at once, without waiting to
## hear from the host where it went.
func _show_own_shot(origin: Vector3, direction: Vector3, seed: int, player: Node3D) -> void:
	var weapon: WeaponStats = player.weapon.stats
	if weapon.fires_projectile():
		# The projectile itself shows once the host launches it.
		effects.show_shot(player.muzzle.global_position, [])
		return
	var space := get_world_3d().direct_space_state
	var ends := []
	for pellet in HitScan.pellet_directions(direction, weapon.pellets, weapon.spread_degrees, seed):
		var query := PhysicsRayQueryParameters3D.create(origin, origin + pellet * weapon.max_range, SHOT_VIEW_MASK, [player.get_rid()])
		var hit := space.intersect_ray(query)
		ends.append(hit.position if not hit.is_empty() else origin + pellet * weapon.max_range)
	effects.show_shot(player.muzzle.global_position, ends)


func _show_others_shot(shooter: int, ends: Array) -> void:
	var player := player_node(shooter)
	if player != null:
		effects.show_shot(player.muzzle.global_position, ends)


## The player node for peer `id`, or null.
func player_node(id: int) -> Node3D:
	var node := players.get_node_or_null(str(id))
	return node if node != null and not node.is_queued_for_deletion() else null


func spawn_positions() -> Array[Vector3]:
	var spots: Array[Vector3] = []
	for spot: Node3D in spawn_points.get_children():
		spots.append(spot.global_position)
	return spots


## Runs on every peer, with the data the host passed to `spawner.spawn`: the
## player's peer id and the starting spot the host chose for them.
func _create_player(data: Array) -> Node:
	var player := PLAYER.instantiate()
	player.name = str(data[0])
	player.match_ = match_
	player.fired.connect(_show_own_shot.bind(player))
	player.spawn_position = spawn_points.get_child(data[1]).global_position
	return player
