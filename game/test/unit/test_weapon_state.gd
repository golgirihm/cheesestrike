extends GutTest

var stats: WeaponStats
var weapon: WeaponState


func before_each() -> void:
	stats = WeaponStats.new()
	stats.id = "test"
	stats.magazine = 3
	stats.fire_interval = 0.1
	stats.reload_time = 1.0
	weapon = WeaponState.new(stats)


func test_it_starts_full_and_ready() -> void:
	assert_eq(weapon.ammo, 3)
	assert_true(weapon.can_fire())


func test_firing_uses_a_round() -> void:
	assert_true(weapon.fire())
	assert_eq(weapon.ammo, 2)


func test_it_cannot_fire_again_before_the_interval() -> void:
	weapon.fire()
	assert_false(weapon.fire())
	weapon.tick(0.05)
	assert_false(weapon.fire())
	weapon.tick(0.05)
	assert_true(weapon.fire())


func test_holding_the_trigger_fires_at_the_weapons_rate() -> void:
	var shots := 0
	stats.magazine = 1000
	weapon = WeaponState.new(stats)
	for frame in 60:
		if weapon.fire():
			shots += 1
		weapon.tick(1.0 / 60.0)
	assert_between(shots, 9, 11, "a second at 0.1 s per shot")


func test_waiting_does_not_save_up_shots() -> void:
	weapon.tick(5.0)
	assert_true(weapon.fire())
	assert_false(weapon.fire())


func test_firing_the_last_round_starts_a_reload() -> void:
	for i in 3:
		weapon.fire()
		weapon.tick(0.1)
	assert_eq(weapon.ammo, 0)
	assert_true(weapon.is_reloading())
	assert_false(weapon.can_fire())


func test_a_reload_fills_the_magazine_after_the_reload_time() -> void:
	weapon.fire()
	assert_true(weapon.start_reload())
	weapon.tick(0.9)
	assert_eq(weapon.ammo, 2)
	assert_false(weapon.can_fire())
	weapon.tick(0.1)
	assert_eq(weapon.ammo, 3)
	assert_false(weapon.is_reloading())
	assert_true(weapon.can_fire())
	assert_eq(weapon.reloads, 1)


func test_a_full_weapon_does_not_reload() -> void:
	assert_false(weapon.start_reload())
	assert_false(weapon.is_reloading())


func test_reloading_twice_does_not_restart_the_reload() -> void:
	weapon.fire()
	weapon.start_reload()
	weapon.tick(0.6)
	assert_false(weapon.start_reload())
	weapon.tick(0.4)
	assert_eq(weapon.ammo, 3)


func test_refilling_ends_a_reload_and_fills_up() -> void:
	weapon.fire()
	weapon.start_reload()
	weapon.refill()
	assert_eq(weapon.reloads, 0, "refilling isn't a reload")
	assert_eq(weapon.ammo, 3)
	assert_false(weapon.is_reloading())
	assert_true(weapon.can_fire())


func test_the_starting_weapon_exists_and_is_sensible() -> void:
	var sidearm := Weapons.find(Weapons.STARTING)
	assert_not_null(sidearm)
	assert_eq(sidearm.id, Weapons.STARTING)
	assert_gt(sidearm.magazine, 0)
	assert_gt(sidearm.fire_interval, 0.0)
	assert_gt(sidearm.damage, 0.0)


func test_every_weapon_is_listed_under_its_own_id() -> void:
	for id: String in Weapons.ids():
		assert_eq(Weapons.find(id).id, id)
		assert_false(Weapons.find(id).display_name.is_empty(), id)


func test_an_unknown_weapon_is_not_found() -> void:
	assert_null(Weapons.find("banana"))
	assert_null(Weapons.find(7))


func test_the_weapons_come_in_their_number_key_order() -> void:
	assert_eq(Weapons.ids(), ["pistol", "blaster", "shotgun", "grater", "launcher"])
	for index in Weapons.ORDER.size():
		assert_true(InputMap.has_action("weapon_%d" % (index + 1)), "a number key for %s" % Weapons.ORDER[index])


## Damage a second from `stats` with every shot landing on the body.
func _damage_rate(stats: WeaponStats) -> float:
	return stats.damage * stats.pellets / stats.fire_interval


func test_players_start_with_a_sidearm_weaker_than_anything_they_can_find() -> void:
	var sidearm := Weapons.find(Weapons.STARTING)
	assert_eq(sidearm.id, "pistol")
	assert_false(sidearm.automatic)
	for id: String in Weapons.ids():
		var other := Weapons.find(id)
		if id != Weapons.STARTING and not other.fires_projectile():
			assert_lt(_damage_rate(sidearm), _damage_rate(other) / 2.0, id)


func test_every_weapon_is_far_less_accurate_from_the_hip() -> void:
	for id: String in Weapons.ids():
		var stats := Weapons.find(id)
		assert_gte(stats.spread(false), 6.0, "%s from the hip" % id)
		assert_gte(stats.spread(false), stats.spread(true) * 2.0, id)
		assert_between(stats.aim_zoom, 0.4, 0.95, id)


func test_the_launcher_holds_two_wheels_that_knock_out_whoever_they_land_by() -> void:
	var launcher := Weapons.find("launcher")
	assert_eq(launcher.magazine, 2)
	# A wheel that lands on a player bursts on their surface, at most the
	# width of their body from their middle.
	var on_them := Splash.damage_at(BodyShape.RADIUS, launcher.splash_damage, launcher.splash_radius)
	assert_gte(on_them, float(PlayerRecord.MAX_HEALTH))
	# One that lands a couple of metres off still does.
	assert_gte(Splash.damage_at(2.5, launcher.splash_damage, launcher.splash_radius), float(PlayerRecord.MAX_HEALTH))


func test_each_weapon_is_set_up_sensibly() -> void:
	for id: String in Weapons.ids():
		var stats := Weapons.find(id)
		assert_gt(stats.magazine, 0, id)
		assert_gt(stats.fire_interval, 0.0, id)
		assert_gt(stats.reload_time, 0.0, id)
		if stats.fires_projectile():
			assert_gt(stats.splash_damage, 0.0, id)
			assert_gt(stats.splash_radius, 0.0, id)
			assert_gt(stats.fuse, 0.0, id)
		else:
			assert_gt(stats.damage * stats.pellets, 0.0, id)
			assert_gt(stats.max_range, 0.0, id)


func test_the_shotgun_needs_more_than_one_close_blast_but_not_many() -> void:
	var shotgun := Weapons.find("shotgun")
	var all_pellets := shotgun.damage * shotgun.pellets
	assert_lt(all_pellets, PlayerRecord.MAX_HEALTH)
	assert_gt(all_pellets * 2.0, PlayerRecord.MAX_HEALTH)


func test_stepping_through_weapons_goes_in_order_and_comes_round() -> void:
	var carried := ["launcher", "blaster", "shotgun"]
	assert_eq(Weapons.cycle(carried, "blaster"), "shotgun")
	assert_eq(Weapons.cycle(carried, "shotgun"), "launcher")
	assert_eq(Weapons.cycle(carried, "launcher"), "blaster")
	assert_eq(Weapons.cycle(carried, "blaster", -1), "launcher")


func test_stepping_with_one_weapon_stays_put() -> void:
	assert_eq(Weapons.cycle(["blaster"], "blaster"), "blaster")


func test_stepping_from_a_weapon_not_carried_gives_the_first() -> void:
	assert_eq(Weapons.cycle(["shotgun", "grater"], "blaster"), "shotgun")


func test_every_weapon_has_a_model_with_a_muzzle() -> void:
	for id: String in Weapons.ids():
		var model := WeaponModels.build(id)
		assert_not_null(model.get_node_or_null("Muzzle"), id)
		assert_lt(model.get_node("Muzzle").position.z, 0.0, "%s points forward" % id)
		model.free()


func test_a_reload_takes_its_rounds_from_the_reserve() -> void:
	var blaster := WeaponState.new(Weapons.find("blaster"))
	var carried := blaster.reserve
	assert_eq(carried, blaster.stats.reserve)
	for shot in 10:
		blaster.fire()
		blaster.tick(blaster.stats.fire_interval)
	assert_true(blaster.start_reload())
	blaster.tick(blaster.stats.reload_time)
	assert_eq(blaster.ammo, blaster.stats.magazine)
	assert_eq(blaster.reserve, carried - 10)


func test_a_short_reserve_gives_a_short_magazine_then_none() -> void:
	var launcher := WeaponState.new(Weapons.find("launcher"))
	launcher.reserve = 1
	launcher.ammo = 0
	assert_false(launcher.is_dry(), "there is one left to load")
	assert_true(launcher.start_reload())
	launcher.tick(launcher.stats.reload_time)
	assert_eq(launcher.ammo, 1)
	assert_eq(launcher.reserve, 0)
	launcher.cooldown = 0.0
	assert_true(launcher.fire())
	assert_false(launcher.is_reloading(), "nothing to reload with")
	assert_true(launcher.is_dry())
	assert_false(launcher.start_reload())
	assert_false(launcher.can_fire())


func test_the_sidearm_never_runs_out() -> void:
	var pistol := WeaponState.new(Weapons.find(Weapons.STARTING))
	assert_true(pistol.stats.unlimited())
	for reload in 30:
		pistol.ammo = 0
		assert_false(pistol.is_dry())
		assert_true(pistol.start_reload())
		pistol.tick(pistol.stats.reload_time)
		assert_eq(pistol.ammo, pistol.stats.magazine)
	assert_false(pistol.add_reserve(10), "and has no use for more")


func test_every_other_weapon_can_run_out_and_carry_more_than_it_starts_with() -> void:
	for id: String in Weapons.ids():
		var stats := Weapons.find(id)
		if id == Weapons.STARTING:
			continue
		assert_false(stats.unlimited(), id)
		assert_gte(stats.reserve, stats.magazine, "%s: at least one reload" % id)
		assert_gt(stats.reserve_max, stats.reserve, id)


func test_picked_up_rounds_fill_the_reserve_to_its_limit() -> void:
	var blaster := WeaponState.new(Weapons.find("blaster"))
	assert_true(blaster.add_reserve(blaster.stats.magazine))
	assert_eq(blaster.reserve, blaster.stats.reserve + blaster.stats.magazine)
	assert_true(blaster.add_reserve(9999))
	assert_eq(blaster.reserve, blaster.stats.reserve_max)
	assert_false(blaster.add_reserve(1), "full")


func test_coming_back_gives_back_the_reserve() -> void:
	var blaster := WeaponState.new(Weapons.find("blaster"))
	blaster.reserve = 0
	blaster.ammo = 3
	blaster.refill()
	assert_eq(blaster.ammo, blaster.stats.magazine)
	assert_eq(blaster.reserve, blaster.stats.reserve)
