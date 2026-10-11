extends GutTest

const WORLD := preload("res://world/world.tscn")

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


func test_history_gives_where_a_player_was_between_records() -> void:
	var history := PositionHistory.new()
	history.push(1.0, Vector3(0, 0, 0))
	history.push(1.2, Vector3(4, 0, 0))
	assert_almost_eq(history.at(1.1).x, 2.0, 0.001)


func test_history_holds_at_its_ends() -> void:
	var history := PositionHistory.new()
	history.push(1.0, Vector3(1, 0, 0))
	history.push(1.2, Vector3(4, 0, 0))
	assert_eq(history.at(0.5), Vector3(1, 0, 0))
	assert_eq(history.at(3.0), Vector3(4, 0, 0))


func test_history_forgets_what_is_older_than_it_keeps() -> void:
	var history := PositionHistory.new()
	for i in 100:
		history.push(i * 0.05, Vector3(i, 0, 0))
	assert_almost_eq(history.at(0.0).x, 99.0 - PositionHistory.KEEP / 0.05, 1.0)


func test_history_ignores_records_out_of_order() -> void:
	var history := PositionHistory.new()
	history.push(1.0, Vector3(1, 0, 0))
	history.push(0.5, Vector3(9, 0, 0))
	assert_eq(history.at(0.5), Vector3(1, 0, 0))


func test_the_host_rewinds_by_the_shooters_round_trip_and_the_smoothing() -> void:
	match_._pings[7] = PingTracker.new()
	match_._pings[7].add(0.08)
	assert_almost_eq(match_.rewind_for(7), 0.08 + InterpolationBuffer.DELAY, 0.0001)
	assert_almost_eq(match_.rewind_for(1), InterpolationBuffer.DELAY, 0.0001, "the host's own view")


func test_rewinding_has_a_limit() -> void:
	match_._pings[7] = PingTracker.new()
	match_._pings[7].add(2.0)
	assert_eq(match_.rewind_for(7), Match.MAX_REWIND)


func test_a_shot_hits_where_the_target_was_when_the_shooter_saw_them() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	var target := _add(2, Vector3(3, 0, -5))
	var now := Match.now()
	# The target stood straight ahead a moment ago and has since stepped aside.
	match_.record_position(now - 0.3, Vector3(0, 0, -5), 2)
	match_.record_position(now - 0.05, Vector3(0, 0, -5), 2)
	match_.record_position(now, Vector3(3, 0, -5), 2)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 1.0, -5) - origin).normalized(), 1)
	assert_eq(match_.state.player(2).health, 80, "hit where they were")
	assert_eq(target.global_position, Vector3(3, 0, -5), "they stay where they are")


func test_a_shot_at_where_the_target_is_now_but_was_not_then_misses() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(3, 0, -5))
	var now := Match.now()
	match_.record_position(now - 0.3, Vector3(0, 0, -5), 2)
	match_.record_position(now - 0.05, Vector3(0, 0, -5), 2)
	match_.record_position(now, Vector3(3, 0, -5), 2)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(3, 1.0, -5) - origin).normalized(), 1)
	assert_eq(match_.state.player(2).health, 100)


func test_without_a_history_the_target_is_where_they_stand() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(3, 0, -5))
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(3, 1.0, -5) - origin).normalized(), 1)
	assert_eq(match_.state.player(2).health, 80)


func test_the_hosts_own_movement_is_recorded() -> void:
	_add(1, Vector3(2, 0, 2))
	match_._physics_process(1.0 / 60.0)
	assert_true(match_._histories.has(1))
	assert_eq(match_._histories[1].at(Match.now()), Vector3(2, 0, 2))


func test_others_updates_are_recorded_as_they_arrive() -> void:
	var other := _add(2, Vector3.ZERO)
	other.net_state = PackedFloat32Array([5, 0, 0.5, 0, 0, 0])
	assert_true(match_._histories.has(2))
	assert_eq(match_._histories[2].at(Match.now()), Vector3(5, 0, 0.5))


func test_a_jump_to_somewhere_new_restarts_the_history() -> void:
	var other := _add(2, Vector3.ZERO)
	other.net_state = PackedFloat32Array([5, 0, 6, 0, 0, 0])
	await wait_seconds(0.02)
	# The host sent them there, as on coming back.
	match_._check_for(2).expect_move(Vector3(-10, 0, -10))
	other.net_state = PackedFloat32Array([-10, 0, -10, 0, 0, 1])
	assert_eq(match_._histories[2].at(0.0), Vector3(-10, 0, -10), "nothing from before the jump")


func test_coming_back_into_play_forgets_where_they_were() -> void:
	_add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_.record_position(Match.now(), Vector3(0, 0, -5), 2)
	match_.state.damage(1, 2, 500.0)
	match_._process(MatchState.RESPAWN_DELAY + 0.01)
	assert_false(match_._histories.has(2))
