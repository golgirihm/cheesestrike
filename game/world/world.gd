extends Node3D
## The arena and everyone in it. The host spawns and removes a player node per
## connected peer under Players, and the MultiplayerSpawner mirrors those nodes
## on every client.
##
## The arena itself is one of the Maps, loaded as the child Map. The host
## loads the one it chose before the session starts; everyone else loads the
## one named in the match state when it arrives.

const PLAYER := preload("res://player/player.tscn")
## Physics layers a shot stops at, for showing it: the arena and players.
const SHOT_VIEW_MASK := 3

@onready var players: Node3D = $Players
@onready var spawner: MultiplayerSpawner = $PlayerSpawner
@onready var match_: Node = $Match
@onready var effects: Node3D = $Effects

var sound_cues: Node

## The loaded arena, and which of the Maps it is.
var map: Node3D
var map_id := ""
var spawn_points: Node3D
var menu_camera: Camera3D

# Which starting spot each player holds. Only the host uses it.
var _spawn_slots: SpawnSlots


func _ready() -> void:
	spawner.spawn_function = _create_player
	match_.shot_shown.connect(_show_others_shot)
	match_.projectile_launched.connect(effects.launch_projectile)
	match_.projectile_burst.connect(effects.burst_projectile)
	match_.state_changed.connect(_on_state_changed)
	sound_cues = preload("res://audio/sound_cues.gd").new(self)
	add_child(sound_cues)
	load_map(Maps.DEFAULT)
	menu_camera.make_current()


## Replaces the arena with map `id`, unless it is already loaded.
func load_map(id: String) -> void:
	if not Maps.exists(id):
		id = Maps.DEFAULT
	if id == map_id:
		return
	var was_menu_view := menu_camera != null and menu_camera.current
	if map != null:
		remove_child(map)
		map.queue_free()
	map = Maps.scene(id).instantiate()
	map.name = "Map"
	add_child(map)
	move_child(map, 0)
	map_id = id
	spawn_points = map.get_node("SpawnPoints")
	menu_camera = map.get_node("MenuCamera")
	if was_menu_view:
		menu_camera.make_current()
	_build_pickups()


## Begins the session. `settings` are the host's choice, and only used there.
func start(settings := MatchSettings.new()) -> void:
	if not multiplayer.is_server():
		match_.start()
		return
	load_map(settings.map)
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


## Whether this machine has the arena the host chose, so its player can move.
func ready_to_play() -> bool:
	return match_.has_state() and map_id == match_.state.settings.map


func _on_state_changed() -> void:
	if match_.has_state():
		load_map(match_.state.settings.map)
	_show_pickups()


func _add_player(id: int) -> void:
	match_.add_player(id)
	spawner.spawn([id, spawn_points.get_child(_spawn_slots.assign(id)).global_position])
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _remove_player(id: int) -> void:
	_spawn_slots.release(id)
	match_.remove_player(id)
	var player := players.get_node_or_null(str(id))
	if player != null:
		player.queue_free()
		print("player left: %d" % id)


# A model floating over each pickup pad, shown while its pickup is there.
func _build_pickups() -> void:
	for pad: Node3D in map.get_node("Pickups").get_children():
		var kind := String(pad.get_meta("kind", Pickups.HEALTH))
		var item := PickupModels.build(kind)
		item.name = "Item"
		pad.add_child(item)
		pad.add_child(PickupModels.base())


func _show_pickups() -> void:
	var pads := map.get_node("Pickups").get_children()
	for index in pads.size():
		pads[index].get_node("Item").visible = match_.state.pickup_ready(index) or match_.state.pickups.size() <= index


func _process(delta: float) -> void:
	for pad: Node3D in map.get_node("Pickups").get_children():
		pad.get_node("Item").rotate_y(delta * 1.5)


## This machine's own player fired: show the shot at once, without waiting to
## hear from the host where it went.
func _show_own_shot(origin: Vector3, direction: Vector3, seed: int, player: Node3D) -> void:
	var weapon: WeaponStats = player.weapon.stats
	sound_cues.own_shot(weapon.id, player.muzzle.global_position)
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
		player.kick()
		effects.show_shot(player.muzzle.global_position, ends)


## The arena's pickup pads, in order: [{"position": Vector3, "kind": String}].
func pickup_pads() -> Array:
	var pads := []
	for pad: Node3D in map.get_node("Pickups").get_children():
		pads.append({"position": pad.global_position, "kind": String(pad.get_meta("kind", Pickups.HEALTH))})
	return pads


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
## player's peer id and where the host chose for them to start.
func _create_player(data: Array) -> Node:
	var player := PLAYER.instantiate()
	player.name = str(data[0])
	player.match_ = match_
	player.world = self
	player.fired.connect(_show_own_shot.bind(player))
	player.received.connect(match_.on_moved.bind(data[0]))
	player.teleported.connect(match_.forget_positions.bind(data[0]))
	player.spawn_position = data[1]
	return player
