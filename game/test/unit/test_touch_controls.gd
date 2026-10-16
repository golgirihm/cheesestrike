extends GutTest

const TOUCH := preload("res://ui/touch_controls.gd")

var controls: Control


func before_each() -> void:
	controls = TOUCH.new()
	controls.size = Vector2(1000, 600)
	add_child_autofree(controls)


func test_fire_and_reload_sit_in_the_lower_right() -> void:
	var placed := TOUCH.layout_for(Vector2(1000, 600))
	for name: String in ["fire", "reload"]:
		assert_gt(placed[name].centre.x, 500.0, name)
		assert_gt(placed[name].centre.y, 300.0, name)


func test_the_scores_button_sits_at_the_top_middle() -> void:
	var placed := TOUCH.layout_for(Vector2(1000, 600))
	assert_eq(placed.scores.centre.x, 500.0)
	assert_lt(placed.scores.centre.y, 100.0)


func test_buttons_do_not_overlap_or_leave_the_screen() -> void:
	for screen in [Vector2(1000, 600), Vector2(648, 648), Vector2(1152, 648)]:
		var placed := TOUCH.layout_for(screen)
		var rect := Rect2(Vector2.ZERO, screen)
		for name: String in placed:
			var reach: Vector2 = Vector2.ONE * placed[name].radius
			assert_true(rect.encloses(Rect2(placed[name].centre - reach, reach * 2.0)), "%s at %s" % [name, screen])
		var names := placed.keys()
		for i in names.size():
			for j in range(i + 1, names.size()):
				var a: Dictionary = placed[names[i]]
				var b: Dictionary = placed[names[j]]
				assert_gt(a.centre.distance_to(b.centre), a.radius + b.radius, "%s and %s" % [names[i], names[j]])


func test_nothing_is_a_button_until_the_screen_is_touched() -> void:
	var fire: Vector2 = TOUCH.layout_for(controls.size).fire.centre
	assert_eq(controls.button_at(fire), "")
	controls.notice_touch()
	assert_eq(controls.button_at(fire), "fire")


# A laptop that reports a touch screen is still played with the mouse until
# someone touches it, as is whatever runs the tests.
func test_any_touch_in_the_game_shows_the_buttons() -> void:
	assert_false(controls.shown)
	var touch := InputEventScreenTouch.new()
	touch.pressed = true
	Input.parse_input_event(touch)
	await wait_frames(1)
	assert_true(controls.shown)


func test_each_button_is_found_where_it_is_drawn() -> void:
	controls.notice_touch()
	var placed := TOUCH.layout_for(controls.size)
	assert_eq(controls.button_at(placed.fire.centre), "fire")
	assert_eq(controls.button_at(placed.reload.centre + Vector2(placed.reload.radius - 1, 0)), "reload")
	assert_eq(controls.button_at(Vector2(100, 100)), "")


func test_hidden_buttons_cannot_be_pressed() -> void:
	controls.notice_touch()
	controls.hide()
	assert_eq(controls.button_at(TOUCH.layout_for(controls.size).fire.centre), "")


func test_bigger_buttons_are_bigger_and_further_from_the_edge() -> void:
	var usual := TOUCH.layout_for(Vector2(1000, 600))
	var big := TOUCH.layout_for(Vector2(1000, 600), 1.5)
	assert_almost_eq(big.fire.radius, usual.fire.radius * 1.5, 0.001)
	assert_lt(big.fire.centre.x, usual.fire.centre.x)
	assert_lt(big.fire.centre.y, usual.fire.centre.y)


func test_left_handed_buttons_mirror_the_usual_ones() -> void:
	var usual := TOUCH.layout_for(Vector2(1000, 600))
	var left := TOUCH.layout_for(Vector2(1000, 600), 1.0, true)
	for name: String in usual:
		assert_almost_eq(left[name].centre.x, 1000.0 - usual[name].centre.x, 0.001, name)
		assert_eq(left[name].centre.y, usual[name].centre.y, name)
		assert_eq(left[name].radius, usual[name].radius, name)


func test_buttons_fit_at_every_size_and_on_either_side() -> void:
	for touch_size in [0.7, 1.0, 1.5]:
		for left_handed in [false, true]:
			for screen in [Vector2(1000, 600), Vector2(1152, 648)]:
				var placed := TOUCH.layout_for(screen, touch_size, left_handed)
				var rect := Rect2(Vector2.ZERO, screen)
				for name: String in placed:
					var reach: Vector2 = Vector2.ONE * placed[name].radius
					assert_true(rect.encloses(Rect2(placed[name].centre - reach, reach * 2.0)), "%s at %s, %s, %s" % [name, screen, touch_size, left_handed])


func test_the_buttons_follow_the_settings() -> void:
	var was_size: float = Settings.get_value("touch_size")
	var was_left: bool = Settings.get_value("touch_left_handed")
	controls.notice_touch()
	Settings.set_value("touch_size", 1.3)
	Settings.set_value("touch_left_handed", true)
	var placed := TOUCH.layout_for(controls.size, 1.3, true)
	assert_eq(controls.button_at(placed.fire.centre), "fire")
	assert_lt(placed.fire.centre.x, 500.0)
	Settings.set_value("touch_size", was_size)
	Settings.set_value("touch_left_handed", was_left)


func test_movement_is_on_the_side_away_from_the_buttons() -> void:
	var screen := Vector2(1000, 600)
	assert_true(TOUCH.is_movement_side(Vector2(100, 300), screen, false))
	assert_false(TOUCH.is_movement_side(Vector2(900, 300), screen, false))
	assert_false(TOUCH.is_movement_side(Vector2(100, 300), screen, true))
	assert_true(TOUCH.is_movement_side(Vector2(900, 300), screen, true))


func test_the_menu_button_is_always_there() -> void:
	assert_true(TOUCH.layout_for(Vector2(1000, 600)).has("menu"), "there is no Esc key on a phone")
