extends GutTest
## Checks what the browser tests are told about the screen. Those tests can only
## run against a web build; this catches a broken description without one.

const MAIN := preload("res://main.tscn")

var main: Node
var hook: TestHook


func before_each() -> void:
	main = MAIN.instantiate()
	add_child_autofree(main)
	hook = TestHook.new(main)
	autofree(hook)
	# Let the menu lay itself out.
	await wait_process_frames(2)


func test_it_starts_on_the_menu_with_nobody_playing() -> void:
	var seen := hook.snapshot()
	assert_eq(seen.screen, "menu")
	assert_eq(seen.players, [])
	assert_eq(seen.hud, "")
	assert_eq(seen.notice, "")


func test_it_says_where_the_visible_controls_are() -> void:
	var controls: Dictionary = hook.snapshot().controls
	assert_has(controls, "HostButton")
	assert_has(controls, "TargetEdit")
	assert_has(controls, "JoinButton")
	assert_does_not_have(controls, "LeaveButton", "the waiting notice isn't showing")
	assert_does_not_have(controls, "DisconnectedButton", "the disconnected notice isn't showing")
	var window := Rect2(Vector2.ZERO, get_window().size)
	for control: String in controls:
		var at := Vector2(controls[control][0], controls[control][1])
		assert_true(window.has_point(at), "%s is reported outside the window, at %s" % [control, at])


func test_each_control_is_reported_at_its_own_place() -> void:
	var controls: Dictionary = hook.snapshot().controls
	assert_ne(controls.HostButton, controls.JoinButton)
	assert_ne(controls.TargetEdit, controls.JoinButton)


func test_it_names_the_notice_that_is_showing() -> void:
	main.waiting_notice.show()
	assert_eq(hook.snapshot().screen, "waiting")
	assert_has(hook.snapshot().controls, "LeaveButton")
	main.waiting_notice.hide()

	main._on_session_ended("The host closed the session.")
	var seen := hook.snapshot()
	assert_eq(seen.screen, "disconnected")
	assert_eq(seen.notice, "The host closed the session.")
	assert_has(seen.controls, "DisconnectedButton")


func test_a_player_is_reported_with_its_position() -> void:
	# Offline, this process is peer 1, so a player named "1" is its own.
	var player: CharacterBody3D = main.world._create_player([1, 0])
	main.world.players.add_child(player)
	var players: Array = hook.snapshot().players
	assert_eq(players.size(), 1)
	assert_eq(players[0].id, 1)
	assert_true(players[0].mine)
	var at := player.global_position
	assert_eq(players[0].position, [at.x, at.y, at.z])


func test_the_description_survives_being_sent_as_json() -> void:
	var seen := hook.snapshot()
	assert_eq(JSON.parse_string(JSON.stringify(seen)).screen, seen.screen)
