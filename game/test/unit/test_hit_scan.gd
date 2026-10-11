extends GutTest

const FEET := Vector3(0, 0, -10)


func test_a_ray_straight_at_the_body_hits_its_front() -> void:
	var t := HitScan.ray_capsule(Vector3(0, 1, 0), Vector3.FORWARD, FEET)
	assert_almost_eq(t, 10.0 - HitScan.BODY_RADIUS, 0.001)


func test_a_ray_beside_the_body_misses() -> void:
	assert_eq(HitScan.ray_capsule(Vector3(0.5, 1, 0), Vector3.FORWARD, FEET), INF)


func test_a_ray_just_inside_the_edge_hits() -> void:
	assert_lt(HitScan.ray_capsule(Vector3(0.39, 1, 0), Vector3.FORWARD, FEET), INF)


func test_a_ray_over_the_head_misses() -> void:
	assert_eq(HitScan.ray_capsule(Vector3(0, 1.85, 0), Vector3.FORWARD, FEET), INF)


func test_a_ray_at_the_top_of_the_head_hits_the_rounded_top() -> void:
	var t := HitScan.ray_capsule(Vector3(0, 1.75, 0), Vector3.FORWARD, FEET)
	assert_lt(t, INF)
	# The rounded top is narrower than the body, so the hit is further away.
	assert_gt(t, 10.0 - HitScan.BODY_RADIUS)


func test_a_ray_under_the_feet_misses() -> void:
	assert_eq(HitScan.ray_capsule(Vector3(0, -0.05, 0), Vector3.FORWARD, FEET), INF)


func test_a_ray_pointing_away_misses() -> void:
	assert_eq(HitScan.ray_capsule(Vector3(0, 1, 0), Vector3.BACK, FEET), INF)


func test_a_ray_from_straight_above_hits_the_top() -> void:
	var t := HitScan.ray_capsule(Vector3(0, 10, -10), Vector3.DOWN, FEET)
	assert_almost_eq(t, 10.0 - HitScan.BODY_HEIGHT, 0.001)


func test_a_ray_from_straight_below_hits_the_bottom() -> void:
	var t := HitScan.ray_capsule(Vector3(0, -5, -10), Vector3.UP, FEET)
	assert_almost_eq(t, 5.0, 0.001)


func test_a_ray_starting_inside_does_not_hit() -> void:
	assert_eq(HitScan.ray_capsule(FEET + Vector3(0, 1, 0), Vector3.FORWARD, FEET), INF)


func test_a_slanted_ray_hits_where_expected() -> void:
	var origin := Vector3(3, 1.6, -5)
	var target := FEET + Vector3(0, 1.0, 0)
	var direction := (target - origin).normalized()
	var t := HitScan.ray_capsule(origin, direction, FEET)
	assert_lt(t, origin.distance_to(target))
	var point := origin + direction * t
	var across := Vector2(point.x - FEET.x, point.z - FEET.z).length()
	assert_almost_eq(across, HitScan.BODY_RADIUS, 0.001, "the hit should be on the body's surface")


func test_the_nearest_of_several_players_is_hit() -> void:
	var targets := {7: Vector3(0, 0, -20), 8: Vector3(0, 0, -10), 9: Vector3(0, 0, -30)}
	var hit := HitScan.first_hit(Vector3(0, 1, 0), Vector3.FORWARD, 100.0, targets)
	assert_eq(hit.id, 8)
	assert_almost_eq(hit.distance, 9.6, 0.001)


func test_nobody_beyond_the_range_is_hit() -> void:
	var hit := HitScan.first_hit(Vector3(0, 1, 0), Vector3.FORWARD, 5.0, {8: Vector3(0, 0, -10)})
	assert_true(hit.is_empty())


func test_a_wall_in_front_of_the_player_stops_the_shot() -> void:
	# The caller passes the distance to the wall as the range.
	var hit := HitScan.first_hit(Vector3(0, 1, 0), Vector3.FORWARD, 9.0, {8: Vector3(0, 0, -10)})
	assert_true(hit.is_empty())


func test_a_hit_high_on_the_body_is_a_headshot() -> void:
	var hit := HitScan.first_hit(Vector3(0, 1.6, 0), Vector3.FORWARD, 100.0, {8: FEET})
	assert_true(hit.head)


func test_a_hit_on_the_chest_is_not_a_headshot() -> void:
	var hit := HitScan.first_hit(Vector3(0, 1.2, 0), Vector3.FORWARD, 100.0, {8: FEET})
	assert_false(hit.head)


func test_headshots_count_from_the_feet_of_a_player_on_a_box() -> void:
	var raised := FEET + Vector3(0, 2, 0)
	assert_true(HitScan.first_hit(Vector3(0, 3.6, 0), Vector3.FORWARD, 100.0, {8: raised}).head)
	assert_false(HitScan.first_hit(Vector3(0, 3.0, 0), Vector3.FORWARD, 100.0, {8: raised}).head)


func test_no_targets_means_no_hit() -> void:
	assert_true(HitScan.first_hit(Vector3.ZERO, Vector3.FORWARD, 100.0, {}).is_empty())


func test_a_single_shot_without_spread_goes_where_aimed() -> void:
	var aim := Vector3(1, 0.2, -3).normalized()
	var directions := HitScan.pellet_directions(aim, 1, 0.0, 99)
	assert_eq(directions.size(), 1)
	assert_almost_eq(directions[0].distance_to(aim), 0.0, 0.0001)


func test_pellets_stay_inside_the_cone() -> void:
	var aim := Vector3(0.3, -0.1, -1).normalized()
	var directions := HitScan.pellet_directions(aim, 200, 10.0, 5)
	assert_eq(directions.size(), 200)
	for direction in directions:
		assert_almost_eq(direction.length(), 1.0, 0.0001)
		assert_lte(rad_to_deg(direction.angle_to(aim)), 5.0001)


func test_pellets_spread_out_rather_than_bunching() -> void:
	var directions := HitScan.pellet_directions(Vector3.FORWARD, 200, 10.0, 5)
	var left := 0
	var high := 0
	for direction in directions:
		left += 1 if direction.x < 0 else 0
		high += 1 if direction.y > 0 else 0
	assert_between(left, 70, 130)
	assert_between(high, 70, 130)


func test_the_same_seed_gives_the_same_pellets() -> void:
	var first := HitScan.pellet_directions(Vector3.FORWARD, 8, 6.0, 1234)
	var again := HitScan.pellet_directions(Vector3.FORWARD, 8, 6.0, 1234)
	var other := HitScan.pellet_directions(Vector3.FORWARD, 8, 6.0, 4321)
	assert_eq(first, again)
	assert_ne(first, other)


func test_spread_works_when_aiming_straight_up() -> void:
	for direction in HitScan.pellet_directions(Vector3.UP, 10, 8.0, 3):
		assert_true(direction.is_finite())
		assert_lte(rad_to_deg(direction.angle_to(Vector3.UP)), 4.0001)
