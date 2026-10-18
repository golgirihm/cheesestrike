extends GutTest
## How each arena is lit, beyond its sun.

const WORLD := preload("res://world/world.tscn")
const LIGHTING := preload("res://world/lighting.gd")

var world: Node3D


func after_each() -> void:
	Settings.set_value("quality", "")


func _start(map: String, sky := Atmosphere.NOON) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": map, "sky": sky}))
	world.match_.state_changed.emit()
	await wait_process_frames(1)


func test_every_arena_has_its_lighting() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	for id: String in Maps.ORDER + [Maps.RANGE]:
		await _start(id)
		var lighting: Node3D = world.lighting
		assert_not_null(lighting, id)
		assert_eq(lighting.get_parent(), world.map, id)
		var counts: Dictionary = lighting.counts()
		assert_eq(counts.fill, 1, "%s: a light for the shade" % id)
		assert_eq(counts.lamps, LIGHTING.LAMPS.get(id, []).size(), id)
		assert_eq(lighting.find_children("*", "CollisionObject3D", true, false).size(), 0, "%s: nothing solid" % id)
		# The sun is still the only light that casts shadows.
		var sun: DirectionalLight3D = world.get_node("Sun")
		for light: Light3D in lighting.find_children("*", "Light3D", true, false):
			assert_false(light.shadow_enabled, "%s: %s" % [id, light.name])
		assert_true(sun.shadow_enabled, id)
		world.free()


func test_the_shade_is_filled_with_cool_light_from_the_other_side_of_the_sky() -> void:
	await _start("box")
	var sun: DirectionalLight3D = world.get_node("Sun")
	var fill: DirectionalLight3D = world.lighting.get_node("Fill")
	var sun_way := -sun.global_basis.z
	var fill_way := -fill.global_basis.z
	assert_lt(Vector2(sun_way.x, sun_way.z).dot(Vector2(fill_way.x, fill_way.z)), 0.0, "from the other side")
	assert_lt(fill_way.y, 0.0, "and from above")
	assert_gt(fill.light_color.b, fill.light_color.r, "cool")
	assert_lt(fill.light_color.s, 0.3, "a hint of it: shade is dim, not blue")
	assert_gt(sun.light_color.r, sun.light_color.b, "against a warm sun")
	assert_lt(fill.light_energy, sun.light_energy * 0.5, "never rivalling it")
	# It follows the sun round as the sky changes.
	world.match_.state.settings.sky = Atmosphere.DUSK
	world._show_sky()
	var dusk: Vector3 = -(world.get_node("Sun") as Node3D).global_basis.z
	var after := -fill.global_basis.z
	assert_lt(Vector2(dusk.x, dusk.z).dot(Vector2(after.x, after.z)), 0.0)


func test_what_stands_on_the_ground_darkens_it_and_what_hangs_over_it_does_not() -> void:
	await _start("dairy")
	var geometry: Node = world.map.get_node("Geometry")
	var standing := 0
	for body: Node3D in geometry.get_children():
		var shape := body.get_node_or_null("CollisionShape3D") as CollisionShape3D
		if body.name == "Floor" or String(body.name).begins_with("Wall") or shape == null:
			continue
		var size: Vector3 = (shape.shape as BoxShape3D).size
		if body.position.y - size.y / 2.0 <= 0.3:
			standing += 1
	assert_gt(standing, 5)
	assert_eq(world.lighting.counts().shades, standing, "one for each: not for the roof, the gantry or a block on a block")
	# All drawn at once, each flat on the ground and a little bigger than
	# what stands there.
	var shades: MultiMeshInstance3D = world.lighting.get_node("Shades")
	assert_eq(shades.multimesh.instance_count, standing)
	assert_true(shades.multimesh.mesh is PlaneMesh)
	assert_false(shades.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON)
	for place: Transform3D in shades.get_meta("places"):
		assert_between(place.origin.y, 0.0, 0.1, "flat on the ground")
		assert_gt(place.basis.get_scale().x, 3.2, "reaching past its edge")


func test_lamps_come_on_when_the_sky_is_dark() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	await _start("city", Atmosphere.NOON)
	assert_gt(world.lighting.counts().lamps, 3)
	assert_eq(world.lighting.counts().lit, 0, "not at noon")
	assert_eq(world.lighting.counts().glowing, 0)
	for sky: String in Atmosphere.ORDER:
		world.match_.state.settings.sky = sky
		world._show_sky()
		var dark: bool = LIGHTING.DARK_SKIES.has(sky)
		assert_eq(world.lighting.counts().lit, world.lighting.counts().lamps if dark else 0, sky)
	assert_has(LIGHTING.DARK_SKIES, Atmosphere.NIGHT)
	assert_does_not_have(LIGHTING.DARK_SKIES, Atmosphere.NOON)


func test_the_caves_lamps_are_always_lit() -> void:
	Settings.set_value("quality", GraphicsQuality.HIGH)
	await _start("cave", Atmosphere.NOON)
	assert_eq(world.lighting.counts().lit, world.lighting.counts().lamps)
	assert_gt(world.lighting.counts().lit, 0)


func test_on_low_graphics_lamps_glow_but_light_nothing() -> void:
	Settings.set_value("quality", GraphicsQuality.LOW)
	await _start("city", Atmosphere.NIGHT)
	assert_eq(world.lighting.counts().lit, 0)
	assert_eq(world.lighting.counts().glowing, world.lighting.counts().lamps, "still seen to be on")


func test_no_lamp_stands_where_a_player_starts_or_inside_anything() -> void:
	await _start("box")
	for id: String in LIGHTING.LAMPS:
		world.load_map(id)
		await wait_physics_frames(2)
		var space := world.get_world_3d().direct_space_state
		for at: Vector3 in LIGHTING.LAMPS[id]:
			for spot: Vector3 in world.spawn_positions():
				assert_gt(Vector2(spot.x - at.x, spot.z - at.z).length(), BodyShape.RADIUS + 0.3, "%s: a lamp at %s is on a starting spot" % [id, at])
			var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(at + Vector3.UP * 30.0, at + Vector3.UP * 0.5, 1))
			assert_true(hit.is_empty(), "%s: the lamp at %s is inside or under something" % [id, at])
			assert_lt(LIGHTING.LAMPS[id].size(), 8, "%s: few enough for every surface to be lit by them all" % id)
