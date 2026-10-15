extends GutTest

const PLAYER_SCENE := preload("res://player/player.tscn")


func before_each() -> void:
	Settings.set_value(Settings.LESS_MOTION, false)


func after_each() -> void:
	Settings.set_value(Settings.LESS_MOTION, false)
	Settings.set_value("screen_shake", 1.0)
	Settings.set_value("camera_motion", 1.0)


func test_it_is_off_unless_asked_for_and_leaves_the_rest_alone() -> void:
	assert_false(Settings.DEFAULTS[Settings.LESS_MOTION])
	Settings.set_value("screen_shake", 0.6)
	assert_almost_eq(Settings.motion("screen_shake"), 0.6, 0.001)
	assert_eq(Settings.flash(), 1.0)


func test_with_it_on_there_is_no_shake_nor_camera_motion() -> void:
	Settings.set_value("screen_shake", 1.0)
	Settings.set_value("camera_motion", 1.0)
	Settings.set_value(Settings.LESS_MOTION, true)
	assert_eq(Settings.motion("screen_shake"), 0.0)
	assert_eq(Settings.motion("camera_motion"), 0.0)
	assert_false(Look.moves(), "screens don't slide in")
	assert_eq(Settings.get_value("screen_shake"), 1.0, "the slider itself is kept for when it is off")


func test_with_it_on_flashes_are_softer() -> void:
	Settings.set_value(Settings.LESS_MOTION, true)
	assert_lt(Settings.flash(), 0.5)


func test_with_it_on_a_burst_nearby_blurs_nothing() -> void:
	Settings.set_value(Settings.LESS_MOTION, true)
	assert_eq(HurtView.blur_from(1.0, 5.0, Settings.motion("screen_shake")), 0.0)


func test_with_it_on_a_shove_shakes_nothing() -> void:
	Settings.set_value(Settings.LESS_MOTION, true)
	var shake := Shake.new()
	shake.add(1.0)
	for frame in 10:
		assert_eq(shake.tick(1.0 / 60.0, Settings.motion("screen_shake")), Vector2.ZERO, "frame %d" % frame)


func test_with_it_on_the_view_is_still_while_running() -> void:
	Settings.set_value(Settings.LESS_MOTION, true)
	var player: CharacterBody3D = PLAYER_SCENE.instantiate()
	add_child_autofree(player)
	await wait_physics_frames(2)
	assert_eq(player.camera.position, Vector3.ZERO)


func test_the_title_screen_warns_of_flashing_until_it_is_put_away() -> void:
	Settings.set_value(FrontEnd.FLASH_SEEN, false)
	var front := FrontEnd.new()
	add_child_autofree(front)
	assert_true(front.flash_warning.visible)
	assert_string_contains(front.flash_warning.find_child("FlashWarningText", true, false).text, "Less motion")
	await wait_process_frames(3)
	var warned := front.flash_warning.get_global_rect()
	assert_true(get_viewport().get_visible_rect().encloses(warned), "on screen")
	for choice: Array in FrontEnd.CHOICES:
		assert_false(warned.intersects(front.button(choice[0]).get_global_rect()), "clear of %s" % choice[0])
	(front.flash_warning.find_child("FlashWarningOk", true, false) as Button).pressed.emit()
	assert_false(front.flash_warning.visible)
	var again := FrontEnd.new()
	add_child_autofree(again)
	assert_false(again.flash_warning.visible, "and stays away")
	Settings.set_value(FrontEnd.FLASH_SEEN, false)
