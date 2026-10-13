extends GutTest
## The time of day and the weather over an arena.

const WORLD := preload("res://world/world.tscn")

var world: Node3D


func after_each() -> void:
	Settings.set_value("host_sky", Atmosphere.NOON)
	Settings.set_value("quality", "")


func _start(sky: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "sky": sky}))
	world.match_.state_changed.emit()
	await wait_process_frames(1)


func test_every_look_has_a_name_and_everything_it_needs() -> void:
	assert_eq(Atmosphere.ORDER[0], Atmosphere.NOON)
	for id: String in Atmosphere.ORDER:
		assert_true(Atmosphere.NAMES.has(id), id)
		assert_true(Atmosphere.is_kind(id))
		var look: Dictionary = Atmosphere.LOOKS[id]
		for key: String in Atmosphere.LOOKS[Atmosphere.NOON]:
			assert_true(look.has(key), "%s: %s" % [id, key])
		assert_between(look.height, 5.0, 85.0, "%s: the light is above the horizon" % id)
		assert_gt(look.energy, 0.3, "%s: light enough to see by" % id)
		assert_gt(look.ambient_energy, 0.3, "%s: and in the shade" % id)
		assert_lt(look.fog, 0.03, "%s: haze, not a wall" % id)
	assert_true(Atmosphere.NAMES.has(Atmosphere.RANDOM))
	assert_false(Atmosphere.is_kind(Atmosphere.RANDOM), "drawn by lot, it is one of the others")
	assert_false(Atmosphere.is_kind(7))


func test_one_drawn_by_lot_is_one_of_them_and_all_come_up() -> void:
	var rng := RandomNumberGenerator.new()
	var seen := {}
	for seed in 80:
		rng.seed = seed
		var drawn := Atmosphere.settle(Atmosphere.RANDOM, rng)
		assert_has(Atmosphere.ORDER, drawn)
		seen[drawn] = true
	assert_eq(seen.size(), Atmosphere.ORDER.size())
	assert_eq(Atmosphere.settle(Atmosphere.DUSK), Atmosphere.DUSK, "one that was chosen is itself")
	assert_has(Atmosphere.ORDER, Atmosphere.settle("nonsense"))


func test_the_sky_is_part_of_the_hosts_settings() -> void:
	var settings := MatchSettings.from_dict({"sky": Atmosphere.NIGHT})
	assert_eq(settings.sky, Atmosphere.NIGHT)
	assert_eq(MatchSettings.from_dict(bytes_to_var(var_to_bytes(settings.to_dict()))).sky, Atmosphere.NIGHT)
	assert_eq(MatchSettings.from_dict({"sky": "purple"}).sky, Atmosphere.NOON)
	assert_eq(MatchSettings.from_dict({"sky": Atmosphere.RANDOM}).sky, Atmosphere.NOON, "the lot is drawn before it is sent")
	assert_false(MatchSettings.new().to_dict().has("sky"), "nothing sent for the usual")
	assert_false(settings.advertised().has("sky"))


func test_each_look_loads_and_lights_the_arena_its_own_way() -> void:
	await _start(Atmosphere.NOON)
	var sun: DirectionalLight3D = world.get_node("Sun")
	var environment: Environment = world.get_node("WorldEnvironment").environment
	var seen := {}
	for id: String in Atmosphere.ORDER:
		world.atmosphere.show_look(id)
		assert_eq(world.atmosphere.showing, id)
		var look: Dictionary = Atmosphere.LOOKS[id]
		assert_eq(sun.light_color, look.sun)
		assert_almost_eq(sun.light_energy, look.energy, 0.001)
		assert_lt((-sun.global_basis.z).y, -0.05, "%s: it shines down" % id)
		assert_eq(environment.ambient_light_color, look.ambient)
		assert_eq(environment.fog_enabled, look.fog > 0.0, id)
		assert_eq(world.atmosphere.air(), look.air, id)
		assert_almost_eq(float(environment.sky.sky_material.get_shader_parameter("stars")), look.stars, 0.001)
		seen[[look.top, look.sun, look.air]] = true
	assert_eq(seen.size(), Atmosphere.ORDER.size(), "no two alike")
	world.atmosphere.show_look("nonsense")
	assert_eq(world.atmosphere.showing, Atmosphere.NOON)


func test_noon_is_the_arena_as_it_always_was() -> void:
	await _start(Atmosphere.NOON)
	var sun: DirectionalLight3D = world.get_node("Sun")
	# The way the light fell before there was any choice of it.
	assert_almost_eq(-sun.global_basis.z, Vector3(-0.329, -0.8192, -0.4699), Vector3.ONE * 0.002)
	assert_eq(world.atmosphere.air(), Atmosphere.NOTHING)
	assert_false(world.get_node("WorldEnvironment").environment.fog_enabled)


func test_everyone_gets_the_sky_the_host_chose_and_leaving_puts_it_back() -> void:
	await _start(Atmosphere.RAIN)
	assert_eq(world.atmosphere.showing, Atmosphere.RAIN)
	assert_eq(world.atmosphere.air(), Atmosphere.DROPS)
	var drops: CPUParticles3D = world.atmosphere.get_child(0)
	assert_eq(drops.find_children("*", "CollisionObject3D", true, false).size(), 0, "nothing to bump into")
	assert_lt(drops.amount, 1000, "few enough to draw")
	world.stop()
	assert_eq(world.atmosphere.showing, Atmosphere.NOON)
	await wait_process_frames(2)
	assert_eq(world.atmosphere.air(), Atmosphere.NOTHING)


func test_on_low_graphics_nothing_falls_but_the_light_is_the_same() -> void:
	Settings.set_value("quality", GraphicsQuality.LOW)
	await _start(Atmosphere.DUST)
	assert_eq(world.atmosphere.showing, Atmosphere.DUST)
	assert_eq(world.atmosphere.air(), Atmosphere.NOTHING)
	assert_true(world.get_node("WorldEnvironment").environment.fog_enabled)
	Settings.set_value("quality", GraphicsQuality.HIGH)
	world.match_.state_changed.emit()
	assert_eq(world.atmosphere.air(), Atmosphere.MOTES, "turned up, it comes")


func test_the_menu_chooses_the_sky_and_draws_the_lot_once() -> void:
	var main: Node = preload("res://main.tscn").instantiate()
	add_child_autofree(main)
	var button: Button = main.find_child("SkyButton", true, false)
	assert_eq(button.text, "Sky: Clear noon")
	button.pressed.emit()
	assert_eq(button.text, "Sky: Dawn")
	assert_eq(main.host_settings().sky, Atmosphere.DAWN)
	Settings.set_value("host_sky", Atmosphere.RANDOM)
	assert_eq(button.text, "Sky: Surprise me")
	assert_has(Atmosphere.ORDER, main.host_settings().sky)
