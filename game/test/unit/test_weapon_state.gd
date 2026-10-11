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
	var blaster := Weapons.find(Weapons.STARTING)
	assert_not_null(blaster)
	assert_eq(blaster.id, Weapons.STARTING)
	assert_gt(blaster.magazine, 0)
	assert_gt(blaster.fire_interval, 0.0)
	assert_gt(blaster.damage, 0.0)


func test_every_weapon_is_listed_under_its_own_id() -> void:
	for id: String in Weapons.ids():
		assert_eq(Weapons.find(id).id, id)
		assert_false(Weapons.find(id).display_name.is_empty(), id)


func test_an_unknown_weapon_is_not_found() -> void:
	assert_null(Weapons.find("banana"))
	assert_null(Weapons.find(7))


func test_the_weapons_come_in_their_number_key_order() -> void:
	assert_eq(Weapons.ids(), ["blaster", "shotgun", "grater", "launcher"])


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
