extends GutTest
## Shots through thin cover: the rule, and the host applying it.

const WORLD := preload("res://world/world.tscn")


## Slabs standing across a shot fired along -Z, as [near face, far face]
## distances from the origin, cast into as the physics engine would: a ray
## doesn't meet a slab it starts inside.
func _slabs(slabs: Array) -> Callable:
	return func(from: Vector3, to: Vector3) -> float:
		var nearest := INF
		var forward := to.z < from.z
		for slab: Array in slabs:
			var start := -from.z
			var end := -to.z
			if start > slab[0] and start < slab[1]:
				continue
			var face: float = slab[0] if forward else slab[1]
			if (forward and start <= face and end >= face) or (not forward and start >= face and end <= face):
				nearest = minf(nearest, absf(face - start))
		return nearest


func _path(slabs: Array, max_range := 100.0) -> Dictionary:
	return Penetration.path(Vector3.ZERO, Vector3.FORWARD, max_range, _slabs(slabs))


func test_with_nothing_in_the_way_a_shot_goes_its_whole_range() -> void:
	var way := _path([])
	assert_eq(way.first, 100.0)
	assert_eq(way.reach, 100.0)
	assert_false(way.through)
	assert_eq(Penetration.share(way, 50.0), 1.0)


func test_a_shot_passes_through_thin_cover_and_does_less_behind_it() -> void:
	var way := _path([[10.0, 10.3]])
	assert_almost_eq(way.first, 10.0, 0.001)
	assert_true(way.through)
	assert_eq(way.reach, 100.0, "and carries on")
	assert_eq(Penetration.share(way, 5.0), 1.0, "in front of the cover, all of its damage")
	assert_eq(Penetration.share(way, 20.0), Penetration.DAMAGE_SHARE)
	assert_lt(Penetration.DAMAGE_SHARE, 1.0)


func test_a_shot_stops_at_cover_that_is_too_thick() -> void:
	var way := _path([[10.0, 10.0 + Penetration.MAX_THICKNESS + 0.1]])
	assert_false(way.through)
	assert_almost_eq(way.reach, 10.0, 0.001)
	var wall := _path([[10.0, 60.0]])
	assert_false(wall.through)
	assert_almost_eq(wall.reach, 10.0, 0.001)


func test_cover_just_thin_enough_is_passed_through() -> void:
	assert_true(_path([[10.0, 10.0 + Penetration.MAX_THICKNESS - 0.01]]).through)


func test_a_shot_passes_through_only_one_thing() -> void:
	var way := _path([[10.0, 10.2], [15.0, 15.2]])
	assert_true(way.through)
	assert_almost_eq(way.reach, 15.0, 0.01, "it stops at the second, however thin")


func test_thin_cover_right_at_the_end_of_the_range_stops_the_shot() -> void:
	var way := _path([[10.0, 10.3]], 10.2)
	assert_false(way.through)
	assert_almost_eq(way.reach, 10.0, 0.001)


func test_every_arenas_thick_cover_stops_shots_and_its_ramps_let_them_through() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("canyon")
	await wait_physics_frames(2)
	var cast := Match.wall_caster(world.get_world_3d().direct_space_state)
	# Straight down through the middle of a ramp, which is 0.4 m of wood.
	var ramp := Penetration.path(Vector3(12, 6, -18), Vector3.DOWN, 20.0, cast)
	assert_true(ramp.through, "a ramp")
	assert_almost_eq(ramp.reach, 6.0, 0.05, "down to the floor under it")
	var block := Penetration.path(Vector3(6, 1, 5), Vector3.FORWARD, 20.0, cast)
	assert_false(block.through, "a cheese block")
	assert_almost_eq(block.reach, 6.0, 0.05)
	var mesa := Penetration.path(Vector3(0, 1.5, -5), Vector3.FORWARD, 30.0, cast)
	assert_false(mesa.through, "a mesa")
	var edge := Penetration.path(Vector3(0, 1.5, 0), Vector3.RIGHT, 100.0, cast)
	assert_false(edge.through, "the arena's edge")
	assert_almost_eq(edge.reach, 24.0, 0.05)
