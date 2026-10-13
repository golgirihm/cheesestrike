extends GutTest
## Things that move about an arena for the look of it.

const WORLD := preload("res://world/world.tscn")
const LIFE := preload("res://world/ambient_life.gd")

var world: Node3D


func after_each() -> void:
	Settings.set_value("quality", "")


func _start(map: String, sky := Atmosphere.NOON) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": map, "sky": sky}))
	world.match_.state_changed.emit()
	await wait_process_frames(2)


func test_every_arena_has_things_that_move_and_none_of_them_is_solid() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	for id: String in Maps.ORDER + [Maps.RANGE]:
		await _start(id)
		var life: Node3D = world.ambient_life
		assert_not_null(life, id)
		var counts: Dictionary = life.counts()
		assert_eq(counts.flags, 4, "%s: a flag at each corner" % id)
		assert_gt(counts.birds, 0, id)
		assert_gt(counts.shadows, 0, id)
		assert_eq(life.find_children("*", "CollisionObject3D", true, false).size(), 0, "%s: nothing to bump into or to stop a shot" % id)
		assert_eq(life.find_children("*", "Light3D", true, false).size(), 0, "%s: no lights to pay for" % id)
		assert_lt(life.find_children("*", "", true, false).size(), 120, "%s: not much to draw" % id)
		for drawn: MeshInstance3D in life.find_children("*", "MeshInstance3D", true, false):
			var material := drawn.material_override as StandardMaterial3D
			if material != null:
				assert_false(Scenery.is_yellow(material.albedo_color), "%s: %s is the yellow of cheese" % [id, drawn.get_path()])
		world.free()


func test_only_the_city_has_traffic_lights_and_a_train() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	await _start("city")
	assert_eq(world.ambient_life.counts().lights, 4)
	assert_eq(world.ambient_life.counts().train, 1)
	assert_gt(world.ambient_life.find_children("*", "CPUParticles3D", true, false).size(), 0, "steam")
	world.load_map("box")
	assert_eq(world.ambient_life.counts().lights, 0)
	assert_eq(world.ambient_life.counts().train, 0)
	assert_eq(world.ambient_life.find_children("*", "CPUParticles3D", true, false).size(), 0)


func test_a_new_arena_has_its_own_and_the_old_ones_go_with_it() -> void:
	await _start("city")
	var old: Node3D = world.ambient_life
	world.load_map("canyon")
	await wait_process_frames(2)
	assert_false(is_instance_valid(old) and old.is_inside_tree())
	assert_eq(world.ambient_life.get_parent(), world.map)


func test_on_low_graphics_only_what_costs_next_to_nothing_is_made() -> void:
	Settings.set_value("quality", GraphicsQuality.LOW)
	await _start("city")
	var counts: Dictionary = world.ambient_life.counts()
	assert_eq(counts.birds, 0)
	assert_eq(counts.shadows, 0)
	assert_eq(world.ambient_life.find_children("*", "CPUParticles3D", true, false).size(), 0)
	assert_eq(counts.flags, 4)
	assert_eq(counts.lights, 4)


func test_a_traffic_light_goes_red_green_amber_and_round_again() -> void:
	var seen := []
	var time := 0.0
	while time < 30.0:
		var lamp: int = LIFE.lamp_at(time, 0)
		if seen.is_empty() or seen[-1] != lamp:
			seen.append(lamp)
		time += 0.1
	assert_eq(seen.slice(0, 4), [0, 1, 2, 0])
	assert_eq(LIFE.lamp_at(0.0, 0), 0, "one way starts on red")
	assert_eq(LIFE.lamp_at(0.0, 1), 1, "the crossing way on green")
	# The two ways are never both green.
	time = 0.0
	while time < 30.0:
		assert_false(LIFE.lamp_at(time, 0) == 1 and LIFE.lamp_at(time, 1) == 1, "at %.1f" % time)
		time += 0.1


func test_the_lamps_change_and_one_shows_at_a_time() -> void:
	await _start("city")
	var life: Node3D = world.ambient_life
	var shown := {}
	for step in 140:
		life._process(0.1)
		for light: Dictionary in life._lights:
			var lit: Array = light.lamps.filter(func(lamp: MeshInstance3D) -> bool: return lamp.visible)
			assert_eq(lit.size(), 1)
			shown[light.lamps.find(lit[0])] = true
	assert_eq(shown.size(), 3, "each colour came up")


func test_a_train_goes_by_beyond_the_fence_every_so_often() -> void:
	await _start("city")
	var life: Node3D = world.ambient_life
	var edge: Rect2 = Scenery.bounds(world.map.get_node("Geometry"))
	assert_lt(life._train.position.z, edge.position.y - 4.0, "outside the arena")
	life._process(1.0)
	var first: float = life._train.position.x
	life._process(1.0)
	assert_almost_eq(life._train.position.x - first, LIFE.TRAIN_SPEED, 0.01)
	assert_true(life._train.visible)
	life._process(LIFE.TRAIN_EVERY * 0.5)
	assert_false(life._train.visible, "gone, until the next")
	assert_eq(LIFE.train_distance(LIFE.TRAIN_EVERY + 1.0), LIFE.train_distance(1.0))


func test_flags_wave_birds_circle_and_shadows_drift() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	await _start("box")
	var life: Node3D = world.ambient_life
	life._process(0.5)
	var cloth: Node3D = life._flags[0].get_node("Cloth")
	var bird: Node3D = life._birds[0]
	var shadow: Node3D = life._shadows[0]
	var before := [cloth.rotation, bird.position, shadow.position]
	life._process(0.5)
	assert_ne(cloth.rotation, before[0])
	assert_ne(bird.position, before[1])
	assert_gt(bird.position.y, 15.0, "well overhead")
	assert_gt(shadow.position.x, before[2].x)
	assert_almost_eq(shadow.position.y, before[2].y, 0.001, "along the ground")
	# One that leaves the arena comes back on the other side.
	shadow.position.x = 500.0
	life._process(0.1)
	assert_lt(shadow.position.x, 500.0)


func test_cloud_shadows_show_only_under_a_sun() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	await _start("box", Atmosphere.NOON)
	assert_true(world.ambient_life._shadows[0].visible)
	world.free()
	for sky: String in [Atmosphere.NIGHT, Atmosphere.RAIN, Atmosphere.DUST]:
		await _start("box", sky)
		assert_false(world.ambient_life._shadows[0].visible, sky)
		world.free()
	await _start("box", Atmosphere.DUSK)
	assert_true(world.ambient_life._shadows[0].visible)
