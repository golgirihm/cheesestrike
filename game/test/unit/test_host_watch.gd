extends GutTest

const FRAME := 1.0 / 60.0

var watch: HostWatch


func before_each() -> void:
	watch = HostWatch.new()


## Runs `seconds` of frames, with the host's heartbeat arriving on schedule
## when `host_alive` is set.
func _advance(seconds: float, host_alive := false) -> void:
	var since_heartbeat := 0.0
	for i in roundi(seconds / FRAME):
		watch.tick(FRAME)
		since_heartbeat += FRAME
		if host_alive and since_heartbeat >= HostWatch.HEARTBEAT_INTERVAL:
			since_heartbeat = 0.0
			watch.heartbeat_received()


func test_starts_neither_paused_nor_gone() -> void:
	assert_false(watch.is_paused())
	assert_false(watch.is_gone())


func test_stays_unpaused_while_heartbeats_arrive() -> void:
	_advance(10.0, true)
	assert_false(watch.is_paused())


func test_not_paused_just_under_the_heartbeat_timeout() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT - 0.1)
	assert_false(watch.is_paused())


func test_paused_once_heartbeats_stop_for_the_timeout() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	assert_true(watch.is_paused())


func test_heartbeat_ends_the_pause() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.heartbeat_received()
	assert_false(watch.is_paused())


func test_one_long_frame_does_not_pause() -> void:
	watch.tick(30.0)
	assert_false(watch.is_paused())


func test_host_hidden_pauses_at_once() -> void:
	watch.host_hidden_changed(true)
	assert_true(watch.is_paused())


func test_host_hidden_stays_paused_while_heartbeats_arrive() -> void:
	watch.host_hidden_changed(true)
	_advance(3.0, true)
	assert_true(watch.is_paused())


func test_host_shown_ends_the_pause() -> void:
	watch.host_hidden_changed(true)
	_advance(5.0)
	watch.host_hidden_changed(false)
	assert_false(watch.is_paused())


func test_silence_alone_is_a_pause_not_a_departure() -> void:
	_advance(60.0)
	assert_true(watch.is_paused())
	assert_false(watch.is_gone())


func test_signaling_close_alone_does_not_end_a_live_session() -> void:
	watch.signaling_closed()
	_advance(10.0, true)
	assert_false(watch.is_gone())
	assert_false(watch.is_paused())


func test_gone_once_signaling_closed_and_heartbeats_stop() -> void:
	watch.signaling_closed()
	_advance(HostWatch.HEARTBEAT_TIMEOUT - 0.1)
	assert_false(watch.is_gone())
	_advance(0.2)
	assert_true(watch.is_gone())


func test_gone_when_signaling_closes_after_heartbeats_stopped() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	assert_false(watch.is_gone())
	watch.signaling_closed()
	assert_true(watch.is_gone())


func test_hidden_host_that_closes_is_gone_after_the_timeout() -> void:
	watch.host_hidden_changed(true)
	watch.signaling_closed()
	assert_false(watch.is_gone())
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	assert_true(watch.is_gone())
