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


func test_the_notice_for_a_player_who_knocked_themself_out_says_so() -> void:
	var state := MatchState.new()
	state.add_player(1)
	state.damage(1, 1, 500.0, true)
	assert_eq(HUD.knocked_out_text(state, 1)[0], "You knocked yourself out")


func test_the_crosshair_opens_as_wide_as_shots_spread() -> void:
	var tight := HUD.crosshair_gap(0.3, 75.0, 648.0)
	var hip := HUD.crosshair_gap(9.0, 75.0, 648.0)
	assert_eq(tight, HUD.CROSSHAIR_GAP, "never closed up altogether")
	assert_gt(hip, tight * 4.0)
	# The edge of a 9 degree cone, 4.5 degrees off the middle of a view that
	# reaches 37.5 degrees up.
	assert_almost_eq(hip, tan(deg_to_rad(4.5)) / tan(deg_to_rad(37.5)) * 324.0, 0.01)
	assert_gt(HUD.crosshair_gap(9.0, 45.0, 648.0), hip, "wider on screen when zoomed in")
	assert_lte(HUD.crosshair_gap(170.0, 75.0, 648.0), 648.0 * 0.4, "and never off the screen")


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
	match_.mouse_hit.emit(30, false)
	assert_eq(hud.hits_shown, 2, "a giant mouse shows as hit too")
	assert_eq(hud.hit_kind, HUD.HIT)
	hud.hits_shown = 1
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


func test_the_clock_reads_minutes_and_seconds() -> void:
	assert_eq(HUD.clock_text(480.0), "8:00")
	assert_eq(HUD.clock_text(65.2), "1:06", "rounded up, so 0:00 means time is up")
	assert_eq(HUD.clock_text(9.0), "0:09")
	assert_eq(HUD.clock_text(0.0), "0:00")
	assert_eq(HUD.clock_text(-3.0), "0:00")


func test_a_free_for_all_shows_the_goal_and_the_clock() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"kill_limit": 30, "minutes": 5}))
	state.countdown = 4.2
	assert_eq(HUD.match_text(state), "The match begins in 5", "until it has counted down")
	state.countdown = 0.0
	state.add_player(1)
	state.add_player(2)
	assert_eq(HUD.match_text(state), "First to 30   ·   5:00")
	state.time_left = 42.0
	assert_eq(HUD.match_text(state), "First to 30   ·   0:42")


func test_a_team_match_shows_the_teams_scores() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"mode": "teams", "minutes": 3}))
	state.team_kills = [4, 7]
	assert_eq(HUD.match_text(state), "Cheddar 4  –  7 Brie   ·   3:00")


func test_after_a_match_the_line_says_so() -> void:
	var state := MatchState.new()
	state.end_match()
	assert_eq(HUD.match_text(state), "Match over")


func test_the_end_of_a_match_shows_the_result_and_the_scoreboard() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	main.world.start()
	main.combat_hud.show()
	var match_: Match = main.world.match_
	match_.state.end_match()
	await wait_process_frames(2)
	assert_eq(main.combat_hud.result_shown(), "Nobody wins")
	assert_true(main.combat_hud.scoreboard.visible)
	main.world.stop()


func test_the_reserve_is_shown_beside_the_magazine() -> void:
	var blaster := WeaponState.new(Weapons.find("blaster"))
	assert_eq(HUD.reserve_text(blaster), "+ %d" % blaster.stats.reserve)
	blaster.reserve = 0
	assert_eq(HUD.reserve_text(blaster), "+ 0")
	assert_eq(HUD.reserve_text(WeaponState.new(Weapons.find(Weapons.STARTING))), "+ ∞")


func test_grenades_left_show_as_marks() -> void:
	var record := PlayerRecord.new(1)
	assert_eq(HUD.grenade_text(record), "Grenades  ●●")
	record.grenades = 1
	assert_eq(HUD.grenade_text(record), "Grenades  ●○")
	record.grenades = 0
	assert_eq(HUD.grenade_text(record), "Grenades  ○○")
	assert_eq(HUD.grenade_text(null), "")


func test_armour_shows_beside_health_only_while_worn() -> void:
	var record := PlayerRecord.new(1)
	assert_eq(HUD.armour_text(record), "")
	record.armour = 35
	assert_eq(HUD.armour_text(record), "+ 35 rind")
	assert_eq(HUD.armour_text(null), "")


func test_the_lines_across_the_top_never_run_over_one_another_or_the_compass() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	main.world.start()
	main.combat_hud.show()
	await wait_process_frames(2)
	var hud: Control = main.combat_hud
	# Everything that can be up there at once, with its tallest words.
	var lines: Array[Control] = [hud._match_label, hud._objective_label, hud.compass, hud._announce_label, hud._replay_label, hud._tutorial_label]
	hud._match_label.text = "Cheddar 10  –  12 Brie   ·   7:31"
	hud._objective_label.text = "Cheddar wheel home · Brie wheel with Someone"
	hud._announce_label.text = "Team gained the lead"
	hud._replay_label.text = "How you were knocked out"
	hud._tutorial_label.text = "Walk to the flag"
	for line in lines:
		line.visible = true
	await wait_process_frames(2)
	for index in range(1, lines.size()):
		var above := lines[index - 1].get_global_rect()
		var below := lines[index].get_global_rect()
		assert_lte(above.end.y, below.position.y, "%s is clear of %s" % [lines[index].name, lines[index - 1].name])
	main.world.stop()
