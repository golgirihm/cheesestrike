extends GutTest
## Jump pads and ladders: the rules, each arena's own, and a player using them.

const WORLD := preload("res://world/world.tscn")

var world: Node3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)


func _load(id: String) -> void:
	world.load_map(id)
	world.match_.start(MatchSettings.from_dict({"map": id}))
	await wait_physics_frames(2)


func _own(at: Vector3, yaw := 0.0) -> CharacterBody3D:
	world.match_.add_player(1)
	var player: CharacterBody3D = world._create_player([1, at])
	world.players.add_child(player)
	player.global_position = at
	player.rotation.y = yaw
	return player


func test_a_pad_throws_only_whoever_stands_on_it() -> void:
	var pads := [{"position": Vector3(5, 0, 5), "launch": Vector3(0, 12, -6)}]
	assert_eq(Fixtures.launch_at(pads, Vector3(5, 0, 5)), Vector3(0, 12, -6))
	assert_eq(Fixtures.launch_at(pads, Vector3(5 + Fixtures.PAD_RADIUS - 0.01, 0.1, 5)), Vector3(0, 12, -6))
	assert_null(Fixtures.launch_at(pads, Vector3(5 + Fixtures.PAD_RADIUS + 0.1, 0, 5)), "beside it")
	assert_null(Fixtures.launch_at(pads, Vector3(5, 3, 5)), "passing over it")
	assert_null(Fixtures.launch_at([], Vector3(5, 0, 5)))


func test_a_ladder_is_held_from_its_foot_to_its_top() -> void:
	var ladders := [{"position": Vector3(2, 0, 2), "height": 4.0, "facing": Vector3.FORWARD}]
	assert_false(Fixtures.ladder_at(ladders, Vector3(2, 0, 2)).is_empty())
	assert_false(Fixtures.ladder_at(ladders, Vector3(2.5, 3.9, 2.5)).is_empty())
	assert_true(Fixtures.ladder_at(ladders, Vector3(2, 4.5, 2)).is_empty(), "off the top")
	assert_true(Fixtures.ladder_at(ladders, Vector3(2 + Fixtures.LADDER_REACH + 0.1, 1, 2)).is_empty(), "out of reach")
	assert_true(Fixtures.ladder_at([], Vector3(2, 1, 2)).is_empty())


func test_the_flight_from_a_pad_is_an_arc() -> void:
	var gravity := 20.0
	var launch := Vector3(10, 12, 0)
	assert_eq(Fixtures.flight(Vector3.ZERO, launch, gravity, 0.0), Vector3.ZERO)
	var top := Fixtures.flight(Vector3.ZERO, launch, gravity, 0.6)
	assert_almost_eq(top.y, 3.6, 0.001, "at its highest")
	assert_almost_eq(Fixtures.flight(Vector3.ZERO, launch, gravity, 1.2).y, 0.0, 0.001, "and back down")
	assert_almost_eq(Fixtures.flight(Vector3.ZERO, launch, gravity, 1.2).x, 12.0, 0.001)


func test_every_arenas_pads_throw_players_somewhere_they_can_stand() -> void:
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	for id: String in Maps.ORDER:
		await _load(id)
		var space := world.get_world_3d().direct_space_state
		var edge := Scenery.bounds(world.map.get_node("Geometry"))
		assert_gt(world.fixtures.pads.size(), 0, "%s has a jump pad" % id)
		for pad: Dictionary in world.fixtures.pads:
			# Along the arc until it comes down onto something.
			var landed := Vector3.INF
			var last: Vector3 = pad.position + Vector3.UP * 0.1
			for step in range(1, 200):
				var at := Fixtures.flight(pad.position, pad.launch, gravity, step * 0.02) + Vector3.UP * 0.1
				var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(last, at, Match.WORLD_MASK))
				if not hit.is_empty():
					landed = hit.position
					assert_gt((hit.normal as Vector3).y, 0.7, "%s: lands on top of something, not into its side" % id)
					break
				last = at
			assert_ne(landed, Vector3.INF, "%s: the arc comes down" % id)
			assert_true(edge.grow(-BodyShape.RADIUS).has_point(Vector2(landed.x, landed.z)), "%s: inside the arena, clear of its edge" % id)
			assert_gt(Vector2(landed.x - pad.position.x, landed.z - pad.position.z).length(), 6.0, "%s: and somewhere else" % id)
			# The pad itself rests on the floor, clear of where players start.
			assert_false(space.intersect_ray(PhysicsRayQueryParameters3D.create(pad.position + Vector3.UP * 0.2, pad.position + Vector3.DOWN * 0.3, Match.WORLD_MASK)).is_empty(), id)
			for spot in world.spawn_positions():
				assert_gt(Vector2(spot.x - pad.position.x, spot.z - pad.position.z).length(), Fixtures.PAD_RADIUS + BodyShape.RADIUS, "%s: nobody starts on a pad" % id)
			for other: Dictionary in world.pickup_pads():
				assert_gt(pad.position.distance_to(other.position), Fixtures.PAD_RADIUS + 1.0, "%s: a pad and a pickup share a spot" % id)


func test_every_fixture_is_drawn() -> void:
	for id: String in Maps.ORDER:
		await _load(id)
		for group: String in ["JumpPads", "Ladders"]:
			var holder: Node = world.map.get_node_or_null(group)
			if holder == null:
				continue
			for fixture: Node3D in holder.get_children():
				var drawn := fixture.find_children("*", "MeshInstance3D", true, false)
				assert_gt(drawn.size(), 0, "%s: %s" % [id, fixture.name])
				# A pad's rings and arrows, or a ladder's rails and rungs.
				var faces := 0
				for part: MeshInstance3D in drawn:
					faces += part.mesh.get_faces().size() / 3
				assert_gt(faces, 200, "%s: %s is more than a box" % [id, fixture.name])


func test_stepping_on_a_pad_throws_the_owner_onto_the_mesa() -> void:
	await _load("canyon")
	var pad: Dictionary = world.fixtures.pads[0]
	var own := _own(pad.position + Vector3(0, 0.05, 0))
	var before: int = Sounds.counts.get("pad", 0)
	var highest := 0.0
	for frame in 150:
		await wait_physics_frames(1)
		highest = maxf(highest, own.global_position.y)
		if frame > 20 and own.is_on_floor():
			break
	assert_eq(Sounds.counts.get("pad", 0), before + 1, "thrown once")
	assert_gt(highest, 4.0, "high over the arena")
	assert_almost_eq(own.global_position.y, 3.0, 0.2, "and down onto the mesa")
	assert_lt(absf(own.global_position.x), 8.0)
	assert_lt(own.global_position.z, -14.0)


func test_the_host_allows_for_every_arenas_pads() -> void:
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	var open := func(_from: Vector3, _to: Vector3) -> bool: return false
	for id: String in Maps.ORDER:
		await _load(id)
		for pad: Dictionary in world.fixtures.pads:
			var check := MovementCheck.new()
			check.check(10.0, pad.position, 0, open, true)
			# As Match.on_moved does for a player last seen on a pad.
			check.pushed(10.0, Fixtures.launch_at(world.fixtures.pads, check.last_position()))
			var refused := 0
			# Updates a fifth of a second apart, as on a poor connection.
			for update in range(1, 6):
				var at := Fixtures.flight(pad.position, pad.launch, gravity, update * 0.2)
				if not check.check(10.0 + update * 0.2, at, 0, open, false):
					refused += 1
			assert_eq(refused, 0, "%s: the flight from a pad is allowed" % id)


func test_the_owner_climbs_a_ladder_onto_a_roof() -> void:
	await _load("city")
	var ladder: Dictionary = world.fixtures.ladders[0]
	var facing: Vector3 = ladder.facing
	# Standing at its foot, as close to the house as a player gets, facing it.
	var own := _own(ladder.position - facing * 0.4 + Vector3(0, 0.05, 0), atan2(-facing.x, -facing.z))
	await wait_physics_frames(5)
	assert_almost_eq(own.global_position.y, 0.0, 0.1, "holding on doesn't lift them")
	Input.action_press("move_forward")
	for frame in 480:
		await wait_physics_frames(1)
		if own.global_position.y > ladder.height - 0.3 and own.is_on_floor():
			break
	Input.action_release("move_forward")
	assert_almost_eq(own.global_position.y, ladder.height, 0.3, "up on the roof")
	assert_gt((own.global_position - ladder.position).dot(facing), 1.0, "and walked onto it")
	# A ladder is slower than a jump, so the host's limit on rising covers it.
	assert_lt(Fixtures.LADDER_SPEED, MovementCheck.MAX_RISE_SPEED)


func test_letting_go_of_the_keys_holds_on_to_a_ladder() -> void:
	await _load("city")
	var ladder: Dictionary = world.fixtures.ladders[0]
	var facing: Vector3 = ladder.facing
	var own := _own(ladder.position - facing * 0.4 + Vector3(0, 2.0, 0), atan2(-facing.x, -facing.z))
	await wait_seconds(0.5)
	assert_almost_eq(own.global_position.y, 2.0, 0.1, "neither climbing nor falling")
	Input.action_press("move_back")
	await wait_seconds(0.2)
	Input.action_release("move_back")
	assert_lt(own.global_position.y, 1.6, "backing away goes down")
