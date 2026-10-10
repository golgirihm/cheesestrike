extends GutTest
## Checks the arena's starting spots as authored in the scene.

const WORLD := preload("res://world/world.tscn")
const PLAYER := preload("res://player/player.tscn")

var world: Node3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	# Let the physics server register the arena's walls, floor and blocks.
	await wait_physics_frames(2)


func test_there_is_a_spot_for_every_player() -> void:
	assert_gte(world.spawn_points.get_child_count(), Net.MAX_PLAYERS)


func test_no_two_spots_are_close_enough_to_overlap() -> void:
	var spots: Array[Node] = world.spawn_points.get_children()
	for i in spots.size():
		for j in range(i + 1, spots.size()):
			var gap: float = spots[i].global_position.distance_to(spots[j].global_position)
			assert_gt(gap, 1.0, "%s and %s" % [spots[i].name, spots[j].name])


func test_a_player_fits_at_every_spot() -> void:
	# The player's own collision shape, placed where it would be at each spot.
	var player := PLAYER.instantiate()
	var collider: CollisionShape3D = player.get_node("CollisionShape3D")
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = collider.shape
	var space := world.get_world_3d().direct_space_state
	for spot: Node3D in world.spawn_points.get_children():
		query.transform = Transform3D(Basis.IDENTITY, spot.global_position + collider.position)
		assert_eq(space.intersect_shape(query).size(), 0, "%s is blocked" % spot.name)
	player.free()


func test_a_player_starts_at_their_spot_facing_the_middle() -> void:
	for spot: Node3D in world.spawn_points.get_children():
		# Offline, this process is peer 1, so a player named "1" is its own.
		var player: CharacterBody3D = world._create_player([1, spot.get_index()])
		world.players.add_child(player)
		assert_eq(player.global_position, spot.global_position, spot.name)
		var facing := -player.global_basis.z
		var to_middle := (-spot.global_position * Vector3(1, 0, 1)).normalized()
		assert_gt(facing.dot(to_middle), 0.99, "%s faces away from the middle" % spot.name)
		world.players.remove_child(player)
		player.free()


func test_every_spot_is_just_above_the_floor() -> void:
	var space := world.get_world_3d().direct_space_state
	for spot: Node3D in world.spawn_points.get_children():
		var from := spot.global_position
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.DOWN * 0.5))
		assert_false(hit.is_empty(), "%s has no floor within half a metre below it" % spot.name)
