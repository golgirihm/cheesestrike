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
	assert_eq(effects.showing("trail"), 0)
	await wait_seconds(0.2)
	assert_eq(effects.showing("flash"), 0)
	assert_eq(effects.showing("tracer"), 0)


func test_a_tracer_flies_from_the_muzzle_to_where_the_shot_ended() -> void:
	var length := 40.0
	var life := EFFECTS.tracer_life(length)
	assert_eq(EFFECTS.tracer_span(length, 0.0), Vector2.ZERO, "it starts in the muzzle")
	assert_eq(EFFECTS.tracer_span(length, -1.0), Vector2.ZERO)
	# It grows out of the muzzle, flies at its full length, and runs into the end.
	var last := Vector2.ZERO
	var longest := 0.0
	for step in 101:
		var span := EFFECTS.tracer_span(length, life * step / 100.0)
		assert_gte(span.x, last.x)
		assert_gte(span.y, last.y)
		assert_lte(span.y - span.x, EFFECTS.TRACER_LENGTH + 0.001)
		assert_between(span.y, 0.0, length)
		longest = maxf(longest, span.y - span.x)
		last = span
	assert_almost_eq(longest, EFFECTS.TRACER_LENGTH, 0.001)
	assert_almost_eq(last, Vector2(length, length), Vector2.ONE * 0.001, "all of it has arrived")
	var middle := EFFECTS.tracer_span(length, life / 2.0)
	assert_gt(middle.x, 5.0, "in between, it has left the muzzle")
	assert_lt(middle.y, length - 5.0, "and not yet arrived")
	# A shot at something close is a streak no longer than the way there.
	assert_eq(EFFECTS.tracer_span(2.0, 1.0 / 60.0).y, 2.0)
	assert_gt(EFFECTS.tracer_life(100.0), EFFECTS.tracer_life(10.0), "further takes longer")
	assert_lt(EFFECTS.tracer_life(100.0), 0.5, "but is never slow")


func test_the_tracer_shown_moves_along_the_shot_and_goes_when_it_arrives() -> void:
	var effects: Node3D = EFFECTS.new()
	add_child_autofree(effects)
	effects.set_process(false)
	effects.show_shot(Vector3.ZERO, [Vector3(0, 0, -60)])
	var tracer: Node3D = effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "tracer")[0]
	assert_gt(tracer.global_position.z, -4.0, "it starts at the muzzle")
	effects._process(0.1)
	assert_almost_eq(tracer.global_position.z, -(EFFECTS.TRACER_SPEED * (0.1 + 1.0 / 120.0) - EFFECTS.TRACER_LENGTH / 2.0), 0.01)
	assert_almost_eq(tracer.global_basis.get_scale().z, EFFECTS.TRACER_LENGTH, 0.01)
	effects._process(0.2)
	assert_eq(effects.showing("tracer"), 0)


func test_a_rifle_shot_leaves_a_line_that_fades_and_other_shots_do_not() -> void:
	var effects: Node3D = EFFECTS.new()
	add_child_autofree(effects)
	effects.show_shot(Vector3.ZERO, [Vector3(0, 0, -30)], Vector3.ZERO, "blaster")
	assert_eq(effects.showing("trail"), 0)
	for weapon: String in EFFECTS.TRAILED:
		assert_not_null(Weapons.find(weapon), weapon)
	effects.show_shot(Vector3.ZERO, [Vector3(0, 0, -30)], Vector3.ZERO, "rifle")
	assert_eq(effects.showing("trail"), 1)
	var trail: MeshInstance3D = effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "trail")[0]
	assert_almost_eq(trail.global_position, Vector3(0, 0, -15), Vector3.ONE * 0.001, "the whole way, at once")
	assert_almost_eq(trail.global_basis.get_scale().z, 30.0, 0.01)
	var start: float = (trail.material_override as StandardMaterial3D).albedo_color.a
	await wait_seconds(EFFECTS.TRAIL_LIFE / 2.0)
	assert_lt((trail.material_override as StandardMaterial3D).albedo_color.a, start)
	assert_eq(effects.showing("tracer"), 0, "it outlasts the tracer")
	await wait_seconds(EFFECTS.TRAIL_LIFE / 2.0 + 0.1)
	assert_eq(effects.showing("trail"), 0)


func test_a_launchers_wheel_leaves_smoke_behind_it_and_a_grenade_does_not() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	world.effects.set_process(false)
	world.effects.launch_projectile(1, "launcher", Vector3(0, 3, 0), Vector3(0, 0, -20))
	for step in 10:
		world.effects._process(0.02)
	assert_between(world.effects.showing("puff"), 3, 5)
	var wisps: Array = world.effects.get_children().filter(func(child: Node) -> bool: return child is CPUParticles3D)
	assert_lt(wisps[-1].global_position.z, wisps[0].global_position.z - 1.0, "strung out along its path")
	var made: int = world.effects.puffs_made
	world.effects.launch_projectile(2, Weapons.GRENADE, Vector3(5, 3, 0), Vector3(0, 0, -8))
	world.effects.burst_projectile(1, "bow", Vector3.ZERO)
	for step in 10:
		world.effects._process(0.02)
	assert_eq(world.effects.puffs_made, made)
	# Smoke is a lesser effect: low graphics has none.
	Settings.set_value("quality", GraphicsQuality.LOW)
	world.effects.launch_projectile(3, "launcher", Vector3(0, 3, 0), Vector3(0, 0, -20))
	for step in 10:
		world.effects._process(0.02)
	assert_eq(world.effects.puffs_made, made)


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
	assert_eq(Surfaces.dust(geometry.get_node("Building1")), Surfaces.DUST, "plain dust off a wall of windows")
	assert_eq(Surfaces.dust(null), Surfaces.DUST)
	assert_eq(Surfaces.kind(null), Surfaces.STONE)


func test_feet_leave_prints_on_earth_that_fade() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena("canyon")
	var effects: Node3D = world.effects
	effects.footprint(Vector3(0, 0, 5), 0.0, world.map)
	effects.footprint(Vector3(0, 0, 3.3), 0.0, world.map)
	assert_eq(effects.prints_showing(), 2)
	var first: Node3D = effects._prints[0].node
	var second: Node3D = effects._prints[1].node
	assert_almost_eq(first.global_position.y, 0.0, 0.05, "on the ground")
	assert_lt(first.global_position.x * second.global_position.x, 0.0, "a left foot and a right")
	effects._process(effects.PRINT_LIFE / 2.0)
	assert_lt(effects._prints[0].material.albedo_color.a, 0.2, "fading")
	effects._process(effects.PRINT_LIFE)
	assert_eq(effects.prints_showing(), 0)


func test_no_prints_on_paving_in_the_air_or_on_low_graphics() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var city: Node3D = await _arena("city")
	city.effects.footprint(Vector3(0, 0, 5), 0.0, city.map)
	assert_eq(city.effects.prints_showing(), 0, "paving takes no prints")
	assert_false(Surfaces.takes_prints(city.map))
	assert_false(Surfaces.takes_prints(null))
	var canyon: Node3D = await _arena("canyon")
	assert_true(Surfaces.takes_prints(canyon.map))
	canyon.effects.footprint(Vector3(0, 6, 5), 0.0, canyon.map)
	assert_eq(canyon.effects.prints_showing(), 0, "nor does thin air")
	Settings.set_value("quality", GraphicsQuality.LOW)
	canyon.effects.footprint(Vector3(0, 0, 5), 0.0, canyon.map)
	assert_eq(canyon.effects.prints_showing(), 0)


func test_only_so_many_prints_are_kept() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena("canyon")
	for step in 80:
		world.effects.footprint(Vector3(0, 0, 5 - step * 0.1), 0.0, world.map)
	assert_lte(world.effects.prints_showing(), 48)


func _burst(world: Node3D, at: Vector3, weapon := "launcher") -> void:
	world.effects.launch_projectile(1, weapon, at, Vector3.FORWARD)
	world.effects.burst_projectile(1, weapon, at)


func test_a_burst_flashes_and_sends_a_ring_out_to_its_edge() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	var radius: float = Weapons.find("launcher").splash_radius
	_burst(world, Vector3(0, 1, 0))
	assert_eq(world.effects.projectiles_in_flight(), 0)
	assert_eq(world.effects.showing("burst_flash"), 1)
	assert_eq(world.effects.showing("shockwave"), 1)
	var flash: Node3D = world.effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "burst_flash")[0]
	var ring: Node3D = world.effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "shockwave")[0]
	assert_lt(ring.scale.x, 1.0, "it starts small")
	assert_gt(flash.get_child(0).light_energy, 0.0, "and lights what is round it")
	await wait_seconds(EFFECTS.BURST_FLASH_LIFE * 0.6)
	assert_gt(flash.scale.x, radius * 0.3)
	assert_lte(flash.scale.x, radius * EFFECTS.BURST_FLASH_SIZE + 0.001, "never the whole burst: it isn't a growing ball")
	await wait_seconds(EFFECTS.BURST_LIFE * 0.75 - EFFECTS.BURST_FLASH_LIFE * 0.6)
	assert_gt(ring.scale.x, radius * 0.7, "nearly at the edge")
	assert_lte(ring.scale.x, radius + 0.001)
	assert_eq(ring.scale.y, 1.0, "and still flat")
	await wait_seconds(EFFECTS.BURST_LIFE)
	assert_eq(world.effects.showing("burst_flash"), 0)
	assert_eq(world.effects.showing("shockwave"), 0)


func test_a_burst_near_the_ground_scorches_it_and_one_in_the_air_does_not() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	var radius: float = Weapons.find("launcher").splash_radius
	_burst(world, Vector3(0, radius * 2.0, 0))
	assert_eq(world.effects.showing("scorch"), 0)
	assert_eq(world.effects.debris_showing(), 0)
	_burst(world, Vector3(0, 0.3, 0))
	assert_eq(world.effects.showing("scorch"), 1)
	var scorch: Node3D = world.effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "scorch")[0]
	assert_almost_eq(scorch.global_position.y, 0.02, 0.01, "lying on the ground")
	assert_almost_eq(scorch.scale.x, radius * EFFECTS.SCORCH_SIZE, 0.001)
	# It counts among the marks, fades as they do and goes.
	assert_eq(world.effects.marks_showing(), 1)
	world.effects._process(EFFECTS.MARK_LIFE - 1.0)
	assert_almost_eq((scorch.material_override as StandardMaterial3D).albedo_color.a, EFFECTS.SCORCH_ALPHA / EFFECTS.MARK_FADE, 0.01)
	world.effects._process(1.1)
	assert_eq(world.effects.marks_showing(), 0)


func test_a_burst_throws_lumps_of_the_ground_that_fall_and_are_cleared_away() -> void:
	assert_eq(EFFECTS.debris_count(GraphicsQuality.LOW), 0)
	assert_lt(EFFECTS.debris_count(GraphicsQuality.MEDIUM), EFFECTS.debris_count(GraphicsQuality.HIGH))
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	_burst(world, Vector3(0, 0.3, 0))
	assert_eq(world.effects.debris_showing(), EFFECTS.debris_count(GraphicsQuality.HIGH))
	assert_eq(world.effects.showing("debris"), world.effects.debris_showing())
	for step in 30:
		world.effects._process(0.02)
	var lumps: Array = world.effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "debris")
	assert_true(lumps.any(func(lump: Node3D) -> bool: return lump.global_position.y > 1.0), "up in the air")
	for step in 100:
		world.effects._process(0.02)
	for lump: Node3D in lumps:
		assert_almost_eq(lump.global_position.y, 0.02, 0.01, "come to rest on the ground")
		assert_gt(Vector2(lump.global_position.x, lump.global_position.z).length(), 0.05, "thrown outward")
	world.effects._process(EFFECTS.DEBRIS_LIFE)
	assert_eq(world.effects.debris_showing(), 0)
	# However many bursts, only so many lie about.
	for burst in 12:
		_burst(world, Vector3(burst - 6, 0.3, 3))
	assert_lte(world.effects.debris_showing(), 36)


func test_low_graphics_still_shows_the_burst_but_throws_no_lumps() -> void:
	Settings.set_value("quality", GraphicsQuality.LOW)
	var world: Node3D = await _arena()
	_burst(world, Vector3(0, 0.3, 0))
	assert_eq(world.effects.showing("burst_flash"), 1)
	assert_eq(world.effects.showing("shockwave"), 1)
	assert_eq(world.effects.showing("scorch"), 1)
	assert_eq(world.effects.debris_showing(), 0)


func test_what_only_strikes_does_not_burst() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	var world: Node3D = await _arena()
	_burst(world, Vector3(0, 0.3, 0), "bow")
	for kind: String in ["burst_flash", "shockwave", "scorch", "debris", "puff"]:
		assert_eq(world.effects.showing(kind), 0, kind)
