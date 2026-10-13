extends GutTest
## Checks every arena as authored: where players start, where pickups lie, and
## that the ways around can be walked.

const WORLD := preload("res://world/world.tscn")
const PLAYER := preload("res://player/player.tscn")
const SAMPLE := preload("res://test/sample_arena.gd")
const EYE := Vector3(0, BodyShape.HEAD_HEIGHT * BodyShape.SIZE, 0)

var world: Node3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.custom_code = SAMPLE.code()


# Every arena that comes with the game, and one laid out in the editor.
func _each_map() -> Array:
	return Maps.ORDER + [Maps.CUSTOM]


func _load(id: String) -> PhysicsDirectSpaceState3D:
	world.load_map(id)
	# Let the physics server register the new arena.
	await wait_physics_frames(2)
	return world.get_world_3d().direct_space_state


func test_every_map_is_listed_with_a_name() -> void:
	for id: String in Maps.ORDER:
		assert_true(Maps.exists(id), id)
		assert_has(Maps.NAMES, id)
	assert_has(Maps.ORDER, Maps.DEFAULT)
	assert_false(Maps.exists("moon"))


func test_an_unknown_map_loads_the_default() -> void:
	world.load_map("moon")
	assert_eq(world.map_id, Maps.DEFAULT)


func test_loading_another_map_replaces_the_last() -> void:
	world.load_map("box")
	world.load_map("canyon")
	await wait_process_frames(1)
	assert_eq(world.get_children().filter(func(child: Node) -> bool: return child.name.begins_with("Map")).size(), 1)
	assert_eq(world.map_id, "canyon")


func test_there_is_a_spot_for_every_player_on_every_map() -> void:
	for id: String in _each_map():
		await _load(id)
		assert_gte(world.spawn_points.get_child_count(), MatchSettings.MAX_PLAYERS, id)


func test_no_two_spots_are_close_enough_to_overlap() -> void:
	for id: String in _each_map():
		await _load(id)
		var spots: Array[Vector3] = world.spawn_positions()
		for i in spots.size():
			for j in range(i + 1, spots.size()):
				assert_gt(spots[i].distance_to(spots[j]), 1.0, "%s: spots %d and %d" % [id, i, j])


func test_a_player_fits_at_every_spot() -> void:
	var player := PLAYER.instantiate()
	var collider: CollisionShape3D = player.get_node("CollisionShape3D")
	var query := PhysicsShapeQueryParameters3D.new()
	query.shape = collider.shape
	for id: String in _each_map():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		for spot in world.spawn_positions():
			query.transform = Transform3D(Basis.IDENTITY, spot + collider.position)
			assert_eq(space.intersect_shape(query).size(), 0, "%s: blocked at %s" % [id, spot])
	player.free()


func test_every_spot_is_just_above_the_floor() -> void:
	for id: String in _each_map():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		for spot in world.spawn_positions():
			var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(spot, spot + Vector3.DOWN * 0.5))
			assert_false(hit.is_empty(), "%s: nothing under %s" % [id, spot])


func test_a_player_starts_at_their_spot_facing_the_middle() -> void:
	for id: String in _each_map():
		await _load(id)
		for spot in world.spawn_positions():
			# Offline, this process is peer 1, so a player named "1" is its own.
			var player: CharacterBody3D = world._create_player([1, spot])
			world.players.add_child(player)
			assert_eq(player.global_position, spot)
			var facing := -player.global_basis.z
			var to_middle: Vector3 = (-spot * Vector3(1, 0, 1)).normalized()
			assert_gt(facing.dot(to_middle), 0.99, "%s: %s faces away from the middle" % [id, spot])
			world.players.remove_child(player)
			player.free()


func test_the_first_two_spots_see_each_other() -> void:
	# The browser tests shoot from the first spot at a player on the second
	# without aiming, as each starts facing the middle.
	for id: String in _each_map():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		var from: Vector3 = world.spawn_positions()[0] + EYE
		var to: Vector3 = world.spawn_positions()[1] + EYE
		assert_true(space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, Match.WORLD_MASK)).is_empty(), "%s: something is in the way" % id)
		var facing := ((to - from) * Vector3(1, 0, 1)).normalized()
		assert_gt(facing.dot((-from * Vector3(1, 0, 1)).normalized()), 0.999, "%s: the second spot isn't straight ahead" % id)


func test_a_launcher_lies_straight_ahead_of_the_first_spot_with_a_clear_walk() -> void:
	# The browser tests walk straight ahead from the first spot to pick it up.
	for id: String in _each_map():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		var start: Vector3 = world.spawn_positions()[0]
		var launchers: Array = world.pickup_pads().filter(func(pad: Dictionary) -> bool: return pad.kind == "launcher")
		assert_eq(launchers.size(), 1, id)
		var pad: Vector3 = launchers[0].position
		var ahead := ((pad - start) * Vector3(1, 0, 1)).normalized()
		assert_gt(ahead.dot((-start * Vector3(1, 0, 1)).normalized()), 0.999, "%s: the launcher isn't straight ahead" % id)
		for height in [0.3, 1.3, BodyShape.HEIGHT - 0.1]:
			var knee := Vector3(0, height, 0)
			assert_true(space.intersect_ray(PhysicsRayQueryParameters3D.create(start + knee, pad + knee, Match.WORLD_MASK)).is_empty(), "%s: the walk is blocked at %s m" % [id, height])


func test_every_pad_rests_on_a_floor_and_holds_a_known_pickup() -> void:
	for id: String in _each_map():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		var kinds := []
		for pad: Dictionary in world.pickup_pads():
			kinds.append(pad.kind)
			assert_true(Pickups.is_kind(pad.kind), "%s: %s" % [id, pad.kind])
			var from: Vector3 = pad.position + Vector3.UP * 0.2
			assert_false(space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.DOWN * 0.5)).is_empty(), "%s: %s pad floats" % [id, pad.kind])
		# Weapons a loadout gives aren't lying about: only the power weapons.
		for weapon: String in Weapons.ids():
			if Weapons.find(weapon).slot == Weapons.POWER:
				assert_has(kinds, weapon, id)
			else:
				assert_does_not_have(kinds, weapon, id)
		assert_has(kinds, Pickups.HEALTH, id)
		assert_has(kinds, Pickups.AMMO, id)


func test_the_canyons_ramps_lead_up_to_its_mesas() -> void:
	var space: PhysicsDirectSpaceState3D = await _load("canyon")
	# Down the middle of each ramp, the ground rises steadily toward the mesa
	# and is never too steep to walk.
	for z in [-18.0, 18.0]:
		for sign in [-1.0, 1.0]:
			var last := -1.0
			for step in 9:
				var x: float = sign * (16.0 - step)
				var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(Vector3(x, 10, z), Vector3(x, -1, z)))
				assert_false(hit.is_empty())
				assert_gte(hit.position.y, last - 0.01, "the ramp at x %s z %s dips" % [x, z])
				if last >= 0.0:
					assert_lt(hit.position.y - last, 0.7, "too steep at x %s z %s" % [x, z])
				last = hit.position.y
			assert_almost_eq(last, 3.0, 0.05, "the ramp reaches the mesa's top")


func test_nobody_can_jump_out_of_any_arena() -> void:
	# The highest a player can be thrown, with a jump and a full burst under
	# them, standing on the highest floor.
	var lift: float = Movement.JUMP_VELOCITY + Weapons.find("launcher").knockback
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	# Stacked blocks make 4 m the highest floor anywhere.
	var highest := 4.0 + lift * lift / (2.0 * gravity) + BodyShape.HEIGHT
	for id: String in _each_map():
		await _load(id)
		var walls: Array = world.map.get_node("Geometry").get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Wall"))
		assert_eq(walls.size(), 4, id)
		for wall: StaticBody3D in walls:
			var shape: BoxShape3D = wall.get_node("CollisionShape3D").shape
			assert_gt(wall.global_position.y + shape.size.y / 2.0, highest, "%s: %s" % [id, wall.name])
