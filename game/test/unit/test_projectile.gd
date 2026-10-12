extends GutTest

var launcher: WeaponStats


func before_each() -> void:
	launcher = Weapons.find("launcher").duplicate()


func _no_walls() -> Callable:
	return func(_from: Vector3, _to: Vector3) -> float: return INF


func _wall_at_z(z: float) -> Callable:
	# A wall across the arena at depth `z`, facing the shooter.
	return func(from: Vector3, to: Vector3) -> float:
		if (from.z - z) * (to.z - z) > 0.0:
			return INF
		return from.distance_to(to) * absf(from.z - z) / absf(from.z - to.z)


func test_it_leaves_at_the_weapons_speed() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3.ZERO, Vector3(0, 0, -2))
	assert_almost_eq(projectile.velocity.length(), launcher.projectile_speed, 0.001)
	assert_eq(projectile.shooter, 5)


func test_it_flies_on_while_nothing_is_in_the_way() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3(0, 2, 0), Vector3.FORWARD)
	assert_null(projectile.advance(0.1, _no_walls(), {}))
	assert_lt(projectile.position.z, -2.0)


func test_it_falls_as_it_flies() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3(0, 10, 0), Vector3.FORWARD)
	for i in 10:
		projectile.advance(0.05, _no_walls(), {})
	assert_lt(projectile.position.y, 10.0)
	assert_lt(projectile.velocity.y, 0.0)


func test_its_path_matches_what_others_draw() -> void:
	var from := Vector3(1, 3, 2)
	var projectile := Projectile.new(1, 5, launcher, from, Vector3(0.3, 0.2, -1))
	var launched := projectile.velocity
	for i in 20:
		projectile.advance(1.0 / 60.0, _no_walls(), {})
	var drawn := Projectile.position_at(from, launched, projectile.gravity(), 20.0 / 60.0)
	assert_almost_eq(projectile.position.distance_to(drawn), 0.0, 0.01)


func test_it_bursts_on_a_wall_at_the_wall() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3(0, 2, 0), Vector3.FORWARD)
	var burst: Variant = null
	for i in 60:
		burst = projectile.advance(1.0 / 60.0, _wall_at_z(-5.0), {})
		if burst != null:
			break
	assert_not_null(burst)
	assert_almost_eq(burst.z, -5.0, 0.01)


func test_it_bursts_on_a_player() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3(0, BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE, 0), Vector3.FORWARD)
	var burst: Variant = null
	for i in 60:
		burst = projectile.advance(1.0 / 60.0, _no_walls(), {9: Vector3(0, 0, -6)})
		if burst != null:
			break
	assert_not_null(burst)
	assert_almost_eq(burst.z, -6.0 + BodyShape.WEDGE_SIZE.z / 2.0 * BodyShape.SIZE, 0.05)


func test_it_bursts_when_its_fuse_runs_out() -> void:
	var projectile := Projectile.new(1, 5, launcher, Vector3(0, 500, 0), Vector3.UP)
	var burst: Variant = null
	var steps := 0
	while burst == null and steps < 1000:
		burst = projectile.advance(0.1, _no_walls(), {})
		steps += 1
	assert_almost_eq(steps * 0.1, launcher.fuse, 0.11)


func test_a_burst_hurts_most_in_the_middle() -> void:
	assert_eq(Splash.damage_at(0.0, 90.0, 4.0), 90.0)
	assert_eq(Splash.damage_at(2.0, 90.0, 4.0), 45.0)
	assert_eq(Splash.damage_at(4.0, 90.0, 4.0), 0.0)
	assert_eq(Splash.damage_at(9.0, 90.0, 4.0), 0.0)
	assert_eq(Splash.damage_at(1.0, 90.0, 0.0), 0.0)


func test_a_burst_shoves_away_and_up() -> void:
	var push := Splash.push(Vector3.ZERO, Vector3(2, 0, 0), 10.0, 4.0)
	assert_gt(push.x, 0.0)
	assert_gt(push.y, 0.0)
	assert_almost_eq(push.length(), 5.0, 0.001, "half strength halfway out")


func test_a_burst_underfoot_shoves_straight_up() -> void:
	var push := Splash.push(Vector3(1, 0, 1), Vector3(1, 0, 1), 10.0, 4.0)
	assert_almost_eq(push.normalized().dot(Vector3.UP), 1.0, 0.001)


func test_nothing_out_of_reach_is_shoved() -> void:
	assert_eq(Splash.push(Vector3.ZERO, Vector3(5, 0, 0), 10.0, 4.0), Vector3.ZERO)
