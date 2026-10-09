extends Node3D
## The arena. The host adds and removes a player node per connected peer under
## Players, and the MultiplayerSpawner mirrors those nodes on every client.

const PLAYER := preload("res://player/player.tscn")

@onready var players: Node3D = $Players
@onready var spawn_points: Node3D = $SpawnPoints
@onready var menu_camera: Camera3D = $MenuCamera


func start() -> void:
	if not multiplayer.is_server():
		return
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


func spawn_position(id: int) -> Vector3:
	return spawn_points.get_child(id % spawn_points.get_child_count()).global_position


func _add_player(id: int) -> void:
	var player := PLAYER.instantiate()
	player.name = str(id)
	players.add_child(player, true)
	print("player joined: %d (%d in session)" % [id, players.get_child_count()])


func _remove_player(id: int) -> void:
	var player := players.get_node_or_null(str(id))
	if player != null:
		player.queue_free()
		print("player left: %d" % id)
