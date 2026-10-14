extends GutTest


func test_a_device_held_still_turns_nothing() -> void:
	assert_eq(GyroAim.look_from(Vector3.ZERO, false, 1.0, 0.016), Vector2.ZERO)
	assert_eq(GyroAim.look_from(Vector3(0.01, -0.01, 0.0), false, 1.0, 0.016), Vector2.ZERO, "a tremble or drift")


func test_turning_the_device_turns_the_view_as_fast() -> void:
	var looked := GyroAim.look_from(Vector3(1.0, 0.0, 0.0), false, 1.0, 0.5)
	assert_almost_eq(looked, Vector2(-0.5, 0.0), Vector2.ONE * 0.0001, "half a second at a radian a second")
	looked = GyroAim.look_from(Vector3(0.0, 2.0, 0.0), false, 1.0, 0.25)
	assert_almost_eq(looked, Vector2(0.0, -0.5), Vector2.ONE * 0.0001, "tilting looks up and down")


func test_spinning_flat_on_a_table_does_nothing() -> void:
	assert_eq(GyroAim.look_from(Vector3(0, 0, 3.0), false, 1.0, 0.1), Vector2.ZERO)


func test_held_the_other_way_round_it_still_turns_the_right_way() -> void:
	var one := GyroAim.look_from(Vector3(1.0, 0.5, 0.0), false, 1.0, 0.1)
	var other := GyroAim.look_from(Vector3(-1.0, -0.5, 0.0), true, 1.0, 0.1)
	assert_almost_eq(one, other, Vector2.ONE * 0.0001)


func test_sensitivity_scales_it() -> void:
	var usual := GyroAim.look_from(Vector3(1.0, 0.0, 0.0), false, 1.0, 0.1)
	assert_almost_eq(GyroAim.look_from(Vector3(1.0, 0.0, 0.0), false, 2.5, 0.1), usual * 2.5, Vector2.ONE * 0.0001)


func test_it_is_off_unless_asked_for() -> void:
	assert_false(Settings.DEFAULTS[GyroAim.SETTING])
