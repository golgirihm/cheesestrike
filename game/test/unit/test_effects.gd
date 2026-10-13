extends GutTest

const EFFECTS := preload("res://effects/effects.gd")


func test_a_tracer_sits_between_its_ends() -> void:
	var from := Vector3(1, 2, 3)
	var to := Vector3(4, 2, -1)
	var placed := EFFECTS.tracer_transform(from, to)
	assert_almost_eq(placed.origin.distance_to((from + to) / 2.0), 0.0, 0.0001)
	# The box is a unit long along its own -Z, stretched to the line's length.
	var tip := placed * Vector3(0, 0, -0.5)
	var tail := placed * Vector3(0, 0, 0.5)
	assert_almost_eq(tip.distance_to(to), 0.0, 0.001)
	assert_almost_eq(tail.distance_to(from), 0.0, 0.001)


func test_a_vertical_tracer_works() -> void:
	var placed := EFFECTS.tracer_transform(Vector3.ZERO, Vector3(0, 10, 0))
	assert_true(placed.basis.determinant() != 0.0)
	assert_almost_eq((placed * Vector3(0, 0, -0.5)).distance_to(Vector3(0, 10, 0)), 0.0, 0.001)


func test_a_zero_length_tracer_does_not_break() -> void:
	var placed := EFFECTS.tracer_transform(Vector3.ONE, Vector3.ONE)
	assert_true(placed.origin.is_finite())


func test_a_shot_leaves_a_flash_and_a_tracer_per_pellet_that_soon_go() -> void:
	var effects: Node3D = EFFECTS.new()
	add_child_autofree(effects)
	effects.show_shot(Vector3.ZERO, [Vector3(0, 0, -5), Vector3(1, 0, -5), "not a point"])
	assert_eq(effects.showing("flash"), 1)
	assert_eq(effects.showing("tracer"), 2, "one for each pellet")
	await wait_seconds(0.2)
	assert_eq(effects.showing("flash"), 0)
	assert_eq(effects.showing("tracer"), 0)


const WORLD := preload("res://world/world.tscn")


func _arena(map := "box") -> Node3D:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map(map)
	await wait_physics_frames(2)
	return world


func after_each() -> void:
	Settings.set_value("quality", "")


func test_a_shot_leaves_a_mark_where_it_lands_on_the_arena() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	var effects: Node3D = world.effects
	effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)])
	assert_eq(effects.marks_showing(), 1)
	var mark: Node3D = effects._marks[0].node
	assert_almost_eq(mark.global_position.y, 0.0, 0.05, "lying on the floor")
	assert_almost_eq(mark.global_position.z, -4.0, 0.05)
	assert_gte(effects.showing("puff"), 2, "dust off the floor, and smoke at the muzzle")


func test_a_shot_that_ends_in_the_air_or_at_the_edge_leaves_no_mark() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 6, -4)])
	assert_eq(world.effects.marks_showing(), 0, "as on a player, or at the end of its range")
	# The north edge is solid at z -16, and nothing is drawn there.
	world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 3, -16)])
	assert_eq(world.effects.marks_showing(), 0)


func test_marks_fade_and_go() -> void:
	var world: Node3D = await _arena()
	var effects: Node3D = world.effects
	effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)])
	effects._process(effects.MARK_LIFE - effects.MARK_FADE / 2.0)
	assert_eq(effects.marks_showing(), 1)
	assert_lt(effects._marks[0].material.albedo_color.a, 0.5, "fading")
	effects._process(effects.MARK_FADE)
	assert_eq(effects.marks_showing(), 0)


func test_only_so_many_marks_are_kept_and_fewer_on_lower_graphics() -> void:
	assert_lt(EFFECTS.mark_limit(GraphicsQuality.LOW), EFFECTS.mark_limit(GraphicsQuality.MEDIUM))
	assert_lt(EFFECTS.mark_limit(GraphicsQuality.MEDIUM), EFFECTS.mark_limit(GraphicsQuality.HIGH))
	Settings.set_value("quality", GraphicsQuality.LOW)
	var world: Node3D = await _arena()
	for shot in EFFECTS.mark_limit(GraphicsQuality.LOW) + 10:
		world.effects.show_shot(Vector3(0, 3, 0), [Vector3(shot * 0.1, 0, -4)])
	assert_eq(world.effects.marks_showing(), EFFECTS.mark_limit(GraphicsQuality.LOW))


func test_low_graphics_has_marks_but_no_dust_casings_or_smoke() -> void:
	assert_false(EFFECTS.extras(GraphicsQuality.LOW))
	assert_true(EFFECTS.extras(GraphicsQuality.MEDIUM))
	Settings.set_value("quality", GraphicsQuality.LOW)
	var world: Node3D = await _arena()
	world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)], Vector3.RIGHT)
	assert_eq(world.effects.marks_showing(), 1)
	assert_eq(world.effects.showing("puff"), 0)
	assert_eq(world.effects.casings_showing(), 0)


func test_a_casing_is_thrown_aside_bounces_and_is_cleared_away() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	var effects: Node3D = world.effects
	effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)], Vector3.RIGHT)
	assert_eq(effects.casings_showing(), 1)
	var casing: Node3D = effects._casings[0].node
	for frame in 20:
		effects._process(1.0 / 60.0)
	assert_gt(casing.global_position.x, 0.3, "out to the side")
	for frame in 50:
		effects._process(1.0 / 60.0)
	assert_almost_eq(casing.global_position.y, 0.02, 0.3, "down on the floor")
	effects._process(effects.CASING_LIFE)
	assert_eq(effects.casings_showing(), 0)
	effects.show_shot(Vector3(0, 3, 0), [], Vector3.RIGHT)
	assert_eq(effects.casings_showing(), 0, "nothing was fired, as when a wheel is launched")


func test_only_so_many_casings_lie_about() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	for shot in 60:
		world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 6, -4)], Vector3.RIGHT)
	assert_lte(world.effects.casings_showing(), 24)


func test_what_a_shot_knocks_off_depends_on_what_it_hits() -> void:
	var world: Node3D = await _arena("city")
	var geometry: Node = world.map.get_node("Geometry")
	assert_eq(Surfaces.kind(geometry.get_node("Block1")), Surfaces.CHEESE)
	assert_eq(Surfaces.kind(geometry.get_node("Car1")), Surfaces.METAL)
	assert_eq(Surfaces.kind(geometry.get_node("Bus1")), Surfaces.METAL)
	assert_eq(Surfaces.kind(geometry.get_node("Floor")), Surfaces.STONE)
	assert_true(Surfaces.sparks(geometry.get_node("Car1")), "sparks off metal")
	assert_false(Surfaces.sparks(geometry.get_node("Block1")))
	assert_true(Scenery.is_yellow(Surfaces.dust(geometry.get_node("Block1"))), "yellow crumbs off cheese")
	assert_false(Scenery.is_yellow(Surfaces.dust(geometry.get_node("Floor"))))
	assert_eq(Surfaces.dust(geometry.get_node("BuildingNorthEast")), Surfaces.DUST, "plain dust off a wall of windows")
	assert_eq(Surfaces.dust(null), Surfaces.DUST)
	assert_eq(Surfaces.kind(null), Surfaces.STONE)
