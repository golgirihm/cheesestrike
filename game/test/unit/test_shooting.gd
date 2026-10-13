extends GutTest
## Shots as the host carries them out, in the real arena. Offline, this
## process is the host and peer 1; other players are added as nodes the way
## the spawner would add them.

const WORLD := preload("res://world/world.tscn")
## A height low on a player's wedge: a body shot, under the face and not
## between the legs.
const CHEST := (BodyShape.HIP_HEIGHT + 0.25) * BodyShape.SIZE

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	# The small arena, whose layout these tests are written around.
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	# In hand, a Cheddar Blaster, which these tests' numbers are for.
	match_.state.player(id).weapons.append("blaster")
	match_.state.player(id).weapon = "blaster"
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


## The middle of one of player `id`'s eyes: a headshot.
func _eye_of(id: int) -> Vector3:
	var target: Node3D = world.player_node(id)
	var eye: Vector3 = BodyShape.eye_centres()[0]
	return target.global_position + Basis(Vector3.UP, target.rotation.y) * eye * BodyShape.SIZE


func _shoot(from: Node3D, at: Vector3) -> void:
	var origin: Vector3 = from.head.global_position
	match_._shot(from.name.to_int(), origin, (at - origin).normalized(), 1)


func test_a_shot_at_another_player_hurts_them() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100 - 20)


func test_a_shot_at_the_head_does_more() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, _eye_of(2))
	assert_eq(match_.state.player(2).health, 100 - 35)


func test_a_shot_that_misses_hurts_nobody() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(3, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100)


func test_a_wall_stops_a_shot() -> void:
	# A cheese block stands at (0, 1, -10), between these two; the shot is aimed
	# low at the player close behind it, so the block is in its way.
	var shooter := _add(1, Vector3(0, 0, 10))
	_add(2, Vector3(0, 0, -11.0 - BodyShape.RADIUS))
	_shoot(shooter, Vector3(0, (BodyShape.HIP_HEIGHT + 0.05) * BodyShape.SIZE, -11.0 - BodyShape.RADIUS))
	assert_eq(match_.state.player(2).health, 100)


func test_only_the_nearest_player_in_line_is_hit() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 0))
	_add(3, Vector3(0, 0, -5))
	_shoot(shooter, Vector3(0, CHEST, -5))
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
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100)


func test_a_player_who_is_out_is_not_hit() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, 0))
	_add(3, Vector3(0, 0, -5))
	match_.state.damage(3, 2, 500.0)
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(3).health, 80, "the shot passes where the knocked-out player stood")


func test_firing_faster_than_the_weapon_allows_is_refused() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 10:
		_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100 - 3 * 20, "only the allowed burst lands")


func test_enough_hits_knock_a_player_out_and_score() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 3:
		_shoot(shooter, _eye_of(2))
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
	_shoot(shooter, Vector3(3, CHEST, -5))
	assert_false(match_.state.player(1).is_protected())


func test_a_knocked_out_player_is_hidden_and_not_solid() -> void:
	_add(1, Vector3(0, 0, 5))
	var other := _add(2, Vector3(0, 0, -5))
	match_.state.damage(1, 2, 500.0)
	match_.state_changed.emit()
	await wait_physics_frames(1)
	assert_false(other.alive)
	assert_true(other.collider.disabled, "at once")
	await wait_seconds(other.body.MELT_TIME + 0.1)
	assert_false(other.body.visible, "once it has melted")
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
	_shoot(shooter, _eye_of(2))
	assert_signal_emitted_with_parameters(match_, "hit_confirmed", [2, 35, false, true])
	assert_signal_not_emitted(match_, "shot_shown", "the shooter shows its own shot")


func test_the_host_is_told_when_it_is_hurt_and_sees_the_shot() -> void:
	_add(1, Vector3(0, 0, -5))
	var shooter := _add(2, Vector3(0, 0, 5))
	watch_signals(match_)
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_signal_emitted(match_, "hurt")
	var hurt: Array = get_signal_parameters(match_, "hurt")
	assert_eq(hurt[0], 2)
	assert_eq(hurt[1], shooter.head.global_position)
	assert_eq(hurt[2], 20)
	assert_signal_emitted(match_, "shot_shown")
	var ends: Array = get_signal_parameters(match_, "shot_shown")[1]
	assert_eq(ends.size(), 1)
	assert_almost_eq(ends[0].z, -5.0 + BodyShape.WEDGE_SIZE.z / 2.0 * BodyShape.SIZE, 0.01, "the tracer stops at the player hit")


func test_a_missed_shot_ends_at_the_wall() -> void:
	_add(1, Vector3(-5, 0, 0))
	var shooter := _add(2, Vector3(10, 0, 5))
	watch_signals(match_)
	_shoot(shooter, Vector3(10, 2.3, -100))
	var ends: Array = get_signal_parameters(match_, "shot_shown")[1]
	assert_almost_eq(ends[0].z, -16.0, 0.01, "the north wall's inner face")


func test_the_shooters_own_shot_shows_at_once() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	shooter.fired.emit(shooter.head.global_position, Vector3.FORWARD, 1, WeaponStats.AIMED, 0)
	assert_eq(world.effects.showing("flash"), 1)
	assert_eq(world.effects.showing("tracer"), 1)


func test_a_knock_out_is_announced_with_both_names() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_.state.rename(1, "Shooter")
	match_.state.rename(2, "Target")
	watch_signals(match_)
	for i in 3:
		_shoot(shooter, _eye_of(2))
	assert_signal_emit_count(match_, "knocked_out", 1)
	assert_signal_emitted_with_parameters(match_, "knocked_out", ["Shooter", "Target", "blaster", true])


func _teams() -> void:
	match_.start(MatchSettings.from_dict({"mode": "teams", "kill_limit": 3, "map": "box"}))


func test_teammates_shots_pass_without_hurting() -> void:
	_teams()
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(10, 0, 0))
	_add(3, Vector3(0, 0, -5))
	assert_eq(match_.state.player(1).team, match_.state.player(3).team, "1 and 3 share a team")
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(3).health, 100)


func test_a_new_match_brings_everyone_back_with_scores_cleared() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 3:
		_shoot(shooter, _eye_of(2))
	match_.state.end_match()
	match_._process(MatchState.INTERMISSION + 0.01)
	assert_true(match_.state.is_playing())
	for id in [1, 2]:
		var record := match_.state.player(id)
		assert_true(record.alive)
		assert_eq(record.kills, 0)
		assert_eq(record.deaths, 0)
		assert_true(record.is_protected())


func test_the_match_ends_when_the_kill_limit_is_reached() -> void:
	match_.start(MatchSettings.from_dict({"kill_limit": 1, "map": "box"}))
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	for i in 3:
		_shoot(shooter, _eye_of(2))
	assert_false(match_.state.is_playing())
	assert_eq(match_.state.winners, [1] as Array[int])


func _give(id: int, weapon: String) -> void:
	match_.state.player(id).weapons.append(weapon)


func test_a_weapon_not_carried_cannot_be_fired() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_._shot(1, shooter.head.global_position, Vector3.FORWARD, 1, "shotgun")
	assert_eq(match_.state.player(2).health, 100)


func test_a_shotgun_blast_up_close_lands_many_pellets() -> void:
	var shooter := _add(1, Vector3(0, 0, 0))
	_add(2, Vector3(0, 0, -2))
	_give(1, "shotgun")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, -2) - origin).normalized(), 7, "shotgun")
	var dealt := 100 - match_.state.player(2).health
	assert_gt(dealt, 50, "most pellets hit at two metres")


func test_a_shotgun_blast_far_away_mostly_misses() -> void:
	var shooter := _add(1, Vector3(-12, 0, 12))
	_add(2, Vector3(12, 0, 12))
	_give(1, "shotgun")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(12, CHEST, 12) - origin).normalized(), 7, "shotgun")
	assert_lt(100 - match_.state.player(2).health, 40)


func _fly(seconds: float) -> void:
	for i in int(seconds * 60.0):
		match_._physics_process(1.0 / 60.0)


func test_a_launcher_wheel_bursts_on_a_player_and_hurts_them() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -3))
	_give(1, "launcher")
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, -3) - origin).normalized(), 1, "launcher")
	assert_signal_emitted(match_, "projectile_launched")
	_fly(1.0)
	assert_signal_emitted(match_, "projectile_burst")
	assert_lt(match_.state.player(2).health, 100)
	assert_eq(match_.state.player(1).health, 100, "the shooter is out of reach")


func test_a_burst_hurts_its_shooter_too() -> void:
	var shooter := _add(1, Vector3(0, 0, -7))
	_add(2, Vector3(12, 0, 12))
	_give(1, "launcher")
	watch_signals(match_)
	# Straight at the cheese block just ahead.
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 1, -10) - origin).normalized(), 1, "launcher")
	_fly(1.0)
	assert_lt(match_.state.player(1).health, 100)
	assert_signal_emitted(match_, "hurt", "they are told, as when anyone hurts them")
	assert_signal_not_emitted(match_, "hit_confirmed", "but it is no hit to be pleased with")


func test_a_player_who_bursts_themselves_out_scores_nothing() -> void:
	var shooter := _add(1, Vector3(0, 0, -7))
	_add(2, Vector3(12, 0, 12))
	_give(1, "launcher")
	match_.state.rename(1, "Shooter")
	match_.state.player(1).health = 10
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 1, -10) - origin).normalized(), 1, "launcher")
	_fly(1.0)
	var record := match_.state.player(1)
	assert_false(record.alive)
	assert_eq(record.kills, 0)
	assert_eq(record.deaths, 1)
	assert_signal_emitted_with_parameters(match_, "knocked_out", ["", "Shooter", "launcher", false])


func test_a_launcher_wheel_knocks_out_a_healthy_player_it_lands_on() -> void:
	var shooter := _add(1, Vector3(0, 0, 14))
	_add(2, Vector3(0, 0, 0))
	_give(1, "launcher")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, 0) - origin).normalized(), 1, "launcher")
	_fly(1.5)
	assert_false(match_.state.player(2).alive)
	assert_eq(match_.state.player(1).health, 100, "the shooter stood well back")


func test_a_shot_from_the_hip_strays_and_an_aimed_one_does_not() -> void:
	_add(1, Vector3(-5, 0, 0))
	var shooter := _add(2, Vector3(10, 0, 5))
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	var straight := (Vector3(10, 2.3, -100) - origin).normalized()
	var blaster := Weapons.find("blaster")
	match_._shot(2, origin, straight, 5, "blaster", WeaponStats.AIMED)
	var aimed: Vector3 = get_signal_parameters(match_, "shot_shown")[1][0]
	match_._rules_for(2).refill()
	match_._shot(2, origin, straight, 5, "blaster", 0)
	var hip: Vector3 = get_signal_parameters(match_, "shot_shown")[1][0]
	var off_aimed := rad_to_deg(straight.angle_to(aimed - origin))
	var off_hip := rad_to_deg(straight.angle_to(hip - origin))
	assert_lte(off_aimed, blaster.spread_degrees / 2.0 + 0.01)
	assert_gt(off_hip, off_aimed)
	assert_lte(off_hip, blaster.spread_for(0) / 2.0 + 0.01)


## Lets this process's own player finish taking its weapon in hand.
func _settle(own: Node3D) -> void:
	await wait_physics_frames(3)
	own._switch_left = 0.0
	own.weapon.cooldown = 0.0
	own._animate(0.0)


func test_holding_aim_brings_the_sights_up_and_slows_the_owner() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	assert_false(own.is_aiming())
	var hip_spread: float = own.shot_spread()
	var wide: float = own.camera.fov
	Input.action_press("aim")
	await wait_seconds(own.AIM_TIME + 0.1)
	assert_true(own.is_aiming())
	assert_lt(own.shot_spread(), hip_spread)
	assert_lt(own.camera.fov, wide, "the view closes in")
	var speed: float = await _speed_of(own)
	Input.action_release("aim")
	assert_almost_eq(speed, Movement.WALK_SPEED * Movement.AIM_FACTOR, 0.01)
	await wait_seconds(own.AIM_TIME + 0.1)
	assert_false(own.is_aiming())
	assert_almost_eq(own.camera.fov, wide, 0.01)


func test_the_owner_tells_the_host_whether_a_shot_was_aimed() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	watch_signals(own)
	own._fire_pressed = true
	await wait_physics_frames(2)
	assert_eq(get_signal_parameters(own, "fired")[3] & WeaponStats.AIMED, 0, "from the hip")
	Input.action_press("aim")
	await wait_seconds(own.AIM_TIME + own.weapon.stats.fire_interval + 0.1)
	own._fire_pressed = true
	await wait_physics_frames(2)
	Input.action_release("aim")
	assert_eq(get_signal_parameters(own, "fired")[3] & WeaponStats.AIMED, WeaponStats.AIMED, "down the sights")


func test_a_weapon_changed_to_swings_up_from_below() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	var rest: Vector3 = own.gun.position
	_give(1, "shotgun")
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, "shotgun")
	own._animate(0.01)
	assert_lt(own.gun.position.y, rest.y - 0.1, "it starts low")
	assert_false(own.weapon.can_fire(), "and can't fire until it is up")
	own._animate(own.SWITCH_TIME)
	assert_almost_eq(own.gun.position.distance_to(rest), 0.0, 0.02)


func test_a_burst_behind_a_wall_does_no_harm() -> void:
	var shooter := _add(1, Vector3(0, 0, -6))
	_add(2, Vector3(0, 0, -11.0 - BodyShape.RADIUS))
	_give(1, "launcher")
	# The block at (0, 1, -10) stands between them; burst low on its near face.
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 0.5, -9) - origin).normalized(), 1, "launcher")
	_fly(1.0)
	assert_eq(match_.state.player(2).health, 100)


func test_a_burst_shoves_the_players_near_it() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	var target := _add(2, Vector3(0, 0, -3))
	_give(1, "launcher")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, CHEST, -3) - origin).normalized(), 1, "launcher")
	watch_signals(match_)
	_fly(1.0)
	# Player 2 isn't this process's own, so the shove goes by RPC; the host's
	# own player would be moved at once.
	assert_signal_emitted(match_, "projectile_burst")
	var at: Vector3 = get_signal_parameters(match_, "projectile_burst")[2]
	assert_lt(at.distance_to(target.global_position + Vector3.UP * CHEST), 1.0)


func test_the_hosts_own_player_is_shoved_by_its_own_burst() -> void:
	var shooter := _add(1, Vector3(0, 0, -7))
	_give(1, "launcher")
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 0.2, -8.5) - origin).normalized(), 1, "launcher")
	_fly(0.5)
	assert_gt(shooter.velocity.y, 0.0, "lifted")
	assert_gt(shooter._knockback.length(), 0.0)


func test_choosing_a_carried_weapon_shows_it_to_others() -> void:
	_add(1, Vector3(0, 0, 5))
	_give(1, "grater")
	match_.select_weapon("grater")
	assert_eq(match_.state.player(1).weapon, "grater")
	match_.select_weapon("launcher")
	assert_eq(match_.state.player(1).weapon, "grater", "not carried, so refused")


func test_the_owner_takes_up_a_new_weapon_and_drops_it_when_knocked_out() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	_give(1, "shotgun")
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, "shotgun", "a new weapon comes into hand")
	assert_eq(own.arsenal.keys().size(), 3, "with the sidearm and the blaster")
	match_.state.damage(2, 1, 500.0)
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, Weapons.STARTING)
	assert_eq(own.arsenal.keys(), [Weapons.STARTING])


func test_switching_weapons_takes_a_moment() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	_give(1, "grater")
	match_.state_changed.emit()
	own.equip("blaster")
	assert_false(own.weapon.can_fire())
	own.weapon.tick(own.SWITCH_TIME)
	assert_true(own.weapon.can_fire())


func _pad(kind: String) -> Dictionary:
	for pad: Dictionary in world.pickup_pads():
		if pad.kind == kind:
			return pad
	return {}


func test_standing_on_a_weapon_pad_collects_it() -> void:
	var pad := _pad("launcher")
	var own := _add(1, pad.position)
	match_._physics_process(1.0 / 60.0)
	assert_true(match_.state.player(1).weapons.has("launcher"))
	assert_eq(own.weapon.stats.id, "launcher", "a new weapon comes into hand")


func test_a_hurt_player_heals_on_a_wedge() -> void:
	var pad := _pad(Pickups.HEALTH)
	_add(1, pad.position)
	_add(2, Vector3(10, 0, -10))
	match_.state.damage(2, 1, 60.0)
	watch_signals(match_)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.player(1).health, 90)
	assert_signal_emitted_with_parameters(match_, "picked_up", [1, Pickups.HEALTH])


func test_a_weapon_already_carried_is_refilled_on_its_pad() -> void:
	var pad := _pad("grater")
	var own := _add(1, Vector3(0, 0, 10))
	_give(1, "grater")
	match_.state_changed.emit()
	own.arsenal["grater"].ammo = 3
	own.global_position = pad.position
	match_._physics_process(1.0 / 60.0)
	assert_eq(own.arsenal["grater"].ammo, Weapons.find("grater").magazine)


func test_every_pad_is_on_the_floor_and_holds_a_known_pickup() -> void:
	var space := world.get_world_3d().direct_space_state
	assert_gt(world.pickup_pads().size(), 0)
	for pad: Dictionary in world.pickup_pads():
		assert_true(Pickups.is_kind(pad.kind), pad.kind)
		var from: Vector3 = pad.position + Vector3.UP * 0.2
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.DOWN * 0.5))
		assert_false(hit.is_empty(), "%s pad floats" % pad.kind)


func test_every_weapon_but_the_first_can_be_picked_up() -> void:
	var kinds: Array = world.pickup_pads().map(func(pad: Dictionary) -> String: return pad.kind)
	for id: String in Weapons.ids():
		if id != Weapons.STARTING:
			assert_has(kinds, id)
	assert_has(kinds, Pickups.HEALTH)


func test_a_new_player_starts_at_their_spot_on_every_machine() -> void:
	# Player 2 isn't this process's own, as on the host when someone joins.
	var spot: Vector3 = world.spawn_positions()[3]
	var other: Node3D = world._create_player([2, spot])
	world.players.add_child(other)
	assert_eq(other.global_position, spot)
	other.queue_free()


func test_setting_off_an_overload_makes_the_owner_faster() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await wait_physics_frames(3)
	match_.state.player(1).overload = Overload.METER_FULL
	watch_signals(match_)
	match_.set_off_overload()
	assert_signal_emitted_with_parameters(match_, "overloaded", [1])
	Input.action_press("move_forward")
	await wait_physics_frames(2)
	var speed := Vector2(own.velocity.x, own.velocity.z).length()
	Input.action_release("move_forward")
	assert_almost_eq(speed, Movement.WALK_SPEED * Overload.SPEED_BOOST, 0.01)


func _speed_of(player: Node3D) -> float:
	Input.action_press("move_forward")
	await wait_physics_frames(2)
	var speed := Vector2(player.velocity.x, player.velocity.z).length()
	Input.action_release("move_forward")
	return speed


func test_holding_the_run_key_makes_the_owner_run() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await wait_physics_frames(3)
	Input.action_press("sprint")
	var speed: float = await _speed_of(own)
	Input.action_release("sprint")
	assert_almost_eq(speed, Movement.WALK_SPEED * Movement.RUN_FACTOR, 0.01)


func test_being_hurt_slows_the_owner_for_a_moment() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await wait_physics_frames(3)
	match_.hurt.emit(2, Vector3.ZERO, 10)
	var slowed: float = await _speed_of(own)
	assert_lt(slowed, Movement.WALK_SPEED * 0.6)
	assert_gte(slowed, Movement.WALK_SPEED * Movement.FLINCH_SLOW)
	own._flinch_left = 0.0
	assert_almost_eq(await _speed_of(own), Movement.WALK_SPEED, 0.01)


func test_an_overload_that_is_not_ready_does_nothing() -> void:
	_add(1, Vector3(0, 0, 5))
	watch_signals(match_)
	match_.set_off_overload()
	assert_signal_not_emitted(match_, "overloaded")


func test_a_new_match_forgets_players_who_left() -> void:
	_add(1, Vector3(0, 0, 5))
	match_._known_as(2, "tab-a")
	_add(2, Vector3(0, 0, -5))
	match_.state.player(2).kills = 3
	match_.remove_player(2)
	match_.state.end_match()
	match_._process(MatchState.INTERMISSION + 0.01)
	assert_true(match_._departed.is_empty())


func test_everyone_sees_a_player_who_is_hurt_wince() -> void:
	_add(1, Vector3(0, 0, 5))
	var other := _add(2, Vector3(0, 0, -5))
	match_.state_changed.emit()
	assert_false(other.body.is_grimacing())
	match_.state.damage(1, 2, 10.0)
	match_.state_changed.emit()
	assert_true(other.body.is_grimacing())


func test_coming_back_at_full_health_is_nothing_to_wince_at() -> void:
	_add(1, Vector3(0, 0, 5))
	var other := _add(2, Vector3(0, 0, -5))
	match_.state.damage(1, 2, 500.0)
	match_.state_changed.emit()
	assert_false(other.body.is_grimacing(), "knocked out, they melt instead")
	match_.state.respawn(2)
	match_.state_changed.emit()
	assert_false(other.body.is_grimacing())


func test_no_player_is_coloured_like_cheese_to_eat() -> void:
	for id in range(1, 200):
		var hue: float = preload("res://player/player.gd").colour_for(id).h
		assert_false(hue > 0.11 and hue < 0.2, "player %d is yellow" % id)


func test_the_host_adds_the_weapons_kick_to_a_run_of_fire() -> void:
	_add(1, Vector3(-5, 0, 0))
	var shooter := _add(2, Vector3(10, 0, 5))
	watch_signals(match_)
	var origin: Vector3 = shooter.head.global_position
	var heights := []
	for shot in 3:
		match_._shot(2, origin, Vector3.FORWARD, 5, "blaster", WeaponStats.AIMED)
		var end: Vector3 = get_signal_parameters(match_, "shot_shown")[1][0]
		heights.append(rad_to_deg(asin((end - origin).normalized().y)))
	var blaster := Weapons.find("blaster")
	# Each within the weapon's aimed spread of where the kick puts it.
	var slack := blaster.spread_for(WeaponStats.AIMED, 2) / 2.0 + 0.01
	for shot in 3:
		var kick := rad_to_deg(Recoil.offset(blaster, shot, true).y)
		assert_almost_eq(heights[shot], kick, slack, "shot %d" % shot)
	assert_gt(heights[2], heights[0], "the third lands above the first")


func test_a_pause_lets_the_kick_settle() -> void:
	_add(1, Vector3(-5, 0, 0))
	var shooter := _add(2, Vector3(10, 0, 5))
	var origin: Vector3 = shooter.head.global_position
	for shot in 3:
		match_._shot(2, origin, Vector3.FORWARD, 5, "blaster", WeaponStats.AIMED)
	assert_eq(match_._rules_for(2).run(Weapons.find("blaster")), 2)
	match_._clock += 1.0
	match_._shot(2, origin, Vector3.FORWARD, 5, "blaster", WeaponStats.AIMED)
	assert_eq(match_._rules_for(2).run(Weapons.find("blaster")), 0)


func test_a_hit_from_far_away_does_less() -> void:
	var shooter := _add(1, Vector3(-14, 0, 14))
	_add(2, Vector3(-14, 0, -14))
	_give(1, "grater")
	var origin: Vector3 = shooter.head.global_position
	var grater := Weapons.find("grater")
	# Tried with one seed after another until a shot lands: the grater
	# spreads a little even when aimed.
	for seed in 20:
		match_._rules_for(1).refill()
		match_._shot(1, origin, (Vector3(-14, CHEST, -14) - origin).normalized(), seed, "grater", WeaponStats.AIMED)
		if match_.state.player(2).health < 100:
			break
	var dealt: int = 100 - match_.state.player(2).health
	assert_gt(dealt, 0, "it still hits")
	assert_lt(dealt, roundi(grater.damage), "but for less than up close")


func test_the_owners_view_climbs_with_a_run_of_fire_and_settles() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	watch_signals(own)
	Input.action_press("fire")
	await wait_seconds(0.5)
	Input.action_release("fire")
	assert_gt(own._run, 1, "a run of several shots")
	assert_gt(own.camera.rotation.x, 0.002, "the view has climbed")
	var last: Array = get_signal_parameters(own, "fired")
	assert_gt(last[1].y, 0.0, "and the shots with it")
	assert_eq(last[4], own._run)
	await wait_seconds(1.0)
	assert_almost_eq(own.camera.rotation.x, 0.0, 0.002, "it settles")
	assert_eq(own.next_in_run(), 0)


func test_walking_over_ammunition_tops_up_every_weapon_carried() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	_give(1, "shotgun")
	match_.state_changed.emit()
	var pad := _pad(Pickups.AMMO)
	var before: int = own.arsenal["blaster"].reserve
	var shells: int = own.arsenal["shotgun"].reserve
	own.global_position = pad.position
	match_._physics_process(0.016)
	assert_eq(own.arsenal["blaster"].reserve, before + Weapons.find("blaster").magazine)
	assert_eq(own.arsenal["shotgun"].reserve, shells + Weapons.find("shotgun").magazine)
	assert_eq(match_._rules_for(1).reserve(Weapons.find("blaster")), own.arsenal["blaster"].reserve, "the host counts the same")
	assert_false(match_.state.pickup_ready(world.pickup_pads().find(pad)))


func test_a_knock_out_gives_the_winner_a_little_ammunition() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	match_.state_changed.emit()
	var before: int = shooter.arsenal["blaster"].reserve
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", false)
	assert_eq(shooter.arsenal["blaster"].reserve, before + roundi(Weapons.find("blaster").magazine * Match.SPOILS))


func test_a_weapon_run_dry_gives_way_to_the_sidearm() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	assert_eq(own.weapon.stats.id, "blaster")
	own.weapon.ammo = 0
	own.weapon.reserve = 0
	own._fire_pressed = true
	await wait_physics_frames(2)
	assert_eq(own.weapon.stats.id, Weapons.STARTING)
	assert_true(own.arsenal.has("blaster"), "still carried, for when there is ammunition")


func test_a_thrown_grenade_bounces_then_bursts_and_hurts_who_is_near() -> void:
	var thrower := _add(1, Vector3(0, 0, 12))
	var other := _add(2, Vector3(-14, 0, 14))
	watch_signals(match_)
	var origin: Vector3 = thrower.head.global_position
	var aim := Vector3(0.25, 0.1, -1).normalized()
	match_._throw(1, origin, aim, 0.0)
	assert_eq(match_.state.player(1).grenades, Weapons.GRENADES - 1)
	assert_signal_emitted(match_, "projectile_launched")
	assert_eq(get_signal_parameters(match_, "projectile_launched")[1], Weapons.GRENADE)
	_fly(1.0)
	assert_signal_not_emitted(match_, "projectile_burst", "its fuse is still burning")
	_fly(Weapons.find(Weapons.GRENADE).fuse)
	assert_signal_emitted(match_, "projectile_burst")
	var at: Vector3 = get_signal_parameters(match_, "projectile_burst")[2]
	assert_lt(at.y, 1.5, "it came down to the floor")
	assert_lt(at.z, 4.0, "well ahead of where it was thrown from")
	assert_eq(match_.state.player(2).health, 100, "nobody was near it")
	# Thrown the same way again, it goes the same way: at someone standing there.
	other.global_position = Vector3(at.x, 0, at.z)
	match_._throw(1, origin, aim, 0.0)
	_fly(Weapons.find(Weapons.GRENADE).fuse + 0.5)
	assert_false(match_.state.player(2).alive, "a grenade at their feet")


func test_a_grenade_held_too_long_bursts_almost_at_once() -> void:
	var thrower := _add(1, Vector3(0, 0, 12))
	watch_signals(match_)
	match_._throw(1, thrower.head.global_position, Vector3.FORWARD, 999.0)
	_fly(Match.COOK_MARGIN + 0.1)
	assert_signal_emitted(match_, "projectile_burst")
	var at: Vector3 = get_signal_parameters(match_, "projectile_burst")[2]
	assert_lt(at.distance_to(thrower.head.global_position), 8.0, "barely out of the hand")


func test_nobody_throws_a_grenade_they_do_not_have() -> void:
	var thrower := _add(1, Vector3(0, 0, 12))
	watch_signals(match_)
	for throw in Weapons.GRENADES + 3:
		match_._throw(1, thrower.head.global_position, Vector3.FORWARD, 0.0)
	assert_signal_emit_count(match_, "projectile_launched", Weapons.GRENADES)


func test_a_grenade_reported_from_somewhere_else_is_ignored() -> void:
	_add(1, Vector3(0, 0, 12))
	watch_signals(match_)
	match_._throw(1, Vector3(0, 3, -8), Vector3.FORWARD, 0.0)
	assert_signal_not_emitted(match_, "projectile_launched")
	assert_eq(match_.state.player(1).grenades, Weapons.GRENADES)


func test_holding_the_key_cooks_a_grenade_and_letting_go_throws_it() -> void:
	var own := _add(1, Vector3(0, 0, 12))
	await _settle(own)
	watch_signals(match_)
	Input.action_press("grenade")
	own._start_grenade()
	await wait_seconds(0.4)
	assert_signal_not_emitted(match_, "projectile_launched", "held, not thrown")
	assert_gt(own._cooking, 0.3)
	var arc: MeshInstance3D = own.get_node_or_null("ThrowArc")
	assert_not_null(arc, "its path is shown")
	assert_true(arc.visible)
	Input.action_release("grenade")
	await wait_physics_frames(2)
	assert_signal_emitted(match_, "projectile_launched")
	assert_false(arc.visible)
	assert_lt(own._cooking, 0.0)
	# What it was held for has come off its fuse.
	var thrown: Projectile = match_._projectiles.values()[0]
	assert_gt(thrown.age, 0.3)


func test_a_grenade_held_to_the_end_of_its_fuse_is_thrown_anyway() -> void:
	var own := _add(1, Vector3(0, 0, 12))
	await _settle(own)
	watch_signals(match_)
	Input.action_press("grenade")
	own._start_grenade()
	own._cooking = Weapons.find(Weapons.GRENADE).fuse - Match.COOK_MARGIN - 0.02
	await wait_physics_frames(4)
	Input.action_release("grenade")
	assert_signal_emitted(match_, "projectile_launched")


func test_the_arc_shown_is_the_way_the_grenade_goes() -> void:
	var own := _add(1, Vector3(0, 0, 12))
	await _settle(own)
	var space := world.get_world_3d().direct_space_state
	var from: Vector3 = own.head.global_position
	var arc: PackedVector3Array = own.throw_arc(from, own.throw_direction(), Match.wall_finder(space))
	assert_gt(arc.size(), 5)
	assert_eq(arc[0], from)
	assert_gt(arc[3].y, from.y, "up at first")
	assert_lt(arc[arc.size() - 1].y, 0.3, "and down to where it first lands")
	for index in range(1, arc.size()):
		assert_lt(arc[index].z, arc[index - 1].z, "forward all the way")


func test_others_see_a_grenade_bounce_through_their_own_copy_of_the_arena() -> void:
	_add(1, Vector3(0, 0, 12))
	var grenade := Weapons.find(Weapons.GRENADE)
	world.effects.launch_projectile(77, Weapons.GRENADE, Vector3(0, 3, 10), Vector3(0, 0, -1) * grenade.projectile_speed)
	assert_eq(world.effects.projectiles_in_flight(), 1)
	for frame in 90:
		world.effects._process(1.0 / 60.0)
	var flying: Dictionary = world.effects._projectiles[77]
	assert_gte(flying.flight.bounces, 1)
	assert_gte(flying.node.global_position.y, 0.0, "it never sinks through the floor")
	world.effects.burst_projectile(77, Weapons.GRENADE, flying.node.global_position)
	assert_eq(world.effects.projectiles_in_flight(), 0)


## A solid sheet `thick` metres through, standing across the arena at z 0.
func _sheet(thick: float) -> void:
	var shape := BoxShape3D.new()
	shape.size = Vector3(8, 10, thick)
	var collider := CollisionShape3D.new()
	collider.shape = shape
	var sheet := StaticBody3D.new()
	sheet.add_child(collider)
	sheet.position = Vector3(0, 5, 0)
	world.add_child(sheet)
	await wait_physics_frames(2)


func test_a_shot_through_thin_cover_hurts_less() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	await _sheet(0.3)
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100 - roundi(20 * Penetration.DAMAGE_SHARE))


func test_thick_cover_still_stops_a_shot() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	_add(2, Vector3(0, 0, -5))
	await _sheet(1.0)
	_shoot(shooter, Vector3(0, CHEST, -5))
	assert_eq(match_.state.player(2).health, 100)


func test_the_shooter_sees_their_shot_carry_on_through_thin_cover() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	await _sheet(0.3)
	var origin: Vector3 = shooter.head.global_position
	shooter.fired.emit(origin, Vector3.FORWARD, 1, WeaponStats.AIMED, 0)
	var tracer: Node3D = world.effects.get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == "tracer")[0]
	# The tracer runs from the muzzle to where the shot ended: past the sheet,
	# at the arena's edge.
	var far_end: Vector3 = tracer.global_transform * Vector3(0, 0, -0.5)
	assert_lt(far_end.z, -10.0)


func test_holding_crouch_lowers_the_owner_and_slows_them() -> void:
	var own := _add(1, Vector3(0, 0, 5))
	await _settle(own)
	var standing: float = own.head.position.y
	Input.action_press("crouch")
	await wait_seconds(0.3)
	assert_eq(own.crouch(), 1.0)
	assert_almost_eq(own.head.position.y, BodyShape.head_height(1.0) * BodyShape.SIZE, 0.001, "seeing from lower down")
	assert_eq(own.stance() & WeaponStats.CROUCHED, WeaponStats.CROUCHED)
	Input.action_press("sprint")
	var speed: float = await _speed_of(own)
	Input.action_release("sprint")
	assert_almost_eq(speed, Movement.WALK_SPEED * Movement.CROUCH_FACTOR, 0.01, "and no running")
	own._publish()
	assert_eq(int(own.net_state[6]) & own.POSE_CROUCHING, own.POSE_CROUCHING, "everyone is told")
	Input.action_release("crouch")
	await wait_seconds(0.3)
	assert_eq(own.crouch(), 0.0)
	assert_almost_eq(own.head.position.y, standing, 0.001)


func test_a_shot_over_a_crouched_player_misses() -> void:
	var shooter := _add(1, Vector3(0, 0, 5))
	var other := _add(2, Vector3(0, 0, -5))
	var over := Vector3(0, (BodyShape.WEDGE_CENTRE.y + BodyShape.WEDGE_SIZE.y * 0.45) * BodyShape.SIZE, -5)
	# Level, so that the shot is as high where it passes them as where it is aimed.
	var origin := Vector3(0, over.y, 5)
	shooter.global_position = origin - Vector3.UP * shooter.head.position.y
	other.net_state = PackedFloat32Array([0, 0, -5, 0, 0, 0, other.POSE_CROUCHING])
	other._animate(1.0)
	assert_eq(other.crouch(), 1.0)
	_shoot(shooter, over)
	assert_eq(match_.state.player(2).health, 100, "it passes over their head")
	other.net_state = PackedFloat32Array([0, 0, -5, 0, 0, 0, 0])
	other._animate(1.0)
	match_._rules_for(1).refill()
	_shoot(shooter, over)
	assert_lt(match_.state.player(2).health, 100, "standing, it catches the tip")
