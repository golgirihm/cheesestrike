extends GutTest
## Shots as the host carries them out, in the real arena. Offline, this
## process is the host and peer 1; other players are added as nodes the way
## the spawner would add them.

const WORLD := preload("res://world/world.tscn")
const EYE := Vector3(0, 1.55, 0)

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	match_ = world.match_
	match_.start()
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, 0])
	world.players.add_child(player)
	player.global_position = at
	return player


func _shoot(from: Node3D, at: Vector3) -> void:
	var origin: Vector3 = from.head.global_position
	match_._shot(from.name.to_int(), origin, (at - origin).normalized(), 1)


func test_a_shot_at_another_player_hurts_them() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(0, 1.0, -5))
	assert_eq(match_.state.player(2).health, 100 - 20)


func test_a_shot_at_the_head_does_more() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(0, 1.65, -5))
	assert_eq(match_.state.player(2).health, 100 - 35)


func test_a_shot_that_misses_hurts_nobody() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(3, 1.0, -5))
	assert_eq(match_.state.player(2).health, 100)


func test_a_wall_stops_a_shot() -> void:
	# A cheese block stands at (0, 1, -10), between these two.
	var shooter := _add(1, Vector3(0, 0, -6))
	_add(2, Vector3(0, 0, -13))
	_shoot(shooter, Vector3(0, 1.0, -13))
	assert_eq(match_.state.player(2).health, 100)


func test_only_the_nearest_player_in_line_is_hit() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 0))
	_add(3, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(0, 1.0, -5))
	assert_eq(match_.state.player(2).health, 80)
	assert_eq(match_.state.player(3).health, 100)


func test_a_shot_from_far_from_the_shooter_is_ignored() -> void:
	_add(1, Vector3(0, 0, 10))
	_add(2, Vector3(0, 0, -5))
	match_._shot(1, Vector3(0, 1.5, -2), Vector3.FORWARD, 1)
	assert_eq(match_.state.player(2).health, 100)


func test_a_player_who_is_out_cannot_shoot() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_.state.damage(2, 1, 500.0)
	_shoot(shooter, Vector3(0, 1.0, -5))
	assert_eq(match_.state.player(2).health, 100)


func test_a_player_who_is_out_is_not_hit() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 0))
	_add(3, Vector3(0, 0, -5))
	match_.state.damage(3, 2, 500.0)
	_shoot(shooter, Vector3(0, 1.0, -5))
	assert_eq(match_.state.player(3).health, 80, "the shot passes where the knocked-out player stood")


func test_firing_faster_than_the_weapon_allows_is_refused() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 10:
		_shoot(shooter, Vector3(0, 1.0, -5))
	assert_eq(match_.state.player(2).health, 100 - 3 * 20, "only the allowed burst lands")


func test_enough_hits_knock_a_player_out_and_score() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 3:
		_shoot(shooter, Vector3(0, 1.65, -5))
	assert_false(match_.state.player(2).alive)
	assert_eq(match_.state.player(1).kills, 1)


func test_a_knocked_out_player_comes_back_far_from_the_others() -> void:
	var shooter := _add(1, Vector3(-11, 0, -11))
	var victim := _add(2, Vector3(0, 0, -5))
	match_.state.damage(1, 2, 500.0)
	match_._process(MatchState.RESPAWN_DELAY + 0.01)
	await wait_physics_frames(1)
	var record := match_.state.player(2)
	assert_true(record.alive)
	assert_true(record.is_protected())
	# Victim 2 isn't this process's own player, so the host sends the place
	# by RPC instead of moving it; check the host chose the far corner.
	assert_eq(SpawnChooser.farthest(world.spawn_positions(), [shooter.global_position] as Array[Vector3]), 1)


func test_this_players_own_respawn_moves_them() -> void:
	var own := _add(1, Vector3(0, 0, -5))
	_add(2, Vector3(-11, 0, -11))
	match_.state.damage(2, 1, 500.0)
	match_._process(MatchState.RESPAWN_DELAY + 0.01)
	assert_true(match_.state.player(1).alive)
	assert_gt(own.global_position.distance_to(Vector3(-11, 0, -11)), 20.0)


func test_firing_ends_protection() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_.state.respawn(1)
	assert_true(match_.state.player(1).is_protected())
	_shoot(shooter, Vector3(3, 1.0, -5))
	assert_false(match_.state.player(1).is_protected())


func test_a_knocked_out_player_is_hidden_and_not_solid() -> void:
	_add(1, Vector3(0, 0, 5))
	var other := _add(2, Vector3(0, 0, -5))
	match_.state.damage(1, 2, 500.0)
	match_.state_changed.emit()
	await wait_physics_frames(1)
	assert_false(other.alive)
	assert_false(other.body.visible)
	assert_true(other.collider.disabled)
	match_.state.respawn(2)
	match_.state_changed.emit()
	await wait_physics_frames(1)
	assert_true(other.body.visible)
	assert_false(other.collider.disabled)


func test_the_host_is_told_of_its_own_hit() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	watch_signals(match_)
	_shoot(shooter, Vector3(0, 1.65, -5))
	assert_signal_emitted_with_parameters(match_, "hit_confirmed", [2, 35, false, true])
	assert_signal_not_emitted(match_, "shot_shown", "the shooter shows its own shot")


func test_the_host_is_told_when_it_is_hurt_and_sees_the_shot() -> void:
	_add(1, Vector3(0, 0, -5))
	var shooter := _add(2, Vector3(0, 0, 5))
	watch_signals(match_)
	_shoot(shooter, Vector3(0, 1.0, -5))
	assert_signal_emitted(match_, "hurt")
	var hurt: Array = get_signal_parameters(match_, "hurt")
	assert_eq(hurt[0], 2)
	assert_eq(hurt[1], shooter.head.global_position)
	assert_eq(hurt[2], 20)
	assert_signal_emitted(match_, "shot_shown")
	var ends: Array = get_signal_parameters(match_, "shot_shown")[1]
	assert_eq(ends.size(), 1)
	assert_almost_eq(ends[0].z, -5.0 + HitScan.BODY_RADIUS, 0.01, "the tracer stops at the player hit")


func test_a_missed_shot_ends_at_the_wall() -> void:
	_add(1, Vector3(-5, 0, 0))
	var shooter := _add(2, Vector3(10, 0, 5))
	watch_signals(match_)
	_shoot(shooter, Vector3(10, 1.55, -100))
	var ends: Array = get_signal_parameters(match_, "shot_shown")[1]
	assert_almost_eq(ends[0].z, -16.0, 0.01, "the north wall's inner face")


func test_the_shooters_own_shot_shows_at_once() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	shooter.fired.emit(shooter.head.global_position, Vector3.FORWARD, 1)
	var tracers: int = world.effects.get_child_count()
	assert_eq(tracers, 2, "a flash and a tracer")
