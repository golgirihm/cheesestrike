extends GutTest
## The tutorial: one thing to do at a time, ticked off as it is done.

const MAIN := preload("res://main.tscn")

var tutorial: Tutorial
var now: Dictionary


func before_each() -> void:
	tutorial = Tutorial.new()
	now = {"at": Vector3.ZERO, "yaw": 0.0, "shots": 0, "aiming": false, "reloads": 0, "weapons": 2, "overloaded": false}
	tutorial.update(now)


# Tells the tutorial how things stand, with `changes` made, and returns
# whether that finished a step.
func _then(changes: Dictionary) -> bool:
	now = now.merged(changes, true)
	return tutorial.update(now)


func test_it_takes_a_player_through_everything_in_order() -> void:
	assert_eq(Tutorial.STEPS, ["move", "look", "shoot", "aim", "reload", "pick_up", "overload"])
	var did := [{"at": Vector3(0, 0, -Tutorial.WALK - 0.1)}, {"yaw": Tutorial.TURN + 0.05}, {"shots": 1}, {"aiming": true}, {"reloads": 1}, {"weapons": 3}, {"overloaded": true}]
	for index in Tutorial.STEPS.size():
		assert_eq(tutorial.step_name(), Tutorial.STEPS[index])
		assert_false(tutorial.is_done())
		assert_false(tutorial.update(now), "%s isn't done by doing nothing" % Tutorial.STEPS[index])
		assert_true(_then(did[index]), Tutorial.STEPS[index])
	assert_true(tutorial.is_done())
	assert_eq(tutorial.step_name(), "")
	assert_false(tutorial.update(now), "nothing more to do")


func test_a_step_is_not_done_by_what_another_asks() -> void:
	assert_false(_then({"shots": 5, "aiming": true, "reloads": 2, "weapons": 4, "overloaded": true, "yaw": 2.0}))
	assert_eq(tutorial.step_name(), "move")


func test_what_was_done_before_a_step_began_does_not_count_for_it() -> void:
	# Fire three times while still learning to walk and look.
	_then({"shots": 3})
	_then({"at": Vector3(Tutorial.WALK + 1.0, 0, 0)})
	_then({"yaw": 1.0})
	assert_eq(tutorial.step_name(), "shoot")
	assert_false(tutorial.update(now), "those were before it asked")
	assert_true(_then({"shots": 4}))


func test_a_weapon_taken_in_passing_earlier_will_do_for_the_step_that_asks_for_one() -> void:
	_then({"weapons": 3})
	assert_eq(tutorial.step_name(), "move", "it isn't what was asked yet")
	for did: Dictionary in [{"at": Vector3(Tutorial.WALK + 1.0, 0, 0)}, {"yaw": 1.0}, {"shots": 1}, {"aiming": true}, {"reloads": 1}]:
		assert_true(_then(did))
	assert_eq(tutorial.step_name(), "pick_up")
	assert_true(tutorial.update(now), "ticked off as soon as it is asked")
	assert_eq(tutorial.step_name(), "overload")


func test_a_reload_begun_is_enough() -> void:
	for did: Dictionary in [{"at": Vector3(Tutorial.WALK + 1.0, 0, 0)}, {"yaw": 1.0}, {"shots": 1}, {"aiming": true}]:
		assert_true(_then(did))
	assert_eq(tutorial.step_name(), "reload")
	assert_true(_then({"reloading": true}), "one that the rack's pad cuts short still counts")


func test_walking_is_added_up_however_it_wanders() -> void:
	for step in 5:
		assert_false(_then({"at": Vector3(0.5 if step % 2 == 0 else 0.0, 0, 0)}), "not far yet")
	for step in 6:
		_then({"at": Vector3(0.5 if step % 2 == 1 else 0.0, 0, 0)})
	assert_eq(tutorial.step_name(), "look", "back and forth is still walking")


func test_falling_is_not_walking() -> void:
	assert_false(_then({"at": Vector3(0, -50, 0)}))


func test_turning_is_added_up_either_way_and_across_the_wrap() -> void:
	_then({"at": Vector3(9, 0, 0)})
	assert_false(_then({"yaw": Tutorial.TURN * 0.6}))
	assert_true(_then({"yaw": 0.0}), "there and back")
	tutorial = Tutorial.new()
	now = {"at": Vector3.ZERO, "yaw": PI - 0.01, "shots": 0, "aiming": false, "reloads": 0, "weapons": 2, "overloaded": false}
	tutorial.update(now)
	_then({"at": Vector3(9, 0, 0)})
	assert_false(_then({"yaw": -PI + 0.01}), "a hair's turn, not a whole one")


func test_each_step_says_what_to_do_on_each_device() -> void:
	var keys := {"move": "W A S D", "reload": "R", "overload": "Q"}
	for name: String in Tutorial.STEPS:
		for device: String in ["keyboard", "gamepad", "touch"]:
			assert_true(Tutorial.ASKS[name].has(device), "%s on %s" % [name, device])
	assert_eq(tutorial.text("keyboard", keys), "1 of 7   ·   Walk about with W A S D")
	assert_eq(tutorial.text("gamepad", keys), "1 of 7   ·   Walk about with the left stick")
	assert_eq(tutorial.text("touch", keys), "1 of 7   ·   Drag on the left side of the screen to walk")
	assert_eq(tutorial.text("touch", keys, true), "1 of 7   ·   Drag on the right side of the screen to walk", "with the buttons on the left")
	assert_eq(tutorial.text("spoon", keys), tutorial.text("keyboard", keys), "an unknown device is a keyboard")
	tutorial.step = 4
	assert_eq(tutorial.text("keyboard", keys), "5 of 7   ·   Reload with R")
	tutorial.step = 6
	assert_eq(tutorial.text("keyboard", keys), "7 of 7   ·   Your Overload is full: set it off with Q")
	tutorial.step = 7
	assert_eq(tutorial.text("keyboard", keys), Tutorial.DONE)
	for name: String in Tutorial.STEPS:
		tutorial.step = Tutorial.STEPS.find(name)
		for device: String in ["keyboard", "gamepad", "touch"]:
			assert_false(tutorial.text(device, keys).contains("%"), "%s on %s is filled in" % [name, device])


func test_the_menu_starts_it_on_the_range_and_the_hud_says_what_to_do() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	var seen: bool = Settings.get_value("seen_help")
	Settings.set_value("seen_help", false)
	var button: Button = main.find_child("TutorialButton", true, false)
	assert_not_null(button)
	button.pressed.emit()
	assert_not_null(main.tutorial)
	assert_eq(main.world.map_id, Maps.RANGE)
	assert_false(main.controls_help.visible, "it says how itself")
	assert_false(Settings.get_value("seen_help"), "and the controls are still to be shown another time")
	await wait_physics_frames(2)
	main._process(0.0)
	main._process(0.0)
	assert_string_starts_with(main.combat_hud.tutorial_shown(), "1 of 7")
	# Walking finishes the first step.
	var own: Node3D = main.world.player_node(1)
	own.global_position += Vector3(0, 0, Tutorial.WALK + 0.5)
	main._process(0.0)
	assert_string_starts_with(main.combat_hud.tutorial_shown(), "2 of 7")
	assert_gt(Sounds.counts.get("objective", 0), 0, "with a sound for each")
	# The last step hands the player a full meter to set off.
	main.tutorial.step = Tutorial.STEPS.find("overload")
	main._process(0.0)
	assert_eq(main.world.match_.own_record().overload, Overload.METER_FULL)
	main.world.match_.set_off_overload()
	main._process(0.0)
	assert_true(main.tutorial.is_done())
	assert_eq(main.combat_hud.tutorial_shown(), Tutorial.DONE)
	# Leaving ends it, and the practice range alone has none.
	Net.leave()
	assert_null(main.tutorial)
	assert_eq(main.combat_hud.tutorial_shown(), "")
	main.find_child("PracticeButton", true, false).pressed.emit()
	assert_null(main.tutorial)
	main._process(0.0)
	assert_eq(main.combat_hud.tutorial_shown(), "")
	Net.leave()
	Settings.set_value("seen_help", seen)
	multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()


func test_a_pad_of_the_rack_is_straight_ahead_of_where_a_player_starts() -> void:
	var map: Node3D = Maps.scene(Maps.RANGE).instantiate()
	add_child_autofree(map)
	var start: Vector3 = map.get_node("SpawnPoints").get_child(0).position
	var ahead := map.get_node("Pickups").get_children().filter(func(pad: Node3D) -> bool:
		return absf(pad.position.x - start.x) < 0.5 and pad.position.z < start.z and Weapons.ids().has(String(pad.get_meta("kind"))))
	assert_eq(ahead.size(), 1, "so that walking forward takes a weapon")
