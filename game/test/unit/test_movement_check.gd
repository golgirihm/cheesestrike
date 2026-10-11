extends GutTest

const WORLD := preload("res://world/world.tscn")

var check: MovementCheck
var open := func(_from: Vector3, _to: Vector3) -> bool: return false
var walled := func(_from: Vector3, _to: Vector3) -> bool: return true


func before_each() -> void:
	check = MovementCheck.new()
	check.check(10.0, Vector3.ZERO, 0, open, true)


func test_the_first_update_is_taken_as_it_is() -> void:
	var fresh := MovementCheck.new()
	assert_true(fresh.check(1.0, Vector3(20, 3, 20), 0, open, true))


func test_running_at_full_speed_is_fine() -> void:
	for i in 20:
		assert_true(check.check(10.0 + (i + 1) * 0.05, Vector3((i + 1) * 0.05 * 6.0, 0, 0), 0, open, true), "step %d" % i)


func test_updates_bunched_together_are_fine() -> void:
	assert_true(check.check(10.2, Vector3(1.2, 0, 0), 0, open, true))
	assert_true(check.check(10.201, Vector3(1.5, 0, 0), 0, open, true))


func test_moving_far_too_fast_is_refused() -> void:
	assert_false(check.check(10.05, Vector3(10, 0, 0), 0, open, true))
	assert_eq(check.last_position(), Vector3.ZERO, "the last good place is kept")


func test_an_overload_is_fast_but_allowed() -> void:
	assert_true(check.check(11.0, Vector3(6.0 * 1.35, 0, 0), 0, open, true))


func test_a_shove_allows_more_speed_for_a_moment() -> void:
	check.pushed(10.0, Vector3(9, 4, 0))
	assert_true(check.check(10.5, Vector3(9, 2, 0), 0, open, false))
	var later := MovementCheck.new()
	later.check(10.0, Vector3.ZERO, 0, open, true)
	later.pushed(10.0, Vector3(9, 4, 0))
	assert_false(later.check(13.0, Vector3(40, 0, 0), 0, open, true), "not long after")


func test_rising_faster_than_a_jump_is_refused() -> void:
	assert_true(check.check(10.2, Vector3(0, 1.0, 0), 0, open, false))
	assert_false(check.check(10.3, Vector3(0, 6.0, 0), 0, open, false))


func test_falling_any_speed_is_fine() -> void:
	check.check(10.1, Vector3(0, 0, 0), 0, open, true)
	var high := MovementCheck.new()
	high.check(1.0, Vector3(0, 3, 0), 0, open, true)
	assert_true(high.check(1.05, Vector3(0, 0, 0), 0, open, true))


func test_passing_through_a_wall_is_refused() -> void:
	assert_false(check.check(10.1, Vector3(0.5, 0, 0), 0, walled, true))


func test_hanging_in_the_air_too_long_is_refused() -> void:
	var time := 10.0
	var height := 1.0
	check.check(time + 0.05, Vector3(0, height, 0), 0, open, false)
	for i in 70:
		time += 0.05
		assert_true(check.check(time + 0.05, Vector3(0, height, 0), 0, open, false) or time > 10.0 + MovementCheck.MAX_AIRTIME)
	assert_false(check.check(time + 0.1, Vector3(0, height, 0), 0, open, false))


func test_landing_starts_the_air_clock_again() -> void:
	check.check(10.1, Vector3(0, 1, 0), 0, open, false)
	check.check(13.0, Vector3(0, 0, 0), 0, open, true)
	check.check(13.1, Vector3(0, 1, 0), 0, open, false)
	assert_true(check.check(15.0, Vector3(0, 1, 0), 0, open, false))


func test_a_jump_to_where_the_host_sent_them_is_fine() -> void:
	check.expect_move(Vector3(30, 0, 30))
	assert_true(check.check(10.1, Vector3(30.5, 0, 30), 1, open, true))


func test_a_jump_to_anywhere_else_is_refused() -> void:
	assert_false(check.check(10.1, Vector3(30, 0, 30), 1, open, true), "not sent anywhere")
	check.expect_move(Vector3(30, 0, 30))
	assert_false(check.check(10.1, Vector3(-30, 0, -30), 1, open, true), "sent somewhere else")


func test_nonsense_positions_are_refused() -> void:
	assert_false(check.check(10.1, Vector3(NAN, 0, 0), 0, open, true))
	assert_false(check.check(10.1, Vector3(INF, 0, 0), 0, open, true))


func test_the_host_puts_back_a_player_who_moved_impossibly() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	var match_: Match = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	match_.add_player(2)
	var now := Match.now()
	match_.on_moved(now, Vector3(0, 0, 5), 0, 2)
	match_.on_moved(now + 0.05, Vector3(0, 0, -14), 0, 2)
	assert_eq(match_._histories[2].at(now + 0.05), Vector3(0, 0, 5), "the impossible move isn't believed")
	assert_eq(match_._check_for(2).last_position(), Vector3(0, 0, 5))


func test_the_host_refuses_a_walk_through_a_wall() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	var match_: Match = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	match_.add_player(2)
	var now := Match.now()
	# Either side of the block at (0, 1, -10), a step apart.
	match_.on_moved(now, Vector3(0, 0, -8.7), 0, 2)
	match_.on_moved(now + 0.1, Vector3(0, 0, -11.3), 0, 2)
	assert_eq(match_._check_for(2).last_position(), Vector3(0, 0, -8.7))
