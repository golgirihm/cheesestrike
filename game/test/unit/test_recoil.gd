extends GutTest
## The kick, the spread and the reach of each weapon.


func test_the_first_shot_goes_where_it_is_aimed() -> void:
	for id: String in Weapons.ids():
		assert_eq(Recoil.offset(Weapons.find(id), 0), Vector2.ZERO, id)


func test_each_shot_of_a_run_climbs_above_the_last_then_levels_off() -> void:
	for id: String in ["pistol", "blaster", "grater"]:
		var weapon := Weapons.find(id)
		var last := 0.0
		for shot in range(1, 40):
			var climb := Recoil.offset(weapon, shot).y
			assert_gt(climb, last, "%s shot %d" % [id, shot])
			last = climb
		assert_lte(rad_to_deg(last), weapon.recoil_up * weapon.recoil_cap + 0.001, "%s never climbs past its limit" % id)
		assert_almost_eq(rad_to_deg(Recoil.offset(weapon, 1).y), weapon.recoil_up, weapon.recoil_up * 0.15, "%s: about its kick for the second shot" % id)


func test_the_pattern_is_the_same_every_time() -> void:
	var blaster := Weapons.find("blaster")
	for shot in 12:
		assert_eq(Recoil.offset(blaster, shot), Recoil.offset(blaster, shot))
	var sides := range(1, 12).map(func(shot: int) -> float: return Recoil.offset(blaster, shot).x)
	assert_lt(sides.min(), 0.0, "it wanders left")
	assert_gt(sides.max(), 0.0, "and right")
	assert_lte(rad_to_deg(maxf(absf(sides.min()), sides.max())), blaster.recoil_side + 0.001)


func test_aiming_steadies_the_kick() -> void:
	var blaster := Weapons.find("blaster")
	assert_almost_eq(Recoil.offset(blaster, 5, true).y, Recoil.offset(blaster, 5).y * Recoil.AIM_FACTOR, 0.00001)


func test_a_kick_throws_a_shot_up_and_to_the_right() -> void:
	var up := Recoil.apply(Vector3.FORWARD, Vector2(0, deg_to_rad(10.0)))
	assert_almost_eq(rad_to_deg(asin(up.y)), 10.0, 0.001)
	assert_almost_eq(up.x, 0.0, 0.0001)
	var right := Recoil.apply(Vector3.FORWARD, Vector2(deg_to_rad(10.0), 0))
	assert_gt(right.x, 0.0)
	assert_almost_eq(rad_to_deg(Vector3.FORWARD.angle_to(right)), 10.0, 0.001)
	var turned := Recoil.apply(Vector3.RIGHT, Vector2(deg_to_rad(10.0), 0))
	assert_gt(turned.z, 0.0, "to the right of wherever the player faces")
	assert_eq(Recoil.apply(Vector3.FORWARD, Vector2.ZERO), Vector3.FORWARD)
	assert_almost_eq(Recoil.apply(Vector3.UP, Vector2(0.1, 0.1)).length(), 1.0, 0.0001, "even aimed straight up")


func test_a_run_of_fire_ends_with_a_pause() -> void:
	var blaster := Weapons.find("blaster")
	assert_eq(ShotRules.next_in_run(blaster, -1, 0.0), 0, "the first ever")
	assert_eq(ShotRules.next_in_run(blaster, 0, blaster.fire_interval), 1)
	assert_eq(ShotRules.next_in_run(blaster, 6, blaster.fire_interval), 7)
	assert_eq(ShotRules.next_in_run(blaster, 6, blaster.recoil_reset + 0.01), 0)


func test_every_weapons_run_carries_on_at_its_own_rate_of_fire() -> void:
	for id: String in Weapons.ids():
		var weapon := Weapons.find(id)
		if weapon.automatic:
			assert_gt(weapon.recoil_reset, weapon.fire_interval * 1.5, "%s: holding the trigger is one run" % id)


func test_the_host_counts_a_run_of_fire_for_each_weapon() -> void:
	var rules := ShotRules.new()
	var blaster := Weapons.find("blaster")
	var pistol := Weapons.find("pistol")
	assert_true(rules.allow_shot(blaster, 10.0))
	assert_eq(rules.run(blaster), 0)
	assert_true(rules.allow_shot(blaster, 10.12))
	assert_true(rules.allow_shot(blaster, 10.24))
	assert_eq(rules.run(blaster), 2)
	assert_true(rules.allow_shot(pistol, 10.3))
	assert_eq(rules.run(pistol), 0, "another weapon starts its own")
	assert_true(rules.allow_shot(blaster, 11.0))
	assert_eq(rules.run(blaster), 0, "after a pause")
	rules.allow_shot(blaster, 11.12)
	rules.refill()
	rules.allow_shot(blaster, 11.24)
	assert_eq(rules.run(blaster), 0, "and after coming back into play")


func test_from_the_hip_spread_opens_as_a_run_goes_on() -> void:
	for id: String in ["pistol", "blaster", "grater"]:
		var weapon := Weapons.find(id)
		var first := weapon.spread_for(0, 0)
		assert_almost_eq(first, weapon.hip_spread_degrees * WeaponStats.HIP_START, 0.001, id)
		assert_gt(weapon.spread_for(0, weapon.bloom_shots / 2), first, id)
		assert_almost_eq(weapon.spread_for(0, weapon.bloom_shots), weapon.spread(false), 0.001, id)
		assert_eq(weapon.spread_for(0, 500), weapon.spread(false), "%s: and no wider" % id)


func test_aimed_shots_spread_far_less_all_through_a_run() -> void:
	for id: String in Weapons.ids():
		var weapon := Weapons.find(id)
		for run in [0, 3, 50]:
			assert_lte(weapon.spread_for(WeaponStats.AIMED, run), weapon.spread_for(0, run), "%s at shot %d" % [id, run])
		assert_eq(weapon.spread_for(WeaponStats.AIMED, 0), weapon.spread(true), id)


func test_moving_spreads_shots_and_being_in_the_air_more() -> void:
	var blaster := Weapons.find("blaster")
	for aimed in [0, WeaponStats.AIMED]:
		var still := blaster.spread_for(aimed, 2)
		var moving := blaster.spread_for(aimed | WeaponStats.MOVING, 2)
		var airborne := blaster.spread_for(aimed | WeaponStats.AIRBORNE, 2)
		assert_gt(moving, still)
		assert_gt(airborne, moving)
		assert_eq(blaster.spread_for(aimed | WeaponStats.AIRBORNE | WeaponStats.MOVING, 2), airborne)


func test_damage_holds_to_a_range_then_falls_to_a_floor() -> void:
	for id: String in ["pistol", "blaster", "grater", "shotgun"]:
		var weapon := Weapons.find(id)
		assert_gt(weapon.falloff_end, weapon.falloff_start, id)
		assert_eq(weapon.damage_at(0.0), weapon.damage, id)
		assert_eq(weapon.damage_at(weapon.falloff_start), weapon.damage, id)
		var middle := weapon.damage_at((weapon.falloff_start + weapon.falloff_end) / 2.0)
		assert_between(middle, weapon.damage * weapon.falloff_floor, weapon.damage, id)
		assert_almost_eq(weapon.damage_at(weapon.falloff_end), weapon.damage * weapon.falloff_floor, 0.0001, id)
		assert_almost_eq(weapon.damage_at(9999.0), weapon.damage * weapon.falloff_floor, 0.0001, id)


func test_each_gun_owns_a_distance() -> void:
	var shotgun := Weapons.find("shotgun")
	var grater := Weapons.find("grater")
	var blaster := Weapons.find("blaster")
	assert_lt(shotgun.falloff_end, grater.falloff_end)
	assert_lt(grater.falloff_end, blaster.falloff_end)
	# Per second, with every shot landing on the body.
	var rate := func(weapon: WeaponStats, distance: float) -> float:
		return weapon.damage_at(distance) * weapon.pellets / weapon.fire_interval
	assert_gt(rate.call(grater, 10.0), rate.call(blaster, 10.0), "the grater up close")
	assert_gt(rate.call(blaster, 60.0), rate.call(grater, 60.0), "the blaster far off")


func test_a_weapon_without_a_falloff_does_the_same_everywhere() -> void:
	var plain := WeaponStats.new()
	plain.damage = 30.0
	assert_eq(plain.damage_at(500.0), 30.0)
