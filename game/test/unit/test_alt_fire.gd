extends GutTest
## The other way some weapons fire, on a second trigger: both of the
## shotgun's barrels, a launcher wheel that bounces, a charged blaster shot.

const WORLD := preload("res://world/world.tscn")
const CHEST := (BodyShape.HIP_HEIGHT + 0.25) * BodyShape.SIZE

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3, weapon: String) -> Node3D:
	match_.add_player(id)
	var record := match_.state.player(id)
	if not record.weapons.has(weapon):
		record.weapons.append(weapon)
	record.weapon = weapon
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	match_.state_changed.emit()
	return player


func test_each_other_way_of_firing_belongs_to_a_weapon_that_names_it() -> void:
	var seen := 0
	for id: String in Weapons.ids():
		var weapon := Weapons.find(id)
		assert_eq(weapon.alt_of, "", "%s is a weapon in its own right" % id)
		assert_eq(weapon.credit(), id)
		assert_eq(weapon.rounds, 1, id)
		var other := Weapons.alt_of(weapon)
		if other == null:
			assert_eq(weapon.alt, "", id)
			continue
		seen += 1
		assert_eq(other.id, weapon.alt)
		assert_eq(other.alt_of, id)
		assert_eq(other.credit(), id)
		assert_eq(other.display_name, weapon.display_name, "%s: knock-outs are put down to the weapon" % id)
		assert_false(Weapons.ids().has(other.id), "%s can't be carried or chosen" % other.id)
		assert_between(other.rounds, 1, weapon.magazine, id)
		assert_gte(other.fire_interval, weapon.fire_interval, "%s: no faster than the first way" % id)
		assert_eq(other.alt, "", id)
	assert_eq(seen, 3)
	assert_null(Weapons.alt_of(Weapons.find("pistol")))
	assert_null(Weapons.alt_of(null))


func test_both_barrels_are_two_shots_at_once_for_two_shells() -> void:
	var shotgun := Weapons.find("shotgun")
	var both := Weapons.alt_of(shotgun)
	assert_eq(both.pellets, shotgun.pellets * 2)
	assert_eq(both.rounds, 2)
	assert_eq(both.damage, shotgun.damage)
	assert_gt(both.spread(true), shotgun.spread(true), "wider")
	assert_gt(both.recoil_up, shotgun.recoil_up, "and harder to hold")
	assert_gt(both.damage * both.pellets, float(PlayerRecord.MAX_HEALTH), "enough, if it all lands")


func test_the_charged_shot_spends_rounds_for_one_heavy_hit() -> void:
	var blaster := Weapons.find("blaster")
	var charged := Weapons.alt_of(blaster)
	assert_gt(charged.damage, blaster.damage * 2.0)
	assert_lt(charged.damage, blaster.damage * charged.rounds, "the rounds would do more fired singly")
	assert_lt(charged.damage * charged.headshot_multiplier, float(PlayerRecord.MAX_HEALTH), "never a knock-out by itself")
	assert_lt(charged.damage / charged.fire_interval, blaster.damage / blaster.fire_interval)


func test_the_other_wheel_bounces() -> void:
	var launcher := Weapons.find("launcher")
	var bouncing := Weapons.alt_of(launcher)
	assert_true(bouncing.bounces)
	assert_false(launcher.bounces)
	assert_eq(bouncing.splash_damage, launcher.splash_damage)
	assert_eq(bouncing.rounds, 1)
	assert_lt(bouncing.fuse, launcher.fuse)


func test_firing_the_other_way_uses_its_rounds_and_its_wait() -> void:
	var shotgun := WeaponState.new(Weapons.find("shotgun"))
	var both := Weapons.alt_of(shotgun.stats)
	assert_true(shotgun.fire_other(both))
	assert_eq(shotgun.ammo, shotgun.stats.magazine - 2)
	assert_almost_eq(shotgun.cooldown, both.fire_interval, 0.0001)
	assert_false(shotgun.fire_other(both), "not until it is ready")
	assert_false(shotgun.fire())
	shotgun.tick(both.fire_interval)
	shotgun.ammo = 1
	assert_false(shotgun.fire_other(both), "not with one shell")
	assert_true(shotgun.fire(), "which the first way can still fire")
	assert_false(shotgun.fire_other(null))


func test_spending_the_last_rounds_the_other_way_starts_a_reload() -> void:
	var blaster := WeaponState.new(Weapons.find("blaster"))
	var charged := Weapons.alt_of(blaster.stats)
	blaster.ammo = charged.rounds
	assert_true(blaster.fire_other(charged))
	assert_eq(blaster.ammo, 0)
	assert_true(blaster.is_reloading())


func test_the_host_counts_the_rounds_of_the_other_way() -> void:
	var rules := ShotRules.new()
	var blaster := Weapons.find("blaster")
	var charged := Weapons.alt_of(blaster)
	var allowed := 0
	var time := 10.0
	for shot in 10:
		if rules.allow_shot(blaster, time, charged):
			allowed += 1
		time += charged.fire_interval
	assert_eq(allowed, blaster.magazine / charged.rounds, "a magazine's worth and no more")
	assert_false(rules.allow_shot(blaster, time), "and nothing left for the first way")


func test_the_host_holds_the_other_way_to_its_own_rate() -> void:
	var rules := ShotRules.new()
	var shotgun := Weapons.find("shotgun")
	var both := Weapons.alt_of(shotgun)
	var allowed := 0
	var time := 10.0
	# A changed game asking for both barrels ten times in a second.
	for shot in 10:
		if rules.allow_shot(shotgun, time, both):
			allowed += 1
		time += 0.1
	assert_lte(allowed, 2)


func test_both_barrels_do_more_than_one() -> void:
	var shooter := _add(1, Vector3(0, 0, 0), "shotgun")
	_add(2, Vector3(0, 0, -2.5), "blaster")
	var origin: Vector3 = shooter.head.global_position
	var aim := (Vector3(0, CHEST, -2.5) - origin).normalized()
	match_._shot(1, origin, aim, 3, "shotgun", WeaponStats.AIMED)
	var one: int = 100 - match_.state.player(2).health
	assert_gt(one, 0)
	match_.state.player(2).health = 100
	match_._clock += 5.0
	watch_signals(match_)
	match_._shot(1, origin, aim, 3, "shotgun", WeaponStats.AIMED | WeaponStats.ALT)
	var record := match_.state.player(2)
	var two: int = 100 - record.health if record.alive else 100
	assert_gt(two, one)
	assert_eq(match_.tally.of("shotgun").shots, 2, "both put down to the shotgun")
	assert_eq(match_._rules_for(1)._fired["shotgun"], 3, "one shell, then two")


func test_a_weapon_with_one_way_to_fire_ignores_the_second_trigger() -> void:
	var shooter := _add(1, Vector3(0, 0, 5), "grater")
	_add(2, Vector3(0, 0, -5), "blaster")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, -5) - origin).normalized(), 1, "grater", WeaponStats.AIMED | WeaponStats.ALT)
	assert_eq(match_.state.player(2).health, 100)
	assert_eq(match_.tally.of("grater").shots, 0)


func test_a_bouncing_wheel_is_launched_and_its_burst_is_the_launchers() -> void:
	var shooter := _add(1, Vector3(0, 0, 9), "launcher")
	_add(2, Vector3(0, 0, -4), "blaster")
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 0.5, -3) - origin).normalized(), 1, "launcher", WeaponStats.AIMED | WeaponStats.ALT)
	assert_signal_emitted(match_, "projectile_launched")
	assert_eq(get_signal_parameters(match_, "projectile_launched")[1], "launcher_bounce")
	assert_true(match_._projectiles.values()[0].weapon.bounces)
	for step in 60 * 4:
		match_._physics_process(1.0 / 60.0)
	assert_signal_emitted(match_, "projectile_burst")
	if not match_.state.player(2).alive:
		assert_signal_emitted_with_parameters(match_, "knocked_out", [match_.state.name_of(1), match_.state.name_of(2), "launcher", false])


func test_the_second_trigger_fires_the_other_way_and_tells_the_host() -> void:
	var own := _add(1, Vector3(0, 0, 5), "blaster")
	own.equip("blaster")
	own.weapon.cooldown = 0.0
	var charged := Weapons.alt_of(own.weapon.stats)
	watch_signals(own)
	own._alt_pressed = true
	own._use_weapon()
	assert_signal_emitted(own, "fired")
	assert_eq(own.weapon.ammo, own.weapon.stats.magazine - charged.rounds)
	assert_same(own.shot_stats, charged)
	assert_eq(match_._rules_for(1)._fired["blaster"], charged.rounds, "the host counted the same")
	assert_false(own._alt_pressed)
	# With a weapon that has no other way, nothing.
	own.equip("pistol")
	own.weapon.cooldown = 0.0
	own._alt_pressed = true
	own._use_weapon()
	assert_eq(own.weapon.ammo, own.weapon.stats.magazine)
	assert_false(own._alt_pressed, "and the press isn't kept for later")


func test_there_is_a_key_a_mouse_button_and_a_touch_button_for_it() -> void:
	assert_true(InputMap.has_action("alt_fire"))
	assert_true(Controls.REBINDABLE.has("alt_fire"))
	var kinds := InputMap.action_get_events("alt_fire").map(func(event: InputEvent) -> String: return event.get_class())
	assert_has(kinds, "InputEventKey")
	assert_has(kinds, "InputEventMouseButton")
	assert_true(preload("res://ui/touch_controls.gd").BUTTONS.has("alt"))
