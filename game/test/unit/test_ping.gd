extends GutTest

const MATCH := preload("res://match/match.gd")


func test_nothing_measured_reads_zero() -> void:
	var tracker := PingTracker.new()
	assert_false(tracker.has_measured())
	assert_eq(tracker.milliseconds(), 0)


func test_the_first_measurement_is_taken_as_it_is() -> void:
	var tracker := PingTracker.new()
	tracker.add(0.08)
	assert_eq(tracker.milliseconds(), 80)


func test_one_slow_reply_moves_it_only_part_way() -> void:
	var tracker := PingTracker.new()
	tracker.add(0.05)
	tracker.add(0.5)
	assert_between(tracker.milliseconds(), 100, 250)


func test_it_settles_on_a_steady_round_trip() -> void:
	var tracker := PingTracker.new()
	tracker.add(0.3)
	for i in 30:
		tracker.add(0.04)
	assert_almost_eq(tracker.milliseconds(), 40, 1)


func test_nonsense_measurements_are_ignored() -> void:
	var tracker := PingTracker.new()
	tracker.add(0.05)
	tracker.add(-1.0)
	tracker.add(INF)
	assert_eq(tracker.milliseconds(), 50)


func test_the_host_records_a_reply_as_the_players_ping() -> void:
	var match_: Match = MATCH.new()
	add_child_autofree(match_)
	match_.start()
	match_.add_player(1)
	match_._clock = 10.0
	# A reply from this process stands for one from player 1 here; offline the
	# sender id reads as 0, so check through the tracker.
	match_._pings[1] = PingTracker.new()
	match_._pings[1].add(10.0 - 9.93)
	assert_almost_eq(match_.round_trip(1), 0.07, 0.0001)
	assert_eq(match_.round_trip(99), 0.0)
