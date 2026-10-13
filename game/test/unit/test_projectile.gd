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


## A floor at height 0 and nothing else, as Match.wall_finder would report it.
func _floor() -> Callable:
	return func(from: Vector3, to: Vector3) -> Variant:
		if from.y <= 0.0 or to.y > 0.0:
			return INF
		var share := from.y / (from.y - to.y)
		return {"distance": from.distance_to(to) * share, "normal": Vector3.UP}


func _grenade(at: Vector3, direction: Vector3) -> Projectile:
	return Projectile.new(1, 5, Weapons.find(Weapons.GRENADE), at, direction)


func test_a_grenade_bounces_off_the_floor_rather_than_bursting() -> void:
	var grenade := _grenade(Vector3(0, 3, 0), Vector3(0, 0, -1))
	var burst: Variant = null
	var highest_after := 0.0
	for step in 60:
		burst = grenade.advance(1.0 / 60.0, _floor(), {})
		assert_null(burst, "step %d: its fuse has a while to run" % step)
		if grenade.bounces > 0:
			highest_after = maxf(highest_after, grenade.position.y)
	assert_gte(grenade.bounces, 1)
	assert_gte(grenade.position.y, 0.0, "it stays above the floor")
	assert_gt(highest_after, 0.1, "it comes back up")
	assert_lt(highest_after, 3.0 * grenade.weapon.bounce, "but not as high as it fell from")
	assert_lt(grenade.position.z, -5.0, "and carries on forward")


func test_each_bounce_is_lower_until_it_comes_to_rest() -> void:
	var grenade := _grenade(Vector3(0, 2, 0), Vector3(0, 0, -1))
	grenade.weapon = grenade.weapon.duplicate()
	grenade.weapon.fuse = 60.0
	for step in 600:
		grenade.advance(1.0 / 60.0, _floor(), {})
	assert_true(grenade.resting)
	assert_eq(grenade.velocity, Vector3.ZERO)
	assert_almost_eq(grenade.position.y, 0.0, 0.1)
	var rested := grenade.position
	grenade.advance(1.0, _floor(), {})
	assert_eq(grenade.position, rested, "and stays put")


func test_a_grenade_bursts_when_its_fuse_runs_out_wherever_it_is() -> void:
	var grenade := _grenade(Vector3(0, 2, 0), Vector3(0, 0, -1))
	var burst: Variant = null
	var steps := 0
	while burst == null and steps < 1000:
		burst = grenade.advance(1.0 / 60.0, _floor(), {})
		steps += 1
	assert_almost_eq(steps / 60.0, grenade.weapon.fuse, 0.05)
	assert_eq(burst, grenade.position)


func test_a_grenade_at_rest_still_bursts() -> void:
	var grenade := _grenade(Vector3(0, 0.2, 0), Vector3(0, -1, 0))
	var burst: Variant = null
	for step in 600:
		burst = grenade.advance(1.0 / 60.0, _floor(), {})
		if burst != null:
			break
	assert_not_null(burst)
	assert_true(grenade.resting)


func test_a_grenade_passes_players_by() -> void:
	var grenade := _grenade(Vector3(0, BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE, 0), Vector3.FORWARD)
	for step in 30:
		assert_null(grenade.advance(1.0 / 60.0, _no_walls(), {9: Vector3(0, 0, -4)}))
	assert_lt(grenade.position.z, -6.0, "through where the player stands")


func test_a_grenade_comes_back_off_a_wall_whose_facing_is_not_given() -> void:
	var grenade := _grenade(Vector3(0, 2, 0), Vector3.FORWARD)
	for step in 30:
		grenade.advance(1.0 / 60.0, _wall_at_z(-3.0), {})
	assert_gte(grenade.bounces, 1)
	assert_gt(grenade.velocity.z, 0.0, "back the way it came")
	assert_gt(grenade.position.z, -3.0)


func test_a_wheel_from_the_launcher_still_bursts_on_what_it_touches() -> void:
	var wheel := Projectile.new(1, 5, launcher, Vector3(0, 1, 0), Vector3(0, -1, -1))
	var burst: Variant = null
	for step in 120:
		burst = wheel.advance(1.0 / 60.0, _floor(), {})
		if burst != null:
			break
	assert_not_null(burst)
	assert_almost_eq((burst as Vector3).y, 0.0, 0.01)
	assert_eq(wheel.bounces, 0)
