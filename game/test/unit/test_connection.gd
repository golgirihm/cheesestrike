extends GutTest

const MAIN := preload("res://main.gd")


func test_a_good_connection_and_the_host_are_not_warned_of() -> void:
	assert_eq(Readings.warning(60, 0, false), "")
	assert_eq(Readings.warning(900, 50, true), "", "the host has no connection to warn of")


func test_a_slow_or_lossy_connection_is_warned_of() -> void:
	assert_eq(Readings.warning(Readings.SLOW_PING + 40, 0, false), "⚠ Slow connection: 240 ms")
	assert_eq(Readings.warning(80, 25, false), "⚠ Losing 25% of updates")
	assert_true(Readings.warning(900, 25, false).contains("Losing"), "loss is the worse of the two")


func test_loss_is_not_warned_of_until_enough_is_counted() -> void:
	var readings := Readings.new()
	autofree(readings)
	readings.heard(1.0, 1.0)
	readings.heard(3.0, 1.0)
	assert_eq(readings.loss(), 33, "one of three went astray")
	assert_eq(readings.settled_loss(), 0)
	for second in 10:
		readings.heard(4.0 + second, 1.0)
	assert_gt(readings.settled_loss(), 0)


func test_the_wait_to_try_again_counts_down_in_whole_seconds() -> void:
	assert_eq(MAIN.reconnect_text("Lost the connection.", 2.4), "Lost the connection.\nTrying again in 3…")
	assert_eq(MAIN.reconnect_text("Lost the connection.", 0.2), "Lost the connection.\nTrying again in 1…")
