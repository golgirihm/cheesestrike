extends GutTest

const HUD := preload("res://ui/combat_hud.gd")
const MAIN := preload("res://main.tscn")


func test_damage_from_straight_ahead_points_up() -> void:
	assert_almost_eq(HUD.damage_direction(Vector3.ZERO, 0.0, Vector3(0, 0, -5)), 0.0, 0.001)


func test_damage_from_the_right_points_right() -> void:
	assert_almost_eq(HUD.damage_direction(Vector3.ZERO, 0.0, Vector3(5, 0, 0)), PI / 2.0, 0.001)


func test_damage_from_the_left_points_left() -> void:
	assert_almost_eq(HUD.damage_direction(Vector3.ZERO, 0.0, Vector3(-5, 0, 0)), -PI / 2.0, 0.001)


func test_damage_from_behind_points_down() -> void:
	assert_almost_eq(absf(HUD.damage_direction(Vector3.ZERO, 0.0, Vector3(0, 0, 5))), PI, 0.001)


func test_the_direction_turns_with_the_player() -> void:
	# Turned a quarter to the left, so what was ahead is now on the right.
	assert_almost_eq(HUD.damage_direction(Vector3.ZERO, PI / 2.0, Vector3(0, 0, -5)), PI / 2.0, 0.001)


func test_height_does_not_change_the_direction() -> void:
	assert_almost_eq(HUD.damage_direction(Vector3(1, 0, 1), 0.0, Vector3(1, 30, -4)), 0.0, 0.001)


func test_the_ammo_counter_shows_rounds_left_and_the_magazine() -> void:
	var weapon := WeaponState.new(Weapons.find("blaster"))
	weapon.fire()
	assert_eq(HUD.ammo_text(weapon), "23 / 24")


func test_the_ammo_counter_says_when_reloading() -> void:
	var weapon := WeaponState.new(Weapons.find("blaster"))
	weapon.fire()
	weapon.start_reload()
	assert_eq(HUD.ammo_text(weapon), "Reloading…")


func test_no_notice_while_in_play() -> void:
	var state := MatchState.new()
	state.add_player(1)
	assert_eq(HUD.knocked_out_text(state, 1), ["", ""])


func test_the_notice_says_who_did_it_and_when_you_are_back() -> void:
	var state := MatchState.new()
	state.add_player(1)
	state.add_player(2)
	state.rename(2, "Sharp Cheddar")
	state.damage(2, 1, 500.0)
	state.tick(0.5)
	assert_eq(HUD.knocked_out_text(state, 1), ["Knocked out by Sharp Cheddar", "Back in 3…"])
	state.tick(1.6)
	assert_eq(HUD.knocked_out_text(state, 1)[1], "Back in 1…")


func test_the_notice_without_an_attacker_who_has_left() -> void:
	var state := MatchState.new()
	state.add_player(1)
	state.damage(9, 1, 500.0)
	assert_eq(HUD.knocked_out_text(state, 1)[0], "Knocked out")


func test_the_hud_counts_hits_and_hurts_and_shows_the_notice() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	var match_: Match = main.world.match_
	match_.start()
	main.world.start()
	await wait_process_frames(1)
	var hud: Control = main.combat_hud
	match_.hit_confirmed.emit(2, 20, false, false)
	match_.hurt.emit(2, Vector3(5, 1, 5), 20)
	assert_eq(hud.hits_shown, 1)
	assert_eq(hud.hurts_shown, 1)
	assert_eq(hud.indicators_showing(), 1)
	await wait_seconds(HUD.DAMAGE_INDICATOR_LIFE + 0.1)
	assert_eq(hud.indicators_showing(), 0, "the red edge fades")

	match_.add_player(2)
	match_.state.damage(2, 1, 500.0)
	await wait_process_frames(2)
	assert_string_starts_with(hud.knocked_out_shown(), "Knocked out by")
	assert_eq(hud.health_text(), "0")
	main.world.stop()
