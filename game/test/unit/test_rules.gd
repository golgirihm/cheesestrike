extends GutTest
## The host's rules: what each named set changes, and that the match, the
## host's checks and the arena all play by them.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func after_each() -> void:
	Projectile.gravity_scale = 1.0
	Settings.set_value("host_rules", MatchSettings.STANDARD)


func _state(rules: String, mode := "ffa", ids := [1, 2]) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"rules": rules, "mode": mode}))
	for id: int in ids:
		state.add_player(id)
	return state


func _start(rules: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box", "rules": rules}))
	match_.state_changed.emit()
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- The settings -----------------------------------------------------------

func test_every_set_of_rules_has_a_name_and_changes_only_what_it_may() -> void:
	assert_eq(MatchSettings.RULES[0], MatchSettings.STANDARD)
	var usual := MatchSettings.new()
	for rules: String in MatchSettings.RULES:
		assert_true(MatchSettings.RULE_NAMES.has(rules), rules)
		assert_true(MatchSettings.PRESETS.has(rules), rules)
		for key: String in MatchSettings.PRESETS[rules]:
			assert_has(MatchSettings.RULE_KEYS, key, "%s changes %s" % [rules, key])
		var settings := MatchSettings.from_dict({"rules": rules})
		assert_eq(settings.rules, rules)
		# Whatever it changes is different from the usual, and nothing else is.
		for key: String in MatchSettings.RULE_KEYS:
			if key != "rules":
				assert_eq(settings.get(key) != usual.get(key), MatchSettings.PRESETS[rules].has(key), "%s: %s" % [rules, key])
	assert_eq(MatchSettings.PRESETS[MatchSettings.STANDARD], {})
	for name: String in MatchSettings.WEAPON_SETS:
		for weapon: String in MatchSettings.WEAPON_SETS[name]:
			assert_has(Weapons.ids(), weapon, name)


func test_what_the_named_rules_come_to() -> void:
	assert_eq(MatchSettings.from_dict({"rules": "instant"}).health, 1)
	assert_eq(MatchSettings.from_dict({"rules": "low_gravity"}).gravity, 0.4)
	assert_eq(MatchSettings.from_dict({"rules": "launchers"}).weapons, "launchers")
	assert_true(MatchSettings.from_dict({"rules": "launchers"}).fixed_weapons())
	assert_false(MatchSettings.new().fixed_weapons())
	var frantic := MatchSettings.from_dict({"rules": "frantic"})
	assert_gt(frantic.speed, 1.0)
	assert_lt(frantic.respawn, MatchState.RESPAWN_DELAY)
	assert_true(MatchSettings.from_dict({"rules": "friendly"}).friendly_fire)
	assert_gt(MatchSettings.from_dict({"rules": "tough"}).health, PlayerRecord.MAX_HEALTH)
	assert_eq(MatchSettings.new().respawn, MatchState.RESPAWN_DELAY, "the usual is as it always was")


func test_rules_survive_the_trip_and_the_usual_ones_cost_nothing_to_send() -> void:
	for rules: String in MatchSettings.RULES:
		var settings := MatchSettings.from_dict({"rules": rules, "mode": "teams"})
		var copy := MatchSettings.from_dict(bytes_to_var(var_to_bytes(settings.to_dict())))
		for key: String in MatchSettings.RULE_KEYS:
			assert_eq(copy.get(key), settings.get(key), "%s: %s" % [rules, key])
	var plain := MatchSettings.new().to_dict()
	for key: String in MatchSettings.RULE_KEYS:
		assert_false(plain.has(key), key)
	assert_true(MatchSettings.from_dict({"rules": "tough"}).to_dict().has("health"))


func test_a_host_can_say_more_than_the_named_rules_and_nonsense_is_put_right() -> void:
	var settings := MatchSettings.from_dict({"rules": "low_gravity", "health": 150, "respawn": 6})
	assert_eq(settings.gravity, 0.4, "the rules chosen")
	assert_eq(settings.health, 150, "and what was said besides")
	assert_eq(settings.respawn, 6.0)
	var wild := MatchSettings.from_dict({"rules": "anarchy", "health": -5, "speed": 99, "gravity": 0, "respawn": NAN, "friendly_fire": "yes", "weapons": "spoons"})
	assert_eq(wild.rules, MatchSettings.STANDARD)
	assert_eq(wild.health, 1)
	assert_eq(wild.speed, 2.0)
	assert_eq(wild.gravity, 0.2)
	assert_eq(wild.respawn, 3.0)
	assert_false(wild.friendly_fire)
	assert_eq(wild.weapons, MatchSettings.ALL_WEAPONS)
	assert_eq(MatchSettings.from_dict({"health": 9999}).health, PlayerRecord.MOST_HEALTH)


func test_the_session_list_is_not_told_the_rules_but_a_description_names_them() -> void:
	var settings := MatchSettings.from_dict({"rules": "tough"})
	for key: String in MatchSettings.RULE_KEYS:
		assert_false(settings.advertised().has(key), key)
	assert_string_ends_with(settings.describe(), "· Tough cheese")
	assert_false(MatchSettings.new().describe().contains("Standard"))


# --- The match plays by them -----------------------------------------------

func test_instant_knock_out_is_one_hit_from_anything() -> void:
	var state := _state("instant")
	assert_eq(state.player(2).health, 1)
	assert_true(state.damage(1, 2, 1.0).knocked_out)
	state.respawn(2)
	assert_eq(state.player(2).health, 1, "and they come back with as little")


func test_tough_cheese_has_more_health_and_wedges_fill_it() -> void:
	var state := _state("tough")
	assert_eq(state.player(2).health, 200)
	assert_false(state.damage(1, 2, 150.0).knocked_out)
	assert_eq(state.player(2).health, 50)
	state.set_pickup_count(2)
	assert_eq(state.collect(0, Pickups.HEALTH, 2), "taken")
	assert_eq(state.player(2).health, 50 + Pickups.HEALTH_AMOUNT)
	state.player(2).health = 180
	assert_eq(state.collect(1, Pickups.HEALTH, 2), "taken", "still short of full")
	assert_eq(state.player(2).health, 200)
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(copy.player(2).health, 200, "more than the usual most survives the trip")


func test_under_the_usual_rules_a_wedge_fills_to_the_usual_health() -> void:
	var state := _state(MatchSettings.STANDARD)
	state.set_pickup_count(1)
	assert_eq(state.collect(0, Pickups.HEALTH, 2), "", "nothing to fill")
	state.player(2).health = 80
	assert_eq(state.collect(0, Pickups.HEALTH, 2), "taken")
	assert_eq(state.player(2).health, PlayerRecord.MAX_HEALTH)


func test_fast_and_frantic_is_faster_and_back_sooner() -> void:
	var state := _state("frantic")
	assert_eq(state.speed_factor(state.player(1)), state.settings.speed)
	state.damage(1, 2, 500.0)
	assert_eq(state.player(2).respawn_in, state.settings.respawn)
	assert_eq(state.tick(state.settings.respawn), [2] as Array[int])
	assert_eq(_state(MatchSettings.STANDARD).speed_factor(PlayerRecord.new(1)), 1.0)


func test_launchers_only_gives_everyone_a_launcher_that_never_runs_out() -> void:
	var state := _state("launchers")
	var record := state.player(1)
	record.main = "rifle"
	state.rearm(record)
	assert_eq(record.weapons, ["launcher"], "whatever their loadout")
	assert_eq(record.weapon, "launcher")
	assert_true(record.endless)
	var stats := record.stats("launcher")
	assert_true(stats.unlimited())
	assert_eq(stats.magazine, Weapons.find("launcher").magazine, "nothing else about it changes")
	assert_eq(stats.splash_damage, Weapons.find("launcher").splash_damage)
	assert_false(Weapons.find("launcher").unlimited(), "and the launcher itself is as it was")
	assert_same(record.stats("launcher"), stats, "the same one each time")
	state.set_pickup_count(1)
	assert_eq(state.collect(0, "rifle", 1), "", "nothing else to find")
	assert_eq(state.drop("launcher", Vector3.ZERO), 0, "or to leave behind")
	state.damage(2, 1, 500.0)
	assert_eq(record.weapons, ["launcher"], "after a knock-out too")
	var copy := MatchState.from_dict(state.to_dict()).player(1)
	assert_true(copy.endless)
	assert_true(copy.stats("launcher").unlimited())


func test_pistols_only_gives_the_pistol_which_never_ran_out_anyway() -> void:
	var state := _state("pistols")
	assert_eq(state.player(1).weapons, ["pistol"])
	assert_same(state.player(1).stats("pistol"), Weapons.find("pistol"))
	assert_same(Weapons.endless(Weapons.find("pistol")), Weapons.find("pistol"))
	assert_null(Weapons.endless(null))


func test_the_usual_rules_leave_weapons_to_run_out() -> void:
	var state := _state(MatchSettings.STANDARD)
	assert_false(state.player(1).endless)
	state.player(1).main = "blaster"
	state.rearm(state.player(1))
	assert_false(state.player(1).stats("blaster").unlimited())


func test_teammates_can_hurt_each_other_only_under_friendly_fire_and_it_does_them_no_good() -> void:
	var safe := _state(MatchSettings.STANDARD, "teams", [1, 2, 3, 4])
	var mate := 2 if safe.player(2).team == safe.player(1).team else 3
	assert_eq(safe.damage(1, mate, 50.0).dealt, 0)
	var state := _state("friendly", "teams", [1, 2, 3, 4])
	mate = 2 if state.player(2).team == state.player(1).team else 3
	assert_eq(state.damage(1, mate, 50.0).dealt, 50)
	assert_eq(state.player(1).overload, 0.0, "no meter for it")
	var result := state.damage(1, mate, 500.0)
	assert_true(result.knocked_out)
	assert_eq(state.player(1).kills, 0, "and no score")
	assert_eq(state.team_kills, [0, 0])
	assert_eq(state.player(mate).deaths, 1)
	assert_eq(state.player(mate).last_attacker, 1, "but they are told who did it")


# --- The host and the arena play by them -----------------------------------

func test_the_host_allows_the_speed_and_the_time_in_the_air_its_rules_give() -> void:
	await _start("frantic")
	_add(5, Vector3(0, 0, 9))
	assert_eq(match_._check_for(5).speed_scale, match_.state.settings.speed)
	var fast := MovementCheck.new()
	fast.speed_scale = 1.5
	var usual := MovementCheck.new()
	var clear := func(_from: Vector3, _to: Vector3) -> bool: return false
	var step := MovementCheck.MAX_SPEED * 1.3 * 1.0 + MovementCheck.MAX_SPEED * MovementCheck.SLACK_TIME
	for check: MovementCheck in [fast, usual]:
		check.check(10.0, Vector3.ZERO, 0, clear, true)
	assert_true(fast.check(11.0, Vector3(step, 0, 0), 0, clear, true))
	assert_false(usual.check(11.0, Vector3(step, 0, 0), 0, clear, true), "too fast for the usual rules")
	var floaty := MovementCheck.new()
	floaty.air_scale = 2.5
	assert_gt(floaty.air_scale * MovementCheck.MAX_AIRTIME, MovementCheck.MAX_AIRTIME)


func test_low_gravity_pulls_less_on_players_and_on_what_they_launch() -> void:
	await _start("low_gravity")
	assert_eq(Projectile.gravity_scale, 0.4)
	var usual: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	var own := _add(1, Vector3(0, 10, 9))
	await wait_physics_frames(2)
	assert_almost_eq(own.get_gravity().length(), usual * 0.4, 0.01)
	var wheel := Projectile.new(1, 1, Weapons.find("launcher"), Vector3.ZERO, Vector3.FORWARD)
	assert_almost_eq(wheel.gravity(), usual * Weapons.find("launcher").projectile_gravity * 0.4, 0.001)
	assert_eq(match_._check_for(5).air_scale, 2.5)
	# Leaving puts it back, for the next session.
	world.stop()
	assert_eq(Projectile.gravity_scale, 1.0)
	assert_almost_eq(float(PhysicsServer3D.area_get_param(world.get_world_3d().space, PhysicsServer3D.AREA_PARAM_GRAVITY)), usual, 0.01)


func test_the_health_bar_is_as_long_as_the_rules_health() -> void:
	await _start("tough")
	_add(1, Vector3(0, 0, 9))
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	hud._process(0.1)
	assert_eq(hud._health_bar.max_value, 200.0)
	assert_eq(hud.health_text(), "200")


func test_the_match_begins_by_saying_what_rules_it_is_played_by() -> void:
	await _start("instant")
	_add(1, Vector3(0, 0, 9))
	watch_signals(match_)
	_add(2, Vector3(0, 0, -9))
	match_._process(0.01)
	assert_signal_emitted_with_parameters(match_, "announced", ["The match begins  ·  Instant knock-out", MatchState.NO_TEAM])


func test_the_owner_carries_what_the_rules_give() -> void:
	await _start("launchers")
	var own := _add(1, Vector3(0, 0, 9))
	match_.state_changed.emit()
	assert_eq(own.arsenal.keys(), ["launcher"])
	assert_eq(own.weapon.stats.id, "launcher")
	assert_true(own.weapon.stats.unlimited())
	own.weapon.ammo = 0
	assert_false(own.weapon.is_dry(), "always another wheel to load")


func test_the_menu_chooses_the_rules() -> void:
	var main: Node = preload("res://main.tscn").instantiate()
	add_child_autofree(main)
	var button: Button = main.find_child("RulesButton", true, false)
	assert_eq(button.text, "Rules: Standard")
	button.pressed.emit()
	assert_eq(button.text, "Rules: %s" % MatchSettings.RULE_NAMES[MatchSettings.RULES[1]])
	assert_eq(main.host_settings().rules, MatchSettings.RULES[1])
	assert_eq(main.host_settings().health, 1)
	for press in MatchSettings.RULES.size() - 1:
		button.pressed.emit()
	assert_eq(main.host_settings().rules, MatchSettings.STANDARD, "and round again")
