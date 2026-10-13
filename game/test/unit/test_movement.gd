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
