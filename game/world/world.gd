extends Node3D
## The arena. The host spawns and removes a player node per connected peer
## under Players, and the MultiplayerSpawner mirrors those nodes on every client.

const PLAYER := preload("res://player/player.tscn")

@onready var players: Node3D = $Players
@onready var spawn_points: Node3D = $SpawnPoints
@onready var spawner: MultiplayerSpawner = $PlayerSpawner
@onready var menu_camera: Camera3D = $MenuCamera

# Which starting spot each player holds. Only the host uses it.
var _spawn_slots: SpawnSlots


func _ready() -> void:
	spawner.spawn_function = _create_player


func start() -> void:
	if not multiplayer.is_server():
		return
	_spawn_slots = SpawnSlots.new(spawn_points.get_child_count())
	multiplayer.peer_connected.connect(_add_player)
	multiplayer.peer_disconnected.connect(_remove_player)
	_add_player(1)


func stop() -> void:
	if multiplayer.peer_connected.is_connected(_add_player):
		multiplayer.peer_connected.disconnect(_add_player)
		multiplayer.peer_disconnected.disconnect(_remove_player)
	for player in players.get_children():
		player.queue_free()
	menu_camera.make_current()


func _add_player(id: int) -> void:
	spawner.spawn([id, _spawn_slots.assign(id)])
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _remove_player(id: int) -> void:
	_spawn_slots.release(id)
	var player := players.get_node_or_null(str(id))
	if player != null:
		player.queue_free()
		print("player left: %d" % id)


## Runs on every peer, with the data the host passed to `spawner.spawn`: the
## player's peer id and the starting spot the host chose for them.
func _create_player(data: Array) -> Node:
	var player := PLAYER.instantiate()
	player.name = str(data[0])
	player.spawn_position = spawn_points.get_child(data[1]).global_position
	return player
