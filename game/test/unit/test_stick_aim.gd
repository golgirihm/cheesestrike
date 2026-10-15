extends GutTest


func test_inside_the_dead_zone_the_stick_does_nothing() -> void:
	assert_eq(StickAim.shaped(Vector2(0.1, 0.05), 0.15, "linear"), Vector2.ZERO)
	assert_eq(StickAim.shaped(Vector2(0.1, 0.0), 0.0, "linear"), Vector2(0.1, 0.0), "with none set, everything counts")


func test_past_the_dead_zone_the_whole_range_is_there() -> void:
	assert_almost_eq(StickAim.shaped(Vector2(1, 0), 0.2, "linear"), Vector2(1, 0), Vector2.ONE * 0.001)
	assert_almost_eq(StickAim.shaped(Vector2(0.6, 0), 0.2, "linear").x, 0.5, 0.001, "halfway past it is half")
	assert_almost_eq(StickAim.shaped(Vector2(0.0, -0.6), 0.2, "linear"), Vector2(0, -0.5), Vector2.ONE * 0.001, "in any direction")


func test_curves_are_gentler_at_first_and_reach_the_same_top() -> void:
	var half := Vector2(0.575, 0)
	var linear := StickAim.shaped(half, 0.15, "linear").x
	var classic := StickAim.shaped(half, 0.15, "classic").x
	var precise := StickAim.shaped(half, 0.15, "precise").x
	assert_gt(linear, classic)
	assert_gt(classic, precise)
	for curve: String in StickAim.CURVES:
		assert_almost_eq(StickAim.shaped(Vector2(1, 0), 0.15, curve).x, 1.0, 0.001, curve)
		assert_true(StickAim.CURVE_NAMES.has(curve))
		assert_true(Settings.CHOICES[StickAim.CURVE].has(curve))


func test_the_assist_slows_the_view_over_an_opponent_and_not_elsewhere() -> void:
	var size := StickAim.size_at(10.0)
	assert_eq(StickAim.slowdown(0.0, size), StickAim.ASSIST_SLOWEST)
	assert_eq(StickAim.slowdown(size * 3.0, size), 1.0, "far from them")
	var between := StickAim.slowdown(size * 1.5, size)
	assert_gt(between, StickAim.ASSIST_SLOWEST)
	assert_lt(between, 1.0, "easing off toward the edge")


func test_the_assist_never_turns_the_view() -> void:
	# It only ever scales what the stick does, so with the stick at rest
	# nothing moves, whoever is in front.
	var at_rest := StickAim.shaped(Vector2.ZERO, 0.15, "linear")
	assert_eq(at_rest * StickAim.slowdown(0.0, 0.1), Vector2.ZERO)
	assert_lte(StickAim.slowdown(0.0, 0.1), 1.0)


func test_the_nearest_opponent_to_the_crosshair_counts_and_far_ones_none() -> void:
	var eye := Vector3.ZERO
	var ahead := Vector3(0, 0, -1)
	assert_eq(StickAim.slowdown_among(eye, ahead, []), 1.0)
	assert_eq(StickAim.slowdown_among(eye, ahead, [Vector3(0, 0, -10), Vector3(10, 0, 0)]), StickAim.ASSIST_SLOWEST)
	assert_eq(StickAim.slowdown_among(eye, ahead, [Vector3(0, 0, -StickAim.ASSIST_RANGE - 5)]), 1.0, "out of range")


func test_a_far_opponent_is_a_smaller_target() -> void:
	assert_gt(StickAim.size_at(5.0), StickAim.size_at(40.0))


func test_rumble_grows_with_the_hurt_and_stays_short() -> void:
	assert_gt(StickAim.hurt_rumble(80).x, StickAim.hurt_rumble(10).x)
	assert_lte(StickAim.hurt_rumble(500).x, 1.0)
	assert_lte(StickAim.hurt_rumble(500).y, 0.33)


func test_a_phone_buzzes_longer_for_a_bigger_hit_and_always_briefly() -> void:
	assert_gt(StickAim.buzz_ms(50), StickAim.buzz_ms(5))
	assert_lte(StickAim.buzz_ms(1000), 150)
	assert_gte(StickAim.buzz_ms(0), 30)
