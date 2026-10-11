extends GutTest

const TOUCH := preload("res://ui/touch_controls.gd")

var controls: Control


func before_each() -> void:
	controls = TOUCH.new()
	controls.size = Vector2(1000, 600)
	add_child_autofree(controls)


func test_buttons_sit_in_the_lower_right() -> void:
	var placed := TOUCH.layout_for(Vector2(1000, 600))
	for name: String in placed:
		assert_gt(placed[name].centre.x, 500.0, name)
		assert_gt(placed[name].centre.y, 300.0, name)


func test_buttons_do_not_overlap_or_leave_the_screen() -> void:
	for screen in [Vector2(1000, 600), Vector2(648, 648), Vector2(1152, 648)]:
		var placed := TOUCH.layout_for(screen)
		var rect := Rect2(Vector2.ZERO, screen)
		for name: String in placed:
			var reach: Vector2 = Vector2.ONE * placed[name].radius
			assert_true(rect.encloses(Rect2(placed[name].centre - reach, reach * 2.0)), "%s at %s" % [name, screen])
		assert_gt(placed.fire.centre.distance_to(placed.reload.centre), placed.fire.radius + placed.reload.radius)


func test_nothing_is_a_button_until_the_screen_is_touched() -> void:
	controls.shown = false
	var fire: Vector2 = TOUCH.layout_for(controls.size).fire.centre
	assert_eq(controls.button_at(fire), "")
	controls.notice_touch()
	assert_eq(controls.button_at(fire), "fire")


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
