extends GutTest
## The kinds of Overload a player can choose between, and the build-up each
## begins with.

const WORLD := preload("res://world/world.tscn")
const MENU := preload("res://ui/settings_menu.gd")

var world: Node3D
var match_: Match
var state: MatchState


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	state = match_.state
	await wait_physics_frames(2)


func after_each() -> void:
	Settings.set_value("loadout_overload", Overload.IDS[0])


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


func _set_off(id: int, kind: int) -> void:
	state.choose_overload(id, kind)
	state.player(id).overload = Overload.METER_FULL
	assert_true(state.set_off_overload(id))


func test_every_kind_has_a_name_a_length_and_a_place_in_the_settings() -> void:
	assert_eq(Overload.IDS.size(), Overload.KINDS.size())
	assert_eq(Settings.CHOICES["loadout_overload"], Overload.IDS)
	for kind: int in Overload.KINDS:
		assert_true(Overload.NAMES.has(kind))
		assert_gt(Overload.DURATIONS[kind], 0.0)
		assert_eq(Overload.from_id(Overload.IDS[kind]), kind)
		assert_false(Overload.text(kind).is_empty())
	assert_eq(Overload.DURATIONS[Overload.MELT], Overload.DURATION, "the first is as it always was")
	assert_eq(Overload.from_id("banana"), Overload.MELT)
	assert_false(Overload.is_kind(9))
	assert_false(Overload.is_kind("rain"))


func test_nothing_happens_until_it_has_built_up() -> void:
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	_set_off(1, Overload.MELT)
	var record := state.player(1)
	assert_true(Overload.is_building(record))
	assert_false(Overload.is_active(record))
	assert_eq(Overload.speed_factor(record), 1.0, "no faster yet")
	state.damage(1, 2, 10.0)
	assert_eq(state.player(2).melted_for, 0.0, "and nobody melts")
	assert_eq(record.overload, 0.0, "nor does the meter fill")
	state.tick(Overload.BUILD_UP - 0.1)
	assert_true(Overload.is_building(record))
	assert_eq(record.overload_left, Overload.DURATIONS[Overload.MELT], "it hasn't started to run down")
	state.tick(0.2)
	assert_true(Overload.is_active(record, Overload.MELT))
	assert_eq(state.overloads_begun, [1] as Array[int])
	assert_almost_eq(record.overload_left, Overload.DURATIONS[Overload.MELT] - 0.1, 0.001, "less what ran over")
	assert_eq(Overload.speed_factor(record), Overload.SPEED_BOOST)


func test_others_see_and_hear_it_from_the_moment_it_is_set_off() -> void:
	_add(1, Vector3(0, 0, 9))
	var other := _add(2, Vector3(0, 0, -9))
	state.choose_overload(2, Overload.RAIN)
	state.player(2).overload = Overload.METER_FULL
	var heard: int = Sounds.counts.get("overload", 0)
	watch_signals(match_)
	match_._overload(2)
	assert_signal_emitted_with_parameters(match_, "overloaded", [2])
	assert_gt(Sounds.counts.get("overload", 0), heard, "heard")
	match_.state_changed.emit()
	assert_true(other.body._glowing, "and seen, while it builds")
	assert_true(Overload.is_building(state.player(2)))
	assert_lte(SoundBank.seconds(SoundBank.get_sound("overload")), Overload.BUILD_UP, "the sound is over as it begins")


func test_a_shield_halves_what_gets_through_and_does_nothing_else() -> void:
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	_set_off(1, Overload.SHIELD)
	assert_eq(state.damage(2, 1, 20.0).dealt, 20, "not while it builds")
	state.tick(Overload.BUILD_UP)
	var record := state.player(1)
	assert_eq(state.damage(2, 1, 20.0).dealt, 10)
	assert_eq(Overload.speed_factor(record), 1.0, "no faster")
	state.damage(1, 2, 10.0)
	assert_eq(state.player(2).melted_for, 0.0, "and nobody melts")
	assert_eq(state.damage(1, 2, 10.0).dealt, 10, "nor hits harder")
	state.tick(Overload.DURATIONS[Overload.SHIELD])
	assert_eq(state.damage(2, 1, 20.0).dealt, 20, "and it ends")


func test_a_kind_is_chosen_beforehand_not_in_the_middle_of_one() -> void:
	_add(1, Vector3(0, 0, 9))
	assert_true(state.choose_overload(1, Overload.SHIELD))
	assert_false(state.choose_overload(1, Overload.SHIELD), "no change")
	assert_false(state.choose_overload(1, 9))
	assert_false(state.choose_overload(1, "rain"))
	assert_false(state.choose_overload(7, Overload.RAIN), "nobody there")
	state.player(1).overload = Overload.METER_FULL
	state.set_off_overload(1)
	assert_false(state.choose_overload(1, Overload.RAIN), "one is under way")
	assert_eq(state.player(1).overload_kind, Overload.SHIELD)


func test_a_knock_out_or_a_new_match_ends_the_build_up_too() -> void:
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	_set_off(1, Overload.RAIN)
	state.damage(2, 1, 500.0)
	assert_eq(state.player(1).overload_build, 0.0)
	assert_eq(state.player(1).overload_left, 0.0)
	state.tick(Overload.BUILD_UP + 0.1)
	assert_eq(state.overloads_begun.size(), 0, "nothing begins")
	state.respawn(1)
	_set_off(1, Overload.RAIN)
	state.start_match()
	assert_eq(state.player(1).overload_build, 0.0)


func test_the_kind_and_the_build_up_survive_the_trip() -> void:
	_add(1, Vector3(0, 0, 9))
	_set_off(1, Overload.RAIN)
	state.tick(0.4)
	var copy := MatchState.from_dict(state.to_dict()).player(1)
	assert_eq(copy.overload_kind, Overload.RAIN)
	assert_almost_eq(copy.overload_build, Overload.BUILD_UP - 0.4, 0.011)
	assert_true(Overload.is_building(copy))
	var numbers := PackedInt32Array()
	numbers.resize(PlayerRecord._COUNT)
	numbers.fill(0)
	numbers[PlayerRecord._FITTED] = (11 << 16) | (2000 << 20)
	var odd := PlayerRecord.from_packed(["X", numbers])
	assert_eq(odd.overload_kind, Overload.MELT)
	assert_lte(odd.overload_build, Overload.BUILD_UP)


func test_a_rain_of_wheels_falls_round_its_player_and_never_on_them() -> void:
	var feet := Vector3(2, 0, -3)
	var seen := {}
	for index in Overload.RAIN_WHEELS:
		var start := Overload.rain_start(feet, index)
		assert_almost_eq(start.y, Overload.RAIN_HEIGHT, 0.001)
		var out := Vector2(start.x - feet.x, start.z - feet.z).length()
		assert_between(out, Overload.RAIN_NEAR, Overload.RAIN_FAR)
		seen[start] = true
	assert_eq(seen.size(), Overload.RAIN_WHEELS, "each somewhere else")
	var wheel := Weapons.find(Overload.RAIN_WEAPON)
	assert_lt(wheel.splash_radius, Overload.RAIN_NEAR, "out of reach of whoever called it down, if they stand still")
	assert_lt(wheel.splash_damage, float(PlayerRecord.MAX_HEALTH), "one alone doesn't knock out")
	assert_eq(wheel.display_name, "Gouda Overload")
	assert_false(Weapons.ids().has(wheel.id), "not a weapon to carry")
	assert_false(Pickups.is_kind(wheel.id))


func test_the_host_lets_the_wheels_fall_one_after_another_once_it_has_built_up() -> void:
	_add(1, Vector3(0, 0, 0))
	_add(2, Vector3(0, 0, 6))
	_set_off(1, Overload.RAIN)
	watch_signals(match_)
	match_._process(Overload.BUILD_UP * 0.5)
	assert_signal_emit_count(match_, "projectile_launched", 0, "not while it builds")
	match_._process(Overload.BUILD_UP * 0.5 + 0.01)
	match_._process(0.01)
	assert_signal_emit_count(match_, "projectile_launched", 1, "the first as it begins")
	assert_eq(get_signal_parameters(match_, "projectile_launched")[1], Overload.RAIN_WEAPON)
	for step in Overload.RAIN_WHEELS:
		match_._process(Overload.RAIN_INTERVAL)
	assert_signal_emit_count(match_, "projectile_launched", Overload.RAIN_WHEELS)
	assert_eq(match_._rains.size(), 0, "and then it is over")
	for step in 60 * 5:
		match_._physics_process(1.0 / 60.0)
	assert_signal_emit_count(match_, "projectile_burst", Overload.RAIN_WHEELS)
	assert_eq(state.player(1).health, PlayerRecord.MAX_HEALTH, "standing still, they are unhurt")


func test_a_wheel_of_the_rain_hurts_whoever_it_lands_by_and_is_put_down_to_the_overload() -> void:
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(3, 0, -3))
	watch_signals(match_)
	match_._launch(1, Weapons.find(Overload.RAIN_WEAPON), Vector3(3.5, Overload.RAIN_HEIGHT, -3), Vector3.DOWN)
	for step in 60 * 3:
		match_._physics_process(1.0 / 60.0)
	assert_lt(state.player(2).health, PlayerRecord.MAX_HEALTH)
	assert_gt(match_.tally.of(Overload.RAIN_WEAPON).damage, 0)


func test_a_player_tells_the_host_which_kind_they_want() -> void:
	Settings.set_value("loadout_overload", "shield")
	match_.add_player(1)
	assert_eq(state.player(1).overload_kind, Overload.SHIELD, "on coming in")
	var own: Node3D = world._create_player([1, Vector3.ZERO])
	world.players.add_child(own)
	Settings.set_value("loadout_overload", "rain")
	assert_eq(state.player(1).overload_kind, Overload.RAIN, "and on changing it")
	match_._choose_overload(5, Overload.SHIELD)
	match_.add_player(5)
	assert_eq(state.player(5).overload_kind, Overload.SHIELD, "chosen before they arrived")


func test_the_settings_say_which_and_step_through_them() -> void:
	assert_eq(MENU.loadout_text("loadout_overload"), "Overload: Melt (faster, and your hits slow others)")
	var menu: Control = MENU.new()
	add_child_autofree(menu)
	menu.refresh()
	var button: Button = menu.find_child("Setting_loadout_overload", true, false)
	assert_not_null(button)
	for press in Overload.IDS.size():
		button.pressed.emit()
	assert_eq(Settings.get_value("loadout_overload"), "melt", "and round again")
