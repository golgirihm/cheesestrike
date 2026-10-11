extends GutTest

var buffer: InterpolationBuffer


func before_each() -> void:
	buffer = InterpolationBuffer.new()


func _at(x: float, yaw := 0.0, pitch := 0.0) -> PackedFloat32Array:
	return PackedFloat32Array([x, 0.0, 0.0, yaw, pitch])


func test_nothing_arrived_gives_nothing() -> void:
	assert_true(buffer.sample(1.0).is_empty())
	assert_true(buffer.is_empty())


func test_one_update_is_shown_as_it_is() -> void:
	buffer.push(1.0, _at(5.0))
	assert_eq(buffer.sample(0.5)[0], 5.0)
	assert_eq(buffer.sample(1.5)[0], 5.0)


func test_between_two_updates_it_is_shown_in_between() -> void:
	buffer.push(1.0, _at(0.0))
	buffer.push(1.1, _at(10.0))
	assert_almost_eq(buffer.sample(1.05)[0], 5.0, 0.001)
	assert_almost_eq(buffer.sample(1.075)[0], 7.5, 0.001)


func test_it_picks_the_right_pair_among_many() -> void:
	for i in 10:
		buffer.push(i * 0.05, _at(i * 2.0))
	assert_almost_eq(buffer.sample(0.225)[0], 9.0, 0.001)


func test_before_the_first_update_it_holds_the_first() -> void:
	buffer.push(1.0, _at(3.0))
	buffer.push(1.1, _at(4.0))
	assert_eq(buffer.sample(0.0)[0], 3.0)


func test_past_the_newest_it_carries_on_briefly_then_holds() -> void:
	buffer.push(1.0, _at(0.0))
	buffer.push(1.1, _at(1.0))
	assert_almost_eq(buffer.sample(1.15)[0], 1.5, 0.001, "carried on in a straight line")
	assert_almost_eq(buffer.sample(5.0)[0], 1.0 + InterpolationBuffer.MAX_EXTRAPOLATION / 0.1, 0.001, "then held")


func test_turning_goes_the_short_way_round() -> void:
	buffer.push(1.0, _at(0.0, PI - 0.1))
	buffer.push(1.1, _at(0.0, -PI + 0.1))
	var halfway := buffer.sample(1.05)[3]
	assert_almost_eq(absf(wrapf(halfway, -PI, PI)), PI, 0.001, "through behind, not through the front")


func test_looking_up_and_down_is_smoothed() -> void:
	buffer.push(1.0, _at(0.0, 0.0, -0.4))
	buffer.push(1.1, _at(0.0, 0.0, 0.4))
	assert_almost_eq(buffer.sample(1.05)[4], 0.0, 0.001)


func test_late_or_repeated_updates_are_ignored() -> void:
	buffer.push(1.0, _at(0.0))
	buffer.push(1.1, _at(10.0))
	buffer.push(1.05, _at(-100.0))
	buffer.push(1.1, _at(-100.0))
	assert_almost_eq(buffer.sample(1.05)[0], 5.0, 0.001)


func test_short_updates_are_ignored() -> void:
	buffer.push(1.0, PackedFloat32Array([1.0, 2.0]))
	assert_true(buffer.is_empty())


func test_only_the_latest_updates_are_kept() -> void:
	for i in InterpolationBuffer.CAPACITY + 20:
		buffer.push(i * 0.05, _at(i))
	assert_eq(buffer.sample(0.0)[0], 20.0, "the oldest kept")


func test_clearing_forgets_everything() -> void:
	buffer.push(1.0, _at(1.0))
	buffer.clear()
	assert_true(buffer.is_empty())
