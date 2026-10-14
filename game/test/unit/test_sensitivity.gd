extends GutTest


func after_each() -> void:
	for key: String in ["mouse_sensitivity", Sensitivity.AIMING, Sensitivity.SCOPED, Sensitivity.UP_AND_DOWN]:
		Settings.set_value(key, 1.0)


func test_matching_another_game_turns_as_far_for_a_count() -> void:
	var ours := Sensitivity.matching("source", 2.0)
	# Two at 0.022 degrees a count is 0.044 degrees.
	assert_almost_eq(ours * Sensitivity.MOUSE_RADIANS, deg_to_rad(0.044), 0.000001)
	assert_almost_eq(Sensitivity.matching("valorant", 0.5) * Sensitivity.MOUSE_RADIANS, deg_to_rad(0.035), 0.000001)


func test_matching_keeps_within_what_can_be_set_and_refuses_nonsense() -> void:
	assert_eq(Sensitivity.matching("source", 1000.0), Settings.RANGES["mouse_sensitivity"][1])
	assert_eq(Sensitivity.matching("source", 0.0), -1.0)
	assert_eq(Sensitivity.matching("source", NAN), -1.0)
	assert_eq(Sensitivity.matching("solitaire", 1.0), -1.0)


func test_every_game_can_be_chosen_and_has_a_name() -> void:
	for game: String in Sensitivity.OTHER_GAMES:
		assert_true(Settings.CHOICES[Sensitivity.OTHER_GAME].has(game), game)
		assert_gt(Sensitivity.matching(game, 1.0), 0.0, game)


func test_aiming_and_a_scope_each_have_their_own_share() -> void:
	var usual := Sensitivity.aim_factor(0.8, 1.0, false, 1.0, 1.0)
	assert_almost_eq(Sensitivity.aim_factor(0.8, 1.0, false, 1.5, 1.0), usual * 1.5, 0.0001, "aiming")
	assert_almost_eq(Sensitivity.aim_factor(0.8, 1.0, false, 1.0, 0.3), usual, 0.0001, "the scope's share is only for a scope")
	var scoped := Sensitivity.aim_factor(0.3, 1.0, true, 1.0, 1.0)
	assert_almost_eq(Sensitivity.aim_factor(0.3, 1.0, true, 1.0, 0.5), scoped * 0.5, 0.0001, "through a scope")
	assert_eq(Sensitivity.aim_factor(0.3, 0.0, true, 0.2, 0.2), 1.0, "and not at all from the hip")


func test_the_settings_screen_has_them() -> void:
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	for key: String in [Sensitivity.AIMING, Sensitivity.SCOPED, Sensitivity.UP_AND_DOWN, Sensitivity.OTHER_GAME]:
		assert_not_null(menu.find_child("Setting_" + key, true, false), key)
	var theirs: LineEdit = menu.find_child("MatchSensitivity", true, false)
	Settings.set_value(Sensitivity.OTHER_GAME, "source")
	theirs.text_submitted.emit("2")
	assert_almost_eq(Settings.get_value("mouse_sensitivity"), Sensitivity.matching("source", 2.0), 0.0001)
	theirs.text_submitted.emit("lots")
	assert_almost_eq(Settings.get_value("mouse_sensitivity"), Sensitivity.matching("source", 2.0), 0.0001, "nonsense changes nothing")
