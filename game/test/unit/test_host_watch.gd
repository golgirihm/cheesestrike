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


func test_starts_neither_silent_nor_gone() -> void:
	assert_false(watch.is_silent())
	assert_false(watch.is_gone())


func test_not_silent_while_heartbeats_arrive() -> void:
	_advance(10.0, true)
	assert_false(watch.is_silent())


func test_not_silent_just_under_the_heartbeat_timeout() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT - 0.1)
	assert_false(watch.is_silent())


func test_silent_once_heartbeats_stop_for_the_timeout() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	assert_true(watch.is_silent())


func test_heartbeat_ends_the_silence() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.heartbeat_received()
	assert_false(watch.is_silent())


func test_one_long_frame_is_not_silence() -> void:
	watch.tick(30.0)
	assert_false(watch.is_silent())


func test_silence_alone_is_not_a_departure() -> void:
	_advance(60.0)
	assert_true(watch.is_silent())
	assert_false(watch.is_gone())


func test_signaling_close_alone_does_not_end_a_live_session() -> void:
	_report_closed()
	_advance(10.0, true)
	assert_false(watch.is_gone())
	assert_false(watch.is_silent())


func test_gone_once_signaling_closed_and_heartbeats_stop() -> void:
	_report_closed()
	_advance(HostWatch.HEARTBEAT_TIMEOUT - 0.1)
	assert_false(watch.is_gone())
	_advance(0.2)
	assert_true(watch.is_gone())


func test_gone_once_the_session_server_twice_says_the_session_is_closed() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.session_reported(false)
	assert_false(watch.is_gone(), "once could be the host reconnecting to the session server")
	watch.session_reported(false)
	assert_true(watch.is_gone())


func test_an_open_report_between_closed_ones_starts_the_count_again() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.session_reported(false)
	watch.session_reported(true)
	watch.session_reported(false)
	assert_false(watch.is_gone())


func test_hearing_the_host_again_starts_the_count_again() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.session_reported(false)
	watch.heartbeat_received()
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	watch.session_reported(false)
	assert_false(watch.is_gone())


func test_gone_when_signaling_closes_after_heartbeats_stopped() -> void:
	_advance(HostWatch.HEARTBEAT_TIMEOUT + 0.1)
	assert_false(watch.is_gone())
	_report_closed()
	assert_true(watch.is_gone())


func _report_closed() -> void:
	for report in HostWatch.CLOSED_REPORTS:
		watch.session_reported(false)
