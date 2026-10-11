extends GutTest
## The main menu's match settings and the in-game menu.

const MAIN := preload("res://main.tscn")

var main: Node
var saved := {}


func before_each() -> void:
	for key in ["host_mode", "host_kill_limit", "host_minutes", "host_max_players", "host_map", "host_private"]:
		saved[key] = Settings.get_value(key)
	main = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(1)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func test_each_match_button_steps_through_its_choices_and_comes_round() -> void:
	Settings.set_value("host_kill_limit", MatchSettings.KILL_LIMITS[0])
	var seen := []
	for i in MatchSettings.KILL_LIMITS.size():
		seen.append(Settings.get_value("host_kill_limit"))
		main.kill_limit_button.pressed.emit()
	assert_eq(seen, MatchSettings.KILL_LIMITS)
	assert_eq(Settings.get_value("host_kill_limit"), MatchSettings.KILL_LIMITS[0])


func test_the_buttons_say_what_is_chosen() -> void:
	Settings.set_value("host_mode", "teams")
	Settings.set_value("host_minutes", 3)
	assert_eq(main.mode_button.text, "Cheddar vs Brie")
	assert_eq(main.minutes_button.text, "3 minutes")


func test_hosting_uses_the_chosen_settings() -> void:
	Settings.set_value("host_mode", "teams")
	Settings.set_value("host_kill_limit", 50)
	Settings.set_value("host_minutes", 15)
	Settings.set_value("host_max_players", 4)
	Settings.set_value("host_map", "box")
	assert_eq(main.host_settings().to_dict(), {"mode": "teams", "kill_limit": 50, "minutes": 15, "max_players": 4, "map": "box"})


func test_the_private_button_switches_between_listed_and_invite_only() -> void:
	Settings.set_value("host_private", false)
	assert_eq(main.private_button.text, "Listed for everyone")
	main.private_button.pressed.emit()
	assert_true(Settings.get_value("host_private"))
	assert_eq(main.private_button.text, "Private: invite only")
	main.private_button.pressed.emit()
	assert_false(Settings.get_value("host_private"))


func test_a_session_reads_with_its_players_limit_and_mode() -> void:
	var line: String = main.session_line({"code": "ABCD", "players": 3, "settings": {"mode": "teams", "max_players": 8, "map": "box"}})
	assert_eq(line, "ABCD  ·  3/8  ·  Cheddar vs Brie  ·  Brie Box")


func test_a_session_from_an_older_server_still_reads() -> void:
	assert_eq(main.session_line({"code": "WXYZ", "players": 2}), "WXYZ  ·  2/16  ·  Free-for-all  ·  Cheddar Canyon")


func test_the_in_game_menu_opens_and_closes() -> void:
	main._on_session_started()
	assert_false(main.is_pause_menu_open())
	main.open_pause_menu()
	assert_true(main.is_pause_menu_open())
	assert_eq(Input.mouse_mode, Input.MOUSE_MODE_VISIBLE)
	main.resume_button.pressed.emit()
	assert_false(main.is_pause_menu_open())
	main.world.stop()


func test_esc_opens_and_closes_the_menu_once_the_mouse_is_free() -> void:
	main._on_session_started()
	await wait_process_frames(1)
	var esc := InputEventAction.new()
	esc.action = "ui_cancel"
	esc.pressed = true
	main._input(esc)
	assert_true(main.is_pause_menu_open())
	main._input(esc)
	assert_false(main.is_pause_menu_open())
	main.world.stop()


func test_esc_does_nothing_on_the_main_menu() -> void:
	var esc := InputEventAction.new()
	esc.action = "ui_cancel"
	esc.pressed = true
	main._input(esc)
	assert_false(main.is_pause_menu_open())


func test_the_menu_closes_when_the_session_ends() -> void:
	main._on_session_started()
	main.open_pause_menu()
	main._on_session_ended("The host closed the session.")
	assert_false(main.is_pause_menu_open())


func test_while_the_menu_is_open_the_player_stands_still() -> void:
	main._on_session_started()
	await wait_physics_frames(3)
	var own: CharacterBody3D = main.world.player_node(1)
	main.open_pause_menu()
	Input.action_press("move_forward")
	var start := own.global_position
	await wait_physics_frames(10)
	Input.action_release("move_forward")
	var moved := (own.global_position - start) * Vector3(1, 0, 1)
	assert_almost_eq(moved.length(), 0.0, 0.01)
	main.world.stop()


func test_touches_on_the_menu_are_not_moves() -> void:
	main._on_session_started()
	await wait_process_frames(1)
	var controls: Control = main.touch_controls
	var button_middle: Vector2 = main.menu_button.get_global_rect().get_center()
	assert_true(controls.blocked_at(button_middle))
	assert_false(controls.blocked_at(Vector2(400, 400)))
	main.open_pause_menu()
	await wait_process_frames(1)
	assert_true(controls.blocked_at(Vector2(400, 400)), "the open menu covers the screen")
	main.world.stop()


func test_the_final_standings_list_the_best_players() -> void:
	var state := MatchState.new()
	for id in range(1, 8):
		var record := state.add_player(id)
		record.name = "P%d" % id
		record.kills = id
	var lines: PackedStringArray = main.standings(state).split("\n")
	assert_eq(lines.size(), 5)
	assert_eq(lines[0], "1. P7  ·  7")
	assert_eq(lines[4], "5. P3  ·  3")


func test_the_final_standings_say_who_won_and_the_team_scores() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"mode": "teams"}))
	state.add_player(1)
	state.add_player(2)
	state.team_kills = [3, 7]
	state.end_match()
	var lines: PackedStringArray = main.standings(state).split("\n")
	assert_eq(lines[0], "Brie wins!")
	assert_eq(lines[1], "Cheddar 3  –  7 Brie")


func test_no_players_no_standings() -> void:
	assert_eq(main.standings(MatchState.new()), "")


func test_the_disconnected_notice_shows_how_the_session_stood() -> void:
	main._on_session_started()
	main.world.match_.state.player(1).kills = 2
	main._on_session_ended("The host closed the session.")
	assert_string_contains(main.final_standings.text, "·  2")
	assert_false(main.rejoin_button.visible)


func test_a_session_the_host_ended_is_not_offered_for_rejoining() -> void:
	assert_false(preload("res://main.gd").can_rejoin(Match.HOST_ENDED, "ABCD"))
