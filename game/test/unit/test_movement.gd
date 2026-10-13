extends GutTest


func test_running_is_faster_than_walking() -> void:
	assert_eq(Movement.speed(false, false), Movement.WALK_SPEED)
	assert_almost_eq(Movement.speed(true, false), Movement.WALK_SPEED * Movement.RUN_FACTOR, 0.0001)
	assert_gt(Movement.RUN_FACTOR, 1.0)


func test_aiming_is_slower_and_cannot_be_run_out_of() -> void:
	assert_almost_eq(Movement.speed(false, true), Movement.WALK_SPEED * Movement.AIM_FACTOR, 0.0001)
	assert_eq(Movement.speed(true, true), Movement.speed(false, true))


func test_a_hit_slows_a_player_and_wears_off() -> void:
	assert_almost_eq(Movement.flinch_factor(Movement.FLINCH_TIME), Movement.FLINCH_SLOW, 0.0001)
	assert_eq(Movement.flinch_factor(0.0), 1.0)
	var half := Movement.flinch_factor(Movement.FLINCH_TIME / 2.0)
	assert_between(half, Movement.FLINCH_SLOW, 1.0)
	assert_almost_eq(Movement.speed(false, false, Movement.FLINCH_TIME), Movement.WALK_SPEED * Movement.FLINCH_SLOW, 0.0001)


func test_a_flinching_player_cannot_run() -> void:
	assert_eq(Movement.speed(true, false, 0.2), Movement.speed(false, false, 0.2))


func test_a_boost_multiplies_whatever_else_applies() -> void:
	assert_almost_eq(Movement.speed(true, false, 0.0, 1.35), Movement.WALK_SPEED * Movement.RUN_FACTOR * 1.35, 0.0001)


func test_a_jump_is_short_and_about_a_metre_and_a_half_high() -> void:
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	var height := Movement.JUMP_VELOCITY * Movement.JUMP_VELOCITY / (2.0 * gravity)
	assert_between(height, 1.3, 1.7)
	assert_lt(2.0 * Movement.JUMP_VELOCITY / gravity, 0.9, "back on the ground in under a second")


func test_the_host_allows_for_the_fastest_a_player_can_go() -> void:
	assert_gt(MovementCheck.MAX_SPEED, Movement.speed(true, false, 0.0, Overload.SPEED_BOOST))
	assert_gt(MovementCheck.MAX_RISE_SPEED, Movement.JUMP_VELOCITY)


func test_crouching_is_slow_and_rules_out_running() -> void:
	assert_almost_eq(Movement.speed(false, false, 0.0, 1.0, true), Movement.WALK_SPEED * Movement.CROUCH_FACTOR, 0.0001)
	assert_eq(Movement.speed(true, false, 0.0, 1.0, true), Movement.speed(false, false, 0.0, 1.0, true))
	assert_lt(Movement.speed(false, true, 0.0, 1.0, true), Movement.speed(false, true), "slower still when aiming too")


func test_in_the_air_a_player_can_only_steer() -> void:
	var forward := Vector2(0, -Movement.WALK_SPEED)
	var steered := Movement.air_steer(forward, -forward, 0.1)
	assert_almost_eq(steered.y, -Movement.WALK_SPEED + Movement.AIR_STEER * 0.1, 0.0001, "slowed a little, not turned about")
	assert_eq(Movement.air_steer(forward, forward, 0.1), forward, "nothing to change")
	assert_eq(Movement.air_steer(Vector2.ZERO, forward, 10.0), forward, "given long enough, they get there")
	# A whole jump isn't long enough to turn right round.
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	var airtime := 2.0 * Movement.JUMP_VELOCITY / gravity
	assert_gt(Movement.air_steer(forward, -forward, airtime).y, -forward.y * -1.0 - 0.001)
	assert_lt(Movement.air_steer(forward, -forward, airtime).length(), Movement.WALK_SPEED)


func test_a_slide_starts_fast_and_eases_to_a_crouched_walk() -> void:
	var run := Movement.WALK_SPEED * Movement.RUN_FACTOR
	assert_almost_eq(Movement.slide_speed(run, 0.0), run * Movement.SLIDE_BOOST, 0.0001)
	assert_gt(Movement.slide_speed(run, Movement.SLIDE_TIME * 0.3), run, "still quicker than running, a third of the way in")
	assert_almost_eq(Movement.slide_speed(run, Movement.SLIDE_TIME), Movement.WALK_SPEED * Movement.CROUCH_FACTOR, 0.0001)
	var last := INF
	for step in 11:
		var speed := Movement.slide_speed(run, Movement.SLIDE_TIME * step / 10.0)
		assert_lte(speed, last, "never speeding up")
		last = speed


func test_only_a_run_can_become_a_slide() -> void:
	assert_false(Movement.can_slide(Movement.WALK_SPEED))
	assert_true(Movement.can_slide(Movement.WALK_SPEED * Movement.RUN_FACTOR))
	assert_false(Movement.can_slide(0.0))


func test_the_host_allows_for_a_slide() -> void:
	var fastest := Movement.slide_speed(Movement.speed(true, false, 0.0, Overload.SPEED_BOOST), 0.0)
	assert_gt(MovementCheck.MAX_SPEED, fastest)
	# And for a climb: up the tallest ledge in the time it takes.
	assert_gt(MovementCheck.MAX_RISE_SPEED, Movement.CLIMB_HEIGHT / (Movement.CLIMB_TIME * 0.7) * 0.8)


func test_a_climb_goes_up_the_face_then_over_the_top() -> void:
	var from := Vector3(0, 0, 0)
	var to := Vector3(0, 2, -2)
	assert_eq(Movement.climb_position(from, to, 0.0), from)
	assert_eq(Movement.climb_position(from, to, Movement.CLIMB_TIME), to)
	assert_eq(Movement.climb_position(from, to, 99.0), to)
	var early := Movement.climb_position(from, to, Movement.CLIMB_TIME * 0.4)
	assert_gt(early.y, 0.5, "well on the way up")
	assert_eq(early.z, 0.0, "before any of the way over")
	var late := Movement.climb_position(from, to, Movement.CLIMB_TIME * 0.8)
	assert_eq(late.y, 2.0, "at the top")
	assert_lt(late.z, -0.5)
