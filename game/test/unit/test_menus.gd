extends GutTest
## The main menu's match settings and the in-game menu.

const MAIN := preload("res://main.tscn")

var main: Node
var saved := {}


func before_each() -> void:
	for key in ["host_mode", "host_kill_limit", "host_minutes", "host_max_players", "host_map", "host_private", "seen_help", "touch_left_handed", "bindings"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("seen_help", true)
	main = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(1)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])
	Controls.apply(Settings.get_value("bindings"))


func test_each_match_button_steps_through_its_choices_and_comes_round() -> void:
	Settings.set_value("host_kill_limit", MatchSettings.KILL_LIMITS[0])
	var seen := []
	for i in MatchSettings.KILL_LIMITS.size():
		seen.append(Settings.get_value("host_kill_limit"))
		main.kill_limit_button.pressed.emit()
	assert_eq(seen, MatchSettings.KILL_LIMITS)
	assert_eq(Settings.get_value("host_kill_limit"), MatchSettings.KILL_LIMITS[0])


# What the session list shows, line by line.
func _list_lines() -> Array:
	return main.sessions.get_children().filter(func(line: Node) -> bool: return not line.is_queued_for_deletion()).map(func(line: Node) -> String: return line.text)


func test_a_session_server_that_cant_be_reached_is_explained() -> void:
	var said := Net.unreachable("ws://example.test:9080", false)
	assert_string_starts_with(said, "Can't reach the session server at ws://example.test:9080.")
	assert_string_contains(said, "Hosting and joining need it")
	assert_string_contains(said, "practice and the tutorial work without it", "what can still be played")
	assert_false(said.contains(Net.START_SERVER), "a player can't start it")
	assert_string_contains(Net.unreachable("ws://localhost:9080", true), Net.START_SERVER, "a developer can")


func test_the_menu_says_when_the_session_list_cant_be_had_and_stops_once_it_can() -> void:
	var reason := Net.unreachable("http://localhost:9080")
	Net.sessions_unreachable.emit(reason)
	assert_eq(_list_lines(), [main.UNREACHABLE_LIST], "not that there are none to join")
	assert_eq(main.status.text, reason, "and why, under the buttons")
	Net.sessions_listed.emit([])
	assert_eq(_list_lines(), ["None yet. Host one!"])
	assert_eq(main.status.text, "", "said no longer once the list comes")
	# Something else already said under the buttons stays said.
	main.status.text = "Enter a session code."
	Net.sessions_unreachable.emit(reason)
	assert_eq(main.status.text, "Enter a session code.")
	assert_eq(_list_lines(), [main.UNREACHABLE_LIST])


func test_no_answer_or_a_wrong_one_from_the_session_server_twice_running_is_said() -> void:
	watch_signals(Net)
	var good := JSON.stringify({"sessions": []}).to_utf8_buffer()
	var bad := [[HTTPRequest.RESULT_CANT_CONNECT, 0, PackedByteArray()], [HTTPRequest.RESULT_SUCCESS, 502, "Bad gateway".to_utf8_buffer()],
		[HTTPRequest.RESULT_SUCCESS, 200, "<html>".to_utf8_buffer()], [HTTPRequest.RESULT_TIMEOUT, 0, PackedByteArray()]]
	var said := 0
	for answer: Array in bad:
		# Once is asked again, without a word.
		Net._on_sessions_listed(answer[0], answer[1], PackedStringArray(), answer[2])
		assert_signal_emit_count(Net, "sessions_unreachable", said, "once: %s" % [answer])
		# Twice running is said.
		Net._on_sessions_listed(answer[0], answer[1], PackedStringArray(), answer[2])
		said += 1
		assert_signal_emit_count(Net, "sessions_unreachable", said, "twice: %s" % [answer])
	# A list in between starts the count again.
	Net._on_sessions_listed(HTTPRequest.RESULT_TIMEOUT, 0, PackedStringArray(), PackedByteArray())
	Net._on_sessions_listed(HTTPRequest.RESULT_SUCCESS, 200, PackedStringArray(), good)
	Net._on_sessions_listed(HTTPRequest.RESULT_TIMEOUT, 0, PackedStringArray(), PackedByteArray())
	assert_signal_emit_count(Net, "sessions_unreachable", said)
	assert_signal_emitted_with_parameters(Net, "sessions_listed", [[]])
	Net._on_sessions_listed(HTTPRequest.RESULT_SUCCESS, 200, PackedStringArray(), good)


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
	Settings.set_value("host_bots", 3)
	Settings.set_value("host_bot_skill", "easy")
	assert_eq(main.host_settings().to_dict(), {"mode": "teams", "kill_limit": 50, "minutes": 15, "max_players": 4, "map": "box", "bots": 3, "bot_skill": "easy"})
	Settings.set_value("host_bots", 0)
	Settings.set_value("host_bot_skill", "normal")


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


func test_the_controls_are_shown_on_entering_a_first_session_only() -> void:
	Settings.set_value("seen_help", false)
	main._on_session_started()
	assert_true(main.controls_help.visible)
	assert_true(Settings.get_value("seen_help"))
	(main.controls_help.find_child("HelpDone", true, false) as Button).pressed.emit()
	assert_false(main.controls_help.visible)
	main._on_session_ended("")
	main._on_session_started()
	assert_false(main.controls_help.visible)
	main.world.stop()


func test_esc_closes_the_controls_without_opening_the_menu() -> void:
	Settings.set_value("seen_help", false)
	main._on_session_started()
	var esc := InputEventAction.new()
	esc.action = "ui_cancel"
	esc.pressed = true
	main.controls_help._input(esc)
	main._input(esc)
	assert_false(main.controls_help.visible)
	main.world.stop()


func test_the_controls_from_the_menu_go_back_to_the_menu() -> void:
	main._on_session_started()
	main.open_pause_menu()
	main.help_button.pressed.emit()
	assert_true(main.controls_help.visible)
	assert_false(main.is_pause_menu_open())
	main.controls_help.close()
	assert_true(main.is_pause_menu_open())
	main.world.stop()


func test_the_controls_close_when_the_session_ends() -> void:
	main._on_session_started()
	main.show_help()
	main._on_session_ended("The host closed the session.")
	assert_false(main.controls_help.visible)


func test_while_the_controls_show_the_player_stands_still() -> void:
	main._on_session_started()
	await wait_physics_frames(3)
	var own: CharacterBody3D = main.world.player_node(1)
	main.show_help()
	Input.action_press("move_forward")
	var start := own.global_position
	await wait_physics_frames(10)
	Input.action_release("move_forward")
	var moved := (own.global_position - start) * Vector3(1, 0, 1)
	assert_almost_eq(moved.length(), 0.0, 0.01)
	main.world.stop()


func test_the_controls_shown_suit_the_device() -> void:
	var help: Node = main.controls_help
	assert_eq(help.device_for(true, 1), help.TOUCH)
	assert_eq(help.device_for(false, 1), help.GAMEPAD)
	assert_eq(help.device_for(false, 0), help.KEYBOARD)
	for device: String in [help.KEYBOARD, help.GAMEPAD, help.TOUCH]:
		help.open(device)
		assert_eq(help.shown().size(), help.lines(device).size(), device)
		assert_eq(help.shown()[0].get_slice(":", 0), "Move", device)


func test_the_keyboard_controls_show_the_chosen_keys() -> void:
	Settings.set_value("bindings", {"jump": KEY_J})
	Controls.apply(Settings.get_value("bindings"))
	main.controls_help.open(main.controls_help.KEYBOARD)
	assert_has(main.controls_help.shown(), "Jump: J")


func test_the_touch_controls_follow_the_side_chosen() -> void:
	Settings.set_value("touch_left_handed", true)
	main.controls_help.open(main.controls_help.TOUCH)
	assert_has(main.controls_help.shown(), "Move: Drag on the right half of the screen")


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
