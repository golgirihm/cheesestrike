extends GutTest
## Dust, sparks, smoke, crumbs and fire: how many there may be at once on
## each graphics quality, and what throws them out.

const WORLD := preload("res://world/world.tscn")
const LOW := GraphicsQuality.LOW
const MEDIUM := GraphicsQuality.MEDIUM
const HIGH := GraphicsQuality.HIGH


func after_each() -> void:
	Settings.set_value("quality", "")


func _arena(quality: String) -> Node3D:
	Settings.set_value("quality", quality)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	await wait_physics_frames(2)
	return world


func test_lower_graphics_has_room_for_fewer() -> void:
	assert_lt(ParticleBudget.limit(LOW), ParticleBudget.limit(MEDIUM))
	assert_lt(ParticleBudget.limit(MEDIUM), ParticleBudget.limit(HIGH))
	assert_eq(ParticleBudget.limit("nonsense"), ParticleBudget.limit(HIGH))
	for level: String in GraphicsQuality.LEVELS:
		assert_has(ParticleBudget.LIMITS, level)
		assert_has(ParticleBudget.DENSITY, level)


func test_an_effect_gets_what_it_asks_for_while_there_is_room() -> void:
	assert_eq(ParticleBudget.grant(20, 0, HIGH), 20)
	assert_eq(ParticleBudget.grant(20, 0, MEDIUM), 14, "thinned on lower graphics")
	assert_eq(ParticleBudget.grant(1, 0, MEDIUM), 1, "but never to nothing")
	assert_eq(ParticleBudget.grant(0, 0, HIGH), 0)
	assert_eq(ParticleBudget.grant(-3, 0, HIGH, true), 0)


func test_low_graphics_has_only_the_effects_that_matter() -> void:
	assert_eq(ParticleBudget.grant(20, 0, LOW), 0)
	assert_eq(ParticleBudget.grant(20, 0, LOW, true), 8)


func test_nothing_goes_over_the_limit() -> void:
	for level: String in GraphicsQuality.LEVELS:
		var limit := ParticleBudget.limit(level)
		var live := 0
		for effect in 200:
			live += ParticleBudget.grant(30, live, level, effect % 3 == 0)
		assert_lte(live, limit, level)
		assert_gt(live, limit / 2, "and what room there is gets used")
		assert_eq(ParticleBudget.grant(30, limit, level, true), 0)
		assert_eq(ParticleBudget.grant(30, limit + 50, level, true), 0)


func test_room_is_kept_for_the_effects_that_matter() -> void:
	var lesser := int(ParticleBudget.limit(HIGH) * ParticleBudget.LESSER_SHARE)
	assert_eq(ParticleBudget.grant(30, lesser, HIGH), 0, "the lesser ones stop short")
	assert_eq(ParticleBudget.grant(30, lesser, HIGH, true), 30)
	assert_eq(ParticleBudget.grant(30, lesser - 10, HIGH), 10, "the last of them gets what is left")


func test_the_arena_never_shows_more_than_its_limit() -> void:
	for level: String in GraphicsQuality.LEVELS:
		var world: Node3D = await _arena(level)
		for shot in 60:
			world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)], Vector3.RIGHT)
			world.effects.burst_specks(Vector3(0, 1, 0), 4.0)
			world.effects.crumbs(Vector3(2, 1, 0), Color.YELLOW, 36, 9.0)
		assert_lte(world.effects.specks_showing(), ParticleBudget.limit(level), level)
		assert_gt(world.effects.specks_showing(), 0, level)
		# What is drawn is what was counted.
		var drawn := 0
		for child in world.effects.get_children():
			if child is CPUParticles3D:
				drawn += child.amount
		assert_eq(drawn, world.effects.specks_showing(), level)


func test_specks_stop_counting_once_they_have_gone() -> void:
	var world: Node3D = await _arena(HIGH)
	world.effects.burst_specks(Vector3(0, 1, 0), 4.0)
	var all: int = world.effects.specks_showing()
	assert_eq(all, 18 + 26 + 14)
	# The fire and the sparks go first; the smoke hangs about.
	world.effects._process(0.6)
	assert_eq(world.effects.specks_showing(), 14)
	world.effects._process(world.effects.SMOKE_LIFE)
	assert_eq(world.effects.specks_showing(), 0)
	await wait_seconds(world.effects.SMOKE_LIFE + 0.6)
	assert_eq(world.effects.showing("puff"), 0, "and they are cleared away")


func test_a_burst_throws_out_fire_sparks_and_smoke_on_every_quality() -> void:
	for level: String in GraphicsQuality.LEVELS:
		var world: Node3D = await _arena(level)
		world.effects.launch_projectile(1, "launcher", Vector3(0, 1, 0), Vector3.FORWARD)
		world.effects.burst_projectile(1, "launcher", Vector3(0, 1, -3))
		assert_eq(world.effects.showing("puff"), 3, level)
	# Something that only strikes, such as a breadstick, throws out nothing.
	var world: Node3D = await _arena(HIGH)
	for weapon: String in Weapons.ids():
		if Weapons.find(weapon).projectile_speed > 0.0 and Weapons.find(weapon).splash_radius <= 0.0:
			world.effects.burst_projectile(2, weapon, Vector3.ZERO)
	assert_eq(world.effects.showing("puff"), 0)


func test_a_hard_landing_kicks_up_dust_and_a_soft_one_does_not() -> void:
	var world: Node3D = await _arena(HIGH)
	world.effects.landing(Vector3.ZERO, 0.1)
	assert_eq(world.effects.specks_showing(), 0)
	world.effects.landing(Vector3.ZERO, 0.4)
	var some: int = world.effects.specks_showing()
	assert_gt(some, 0)
	world.effects.landing(Vector3(3, 0, 0), 1.0)
	assert_gt(world.effects.specks_showing() - some, some, "more for a harder one")
	var low: Node3D = await _arena(LOW)
	low.effects.landing(Vector3.ZERO, 1.0)
	assert_eq(low.effects.specks_showing(), 0, "a lesser effect")


func test_a_pickup_sparkles_in_its_own_colour() -> void:
	var world: Node3D = await _arena(HIGH)
	for kind: String in [Pickups.HEALTH, Pickups.AMMO, Pickups.ARMOUR, Pickups.POWER]:
		assert_has(world.effects.SPARKLES, kind)
	world.effects.sparkle(Vector3.ZERO, Pickups.HEALTH)
	world.effects.sparkle(Vector3.ZERO, "launcher")
	var puffs: Array = world.effects.get_children().filter(func(child: Node) -> bool: return child is CPUParticles3D)
	assert_eq(puffs.size(), 2)
	assert_eq((puffs[0].material_override as StandardMaterial3D).albedo_color, world.effects.SPARKLES[Pickups.HEALTH])
	assert_eq((puffs[1].material_override as StandardMaterial3D).albedo_color, WeaponModels.WAX)
	assert_lt(puffs[0].gravity.y * -1.0, 0.0, "it rises")


func test_the_match_sets_off_the_sparkle_and_the_flare() -> void:
	var world: Node3D = await _arena(MEDIUM)
	world.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	world.match_.picked_up.emit(1, Pickups.AMMO)
	assert_eq(world.effects.showing("puff"), 1)
	world.match_.overloaded.emit(1)
	assert_eq(world.effects.showing("puff"), 3, "fire and sparks")
	# Someone who has gone sets nothing off.
	world.match_.picked_up.emit(77, Pickups.AMMO)
	world.match_.overloaded.emit(77)
	assert_eq(world.effects.showing("puff"), 3)


func test_being_knocked_out_scatters_crumbs_even_on_low_graphics() -> void:
	var world: Node3D = await _arena(LOW)
	world.effects.crumbs(Vector3(0, 1, 0), Color.YELLOW, 36, 9.0)
	assert_eq(world.effects.specks_showing(), ParticleBudget.grant(36, 0, LOW, true))
	assert_gt(world.effects.specks_showing(), 0)


func test_smoke_thins_away_and_swells_while_sparks_do_not() -> void:
	var world: Node3D = await _arena(HIGH)
	world.effects.burst_specks(Vector3(0, 1, 0), 4.0)
	var puffs: Array = world.effects.get_children().filter(func(child: Node) -> bool: return child is CPUParticles3D)
	var fire: CPUParticles3D = puffs[0]
	var sparks: CPUParticles3D = puffs[1]
	var smoke: CPUParticles3D = puffs[2]
	assert_not_null(smoke.color_ramp)
	assert_eq(smoke.color_ramp.get_color(1).a, 0.0)
	assert_true((smoke.material_override as StandardMaterial3D).vertex_color_use_as_albedo)
	assert_gt(smoke.scale_amount_curve.sample(1.0), smoke.scale_amount_curve.sample(0.0), "smoke swells")
	assert_lt(fire.scale_amount_curve.sample(1.0), fire.scale_amount_curve.sample(0.0), "fire dies down")
	assert_almost_eq(fire.scale_amount_curve.sample(0.0), 1.0, 0.001)
	assert_null(sparks.color_ramp)
	assert_lt(smoke.gravity.y * -1.0, 0.0, "smoke drifts up")
	assert_almost_eq(smoke.lifetime, world.effects.SMOKE_LIFE, 0.001)
