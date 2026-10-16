extends GutTest
## The knife: whom a strike reaches, what it does, and how the host checks it.

const WORLD := preload("res://world/world.tscn")
const EYE := BodyShape.HEAD_HEIGHT * BodyShape.SIZE

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3, yaw := 0.0) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	player.rotation.y = yaw
	return player


func _strike(from: Node3D, direction := Vector3.FORWARD) -> void:
	match_._strike(from.name.to_int(), from.head.global_position, direction)


func test_a_strike_fans_out_to_each_side_of_where_it_is_aimed() -> void:
	var fan := Melee.sweep(Vector3.FORWARD)
	assert_eq(fan[0], Vector3.FORWARD, "straight ahead first")
	assert_gt(fan.size(), 2)
	var widest := 0.0
	for way in fan:
		assert_almost_eq(way.length(), 1.0, 0.0001)
		widest = maxf(widest, rad_to_deg(Vector3.FORWARD.angle_to(way)))
	assert_almost_eq(widest, Melee.SWEEP, 0.001)
	assert_true(fan.any(func(way: Vector3) -> bool: return way.x < 0.0))
	assert_true(fan.any(func(way: Vector3) -> bool: return way.x > 0.0))


func test_only_a_strike_from_behind_is_a_backstab() -> void:
	# A player turned by 0 faces -Z; a strike travelling -Z comes from behind them.
	assert_true(Melee.from_behind(Vector3.FORWARD, 0.0))
	assert_false(Melee.from_behind(Vector3.BACK, 0.0), "face to face")
	assert_false(Melee.from_behind(Vector3.RIGHT, 0.0), "from the side")
	assert_true(Melee.from_behind(Vector3.BACK, PI))
	assert_true(Melee.from_behind(Vector3(0.3, -0.5, -1).normalized(), 0.0), "from a little to one side, and above")
	assert_eq(Melee.damage(Vector3.BACK, 0.0), Melee.DAMAGE)
	assert_gt(Melee.damage(Vector3.FORWARD, 0.0), float(PlayerRecord.MAX_HEALTH))
	assert_lt(Melee.DAMAGE, float(PlayerRecord.MAX_HEALTH), "from in front, one strike is not enough")
	assert_gte(Melee.DAMAGE * 2.0, float(PlayerRecord.MAX_HEALTH), "but two are")


func test_the_swing_runs_its_course() -> void:
	assert_eq(Melee.swing(0.0), 0.0)
	assert_between(Melee.swing(Melee.SWING_TIME / 2.0), 0.4, 0.6)
	assert_eq(Melee.swing(Melee.SWING_TIME), 1.0)
	assert_eq(Melee.swing(INF), 1.0, "at rest")
	assert_lt(Melee.SWING_TIME, Melee.INTERVAL)


func test_a_strike_hurts_a_player_within_reach() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 2), PI)
	watch_signals(match_)
	_strike(striker)
	assert_eq(match_.state.player(2).health, 100 - int(Melee.DAMAGE))
	assert_signal_emitted(match_, "hit_confirmed")


func test_a_strike_out_of_reach_does_nothing() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 5.0 - Melee.REACH - BodyShape.RADIUS - 0.5), PI)
	_strike(striker)
	assert_eq(match_.state.player(2).health, 100)


func test_a_strike_reaches_a_little_to_each_side() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0.75, 0, 2), PI)
	# Straight ahead passes by their head.
	assert_true(HitScan.first_hit(striker.head.global_position, Vector3.FORWARD, Melee.REACH, match_._targets_except(1)).is_empty())
	_strike(striker)
	assert_lt(match_.state.player(2).health, 100)


func test_a_strike_from_behind_knocks_a_player_out() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	# Facing away from the striker.
	_add(2, Vector3(0, 0, 2), 0.0)
	match_.state.rename(1, "Sneak")
	match_.state.rename(2, "Dozy")
	watch_signals(match_)
	_strike(striker)
	assert_false(match_.state.player(2).alive)
	assert_eq(match_.state.player(1).kills, 1)
	assert_signal_emitted_with_parameters(match_, "knocked_out", ["Sneak", "Dozy", Melee.ID, false])


func test_the_arena_stops_a_strike() -> void:
	# The block at (0, 1, -10) stands between them, and the strike is aimed low.
	var striker := _add(1, Vector3(0, 0, -7.6))
	_add(2, Vector3(0, 0, -11.0 - BodyShape.RADIUS), PI)
	var low: Vector3 = (Vector3(0, 1.0, -11.0) - striker.head.global_position).normalized()
	_strike(striker, low)
	assert_eq(match_.state.player(2).health, 100)


func test_the_host_allows_one_strike_at_a_time() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 2), PI)
	for swing in 5:
		_strike(striker)
	assert_eq(match_.state.player(2).health, 100 - int(Melee.DAMAGE), "only the first lands")
	match_._clock += Melee.INTERVAL
	_strike(striker)
	assert_false(match_.state.player(2).alive, "the next, once the knife is ready")


func test_a_strike_reported_from_somewhere_else_is_ignored() -> void:
	_add(1, Vector3(0, 0, 12))
	_add(2, Vector3(0, 0, 2), PI)
	match_._strike(1, Vector3(0, EYE, 4), Vector3.FORWARD)
	assert_eq(match_.state.player(2).health, 100)


func test_a_knocked_out_player_cannot_strike_and_teammates_are_spared() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 2), PI)
	match_.state.damage(2, 1, 500.0)
	_strike(striker)
	assert_eq(match_.state.player(2).health, 100)


func test_striking_ends_protection() -> void:
	var striker := _add(1, Vector3(0, 0, 5))
	match_.state.respawn(1)
	assert_true(match_.state.player(1).is_protected())
	_strike(striker)
	assert_false(match_.state.player(1).is_protected())


func test_others_see_the_knife_swing() -> void:
	_add(1, Vector3(0, 0, 12))
	var other := _add(2, Vector3(0, 0, 2))
	watch_signals(match_)
	_strike(other, Vector3.RIGHT)
	assert_signal_emitted_with_parameters(match_, "strike_shown", [2])
	other._animate(0.01)
	var knife: Node3D = other.head.get_node_or_null("Knife")
	assert_not_null(knife)
	assert_true(knife.visible)
	other._animate(Melee.SWING_TIME)
	assert_false(knife.visible, "put away after the swing")


func test_the_owner_strikes_lunges_and_must_wait_to_strike_again() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 1.5), PI)
	await wait_physics_frames(3)
	var before: int = Sounds.counts.get("knife", 0)
	own._melee_pressed = true
	await wait_physics_frames(2)
	assert_eq(Sounds.counts.get("knife", 0), before + 1)
	assert_lt(match_.state.player(2).health, 100)
	assert_lt(own.velocity.z, -1.0, "stepping into it")
	var health: int = match_.state.player(2).health
	own._melee_pressed = true
	await wait_physics_frames(2)
	assert_eq(match_.state.player(2).health, health, "the knife isn't ready again yet")
	assert_eq(Sounds.counts.get("knife", 0), before + 1)
