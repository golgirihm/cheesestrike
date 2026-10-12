extends GutTest
## Players' preferences: their limits, the keys they choose, graphics, sound,
## and the settings screen.

const SETTINGS := preload("res://settings/settings.gd")
const SETTINGS_MENU := preload("res://ui/settings_menu.gd")
const PLAYER := preload("res://player/player.tscn")
const PATH := "user://test_preferences.cfg"

var saved := {}


func before_each() -> void:
	DirAccess.remove_absolute(PATH)
	for key: String in SETTINGS.DEFAULTS:
		saved[key] = Settings.get_value(key)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])
	Controls.apply(Settings.get_value("bindings"))


func after_all() -> void:
	DirAccess.remove_absolute(PATH)


func test_numbers_are_kept_within_their_range() -> void:
	var settings: Node = autofree(SETTINGS.new(PATH))
	settings.set_value("fov", 500.0)
	assert_eq(settings.get_value("fov"), 110.0)
	settings.set_value("master_volume", -1.0)
	assert_eq(settings.get_value("master_volume"), 0.0)


func test_numbers_out_of_range_in_the_file_are_brought_in() -> void:
	var file := ConfigFile.new()
	file.set_value(SETTINGS.SECTION, "mouse_sensitivity", 50.0)
	file.save(PATH)
	assert_eq(autofree(SETTINGS.new(PATH)).get_value("mouse_sensitivity"), 4.0)


func test_a_choice_must_be_one_of_its_options() -> void:
	var settings: Node = autofree(SETTINGS.new(PATH))
	settings.set_value("quality", "ultra")
	assert_push_error("can't be ultra")
	assert_eq(settings.get_value("quality"), "")
	settings.set_value("quality", "low")
	assert_eq(settings.get_value("quality"), "low")


func test_changing_a_copy_of_the_keys_changes_nothing() -> void:
	var settings: Node = autofree(SETTINGS.new(PATH))
	var bindings: Dictionary = settings.get_value("bindings")
	bindings["jump"] = KEY_J
	assert_eq(settings.get_value("bindings"), {})


func test_every_setting_has_a_default_of_its_own_kind() -> void:
	for key: String in SETTINGS.RANGES:
		assert_typeof(SETTINGS.DEFAULTS[key], TYPE_FLOAT, key)
		assert_between(SETTINGS.DEFAULTS[key], SETTINGS.RANGES[key][0], SETTINGS.RANGES[key][1], key)


func test_a_chosen_key_replaces_the_usual_ones() -> void:
	Controls.apply({"jump": KEY_J})
	assert_eq(Controls.keys_for("jump"), [KEY_J])


func test_with_no_choices_the_usual_keys_are_back() -> void:
	Controls.apply({"jump": KEY_J})
	Controls.apply({})
	assert_eq(Controls.keys_for("jump"), [KEY_SPACE])
	assert_eq(Controls.keys_for("move_forward"), [KEY_W, KEY_UP])


func test_a_key_taken_for_one_action_leaves_the_action_it_had() -> void:
	Controls.apply({"jump": KEY_W})
	assert_eq(Controls.keys_for("jump"), [KEY_W])
	assert_eq(Controls.keys_for("move_forward"), [KEY_UP], "W no longer moves forward")


func test_other_controls_of_an_action_stay() -> void:
	Controls.apply({"jump": KEY_J})
	var others := InputMap.action_get_events("jump").filter(func(event: InputEvent) -> bool: return event is InputEventJoypadButton)
	assert_eq(others.size(), 1, "the gamepad button still jumps")


func test_keys_read_by_name() -> void:
	Controls.apply({})
	assert_eq(Controls.keys_text("move_forward"), "W / Up")
	assert_eq(Controls.key_name(KEY_SPACE), "Space")


func test_quality_starts_low_on_a_touch_screen_and_high_otherwise() -> void:
	assert_eq(GraphicsQuality.level("", true), GraphicsQuality.LOW)
	assert_eq(GraphicsQuality.level("", false), GraphicsQuality.HIGH)
	assert_eq(GraphicsQuality.level("medium", true), GraphicsQuality.MEDIUM)


func test_quality_sets_the_resolution_and_shadows() -> void:
	var viewport := SubViewport.new()
	var sun := DirectionalLight3D.new()
	add_child_autofree(viewport)
	add_child_autofree(sun)
	GraphicsQuality.apply(GraphicsQuality.LOW, viewport, sun)
	assert_almost_eq(viewport.scaling_3d_scale, GraphicsQuality.RENDER_SCALE[GraphicsQuality.LOW], 0.0001)
	assert_false(sun.shadow_enabled)
	GraphicsQuality.apply(GraphicsQuality.HIGH, viewport, sun)
	assert_eq(viewport.scaling_3d_scale, 1.0)
	assert_true(sun.shadow_enabled)


func test_volume_becomes_decibels_and_nothing_is_silence() -> void:
	assert_eq(Sounds.volume_db(1.0), 0.0)
	assert_almost_eq(Sounds.volume_db(0.5), -6.02, 0.01)
	assert_eq(Sounds.volume_db(0.0), -80.0)


func test_volume_settings_set_the_buses() -> void:
	Settings.set_value("music_volume", 0.5)
	assert_almost_eq(AudioServer.get_bus_volume_db(AudioServer.get_bus_index("Music")), -6.02, 0.01)
	Settings.set_value("master_volume", 0.0)
	assert_eq(AudioServer.get_bus_volume_db(AudioServer.get_bus_index("Master")), -80.0)


func _menu() -> Control:
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	menu.open()
	return menu


func test_moving_a_slider_changes_the_setting() -> void:
	var menu := _menu()
	(menu.find_child("Setting_fov", true, false) as HSlider).value = 90.0
	assert_eq(Settings.get_value("fov"), 90.0)


func test_the_screen_shows_what_is_set() -> void:
	Settings.set_value("mouse_sensitivity", 2.5)
	var menu := _menu()
	assert_eq((menu.find_child("Setting_mouse_sensitivity", true, false) as HSlider).value, 2.5)
	assert_eq(SETTINGS_MENU.value_text("music_volume", 0.4), "40%")
	assert_eq(SETTINGS_MENU.value_text("mouse_sensitivity", 1.25), "1.25×")
	assert_eq(SETTINGS_MENU.value_text("fov", 75.0), "75°")


func test_the_quality_button_comes_round() -> void:
	assert_eq(SETTINGS_MENU.next_quality("low"), "medium")
	assert_eq(SETTINGS_MENU.next_quality("high"), "low")
	var menu := _menu()
	Settings.set_value("quality", "low")
	(menu.find_child("Setting_quality", true, false) as Button).pressed.emit()
	assert_eq(Settings.get_value("quality"), "medium")


func test_invert_look_toggles() -> void:
	var menu := _menu()
	(menu.find_child("Setting_invert_look", true, false) as CheckButton).button_pressed = true
	assert_true(Settings.get_value("invert_look"))


func _key(code: Key) -> InputEventKey:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = true
	return event


func test_choosing_a_key_takes_the_next_key_pressed() -> void:
	var menu := _menu()
	(menu.find_child("Key_jump", true, false) as Button).pressed.emit()
	assert_true(menu.is_waiting_for_key())
	assert_eq((menu.find_child("Key_jump", true, false) as Button).text, "Press a key…")
	menu._input(_key(KEY_J))
	assert_false(menu.is_waiting_for_key())
	assert_eq(Settings.get_value("bindings"), {"jump": KEY_J})
	assert_eq(Controls.keys_for("jump"), [KEY_J])
	assert_eq((menu.find_child("Key_jump", true, false) as Button).text, "J")


func test_escape_leaves_the_key_as_it_was() -> void:
	var menu := _menu()
	menu.rebind("jump")
	menu._input(_key(KEY_ESCAPE))
	assert_false(menu.is_waiting_for_key())
	assert_eq(Settings.get_value("bindings"), {})


func test_resetting_the_keys_brings_back_the_usual_ones() -> void:
	var menu := _menu()
	menu.rebind("jump")
	menu._input(_key(KEY_J))
	(menu.find_child("SettingsResetKeys", true, false) as Button).pressed.emit()
	assert_eq(Settings.get_value("bindings"), {})
	assert_eq(Controls.keys_for("jump"), [KEY_SPACE])


func test_done_closes_the_screen() -> void:
	var menu := _menu()
	watch_signals(menu)
	(menu.find_child("SettingsDone", true, false) as Button).pressed.emit()
	assert_false(menu.visible)
	assert_signal_emitted(menu, "closed")


func _own_player() -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "1"
	add_child_autofree(player)
	return player


func test_sensitivity_scales_how_far_the_mouse_turns() -> void:
	var player := _own_player()
	Settings.set_value("mouse_sensitivity", 2.0)
	var start := player.rotation.y
	player.mouse_look(Vector2(100, 0))
	assert_almost_eq(start - player.rotation.y, 100 * player.MOUSE_SENSITIVITY * 2.0, 0.0001)


func test_inverted_look_turns_the_other_way_up_and_down() -> void:
	var player := _own_player()
	player._look(Vector2(0, 0.1))
	var normal: float = player.head.rotation.x
	player.head.rotation.x = 0.0
	Settings.set_value("invert_look", true)
	player._look(Vector2(0, 0.1))
	assert_almost_eq(player.head.rotation.x, -normal, 0.0001)


func test_the_field_of_view_follows_the_setting() -> void:
	var player := _own_player()
	Settings.set_value("fov", 95.0)
	await wait_process_frames(2)
	assert_eq(player.camera.fov, 95.0)


func test_the_touch_settings_toggle_and_size() -> void:
	var menu := _menu()
	(menu.find_child("Setting_touch_left_handed", true, false) as CheckButton).button_pressed = true
	assert_true(Settings.get_value("touch_left_handed"))
	(menu.find_child("Setting_touch_fullscreen", true, false) as CheckButton).button_pressed = false
	assert_false(Settings.get_value("touch_fullscreen"))
	(menu.find_child("Setting_touch_size", true, false) as HSlider).value = 1.25
	assert_eq(Settings.get_value("touch_size"), 1.25)
	assert_eq(SETTINGS_MENU.value_text("touch_size", 1.25), "1.25×")


func test_touch_buttons_cannot_be_made_too_small_or_too_big() -> void:
	Settings.set_value("touch_size", 5.0)
	assert_eq(Settings.get_value("touch_size"), 1.5)
	Settings.set_value("touch_size", 0.1)
	assert_eq(Settings.get_value("touch_size"), 0.7)


func test_the_seeing_and_hearing_settings_are_on_the_screen() -> void:
	var menu := _menu()
	(menu.find_child("Setting_hud_scale", true, false) as HSlider).value = 1.25
	assert_eq(Settings.get_value("hud_scale"), 1.25)
	(menu.find_child("Setting_screen_shake", true, false) as HSlider).value = 0.0
	assert_eq(Settings.get_value("screen_shake"), 0.0)
	assert_eq(SETTINGS_MENU.value_text("screen_shake", 0.5), "50%")
	(menu.find_child("Setting_captions", true, false) as CheckButton).button_pressed = true
	assert_true(Settings.get_value("captions"))
