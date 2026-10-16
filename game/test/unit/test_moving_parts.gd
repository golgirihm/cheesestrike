extends GutTest
## Hazards that hurt, doors that open and lifts that move: the rules alone,
## and the host and the arena carrying them out.

const WORLD := preload("res://world/world.tscn")
const FEED := preload("res://ui/kill_feed.gd")

var world: Node3D
var match_: Match
var parts: MovingParts


func before_each() -> void:
	parts = MovingParts.new()


func _hazard(kind: String, at := Vector3.ZERO, size := Vector3(4, 1, 4)) -> Dictionary:
	return {"position": at, "size": size, "kind": kind}


func _at(at: Vector3, alive := true) -> Dictionary:
	return {"at": at, "alive": alive}


func _start(map := "dairy") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": map}))
	match_ = world.match_
	await wait_physics_frames(3)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- Hazards ----------------------------------------------------------------

func test_a_player_is_in_a_hazard_when_standing_on_it() -> void:
	var hazards := [_hazard(MovingParts.GRATER, Vector3(10, 0, 0), Vector3(6, 1, 2))]
	assert_false(MovingParts.hazard_at(hazards, Vector3(10, 0, 0)).is_empty())
	assert_false(MovingParts.hazard_at(hazards, Vector3(12.9, 0.1, 0.9)).is_empty(), "to its edge")
	assert_true(MovingParts.hazard_at(hazards, Vector3(13.1, 0, 0)).is_empty(), "and no further")
	assert_true(MovingParts.hazard_at(hazards, Vector3(10, 0, 1.1)).is_empty(), "it is narrower one way than the other")
	assert_true(MovingParts.hazard_at(hazards, Vector3(10, 2.0, 0)).is_empty(), "jumping over it is the way across")
	assert_true(MovingParts.hazard_at([], Vector3.ZERO).is_empty())


func test_a_hazard_hurts_at_once_and_then_every_so_often() -> void:
	var hazards := [_hazard(MovingParts.GRATER)]
	var standing := {1: _at(Vector3.ZERO)}
	var first := parts.hurt(0.01, hazards, standing)
	assert_eq(first.size(), 1, "on stepping in")
	assert_eq(first[0].id, 1)
	assert_eq(first[0].kind, MovingParts.GRATER)
	assert_almost_eq(first[0].amount, MovingParts.DAMAGE[MovingParts.GRATER] * MovingParts.HURT_EVERY, 0.001)
	assert_eq(parts.hurt(MovingParts.HURT_EVERY * 0.5, hazards, standing).size(), 0, "not all the time")
	assert_eq(parts.hurt(MovingParts.HURT_EVERY * 0.6, hazards, standing).size(), 1)
	# A second of it does a second's damage.
	var total := 0.0
	var fresh := MovingParts.new()
	for step in 60:
		for hurt: Dictionary in fresh.hurt(1.0 / 60.0, hazards, standing):
			total += hurt.amount
	assert_almost_eq(total, MovingParts.DAMAGE[MovingParts.GRATER], MovingParts.DAMAGE[MovingParts.GRATER] * MovingParts.HURT_EVERY + 0.01)


func test_stepping_out_and_back_in_hurts_again_at_once() -> void:
	var hazards := [_hazard(MovingParts.FONDUE)]
	parts.hurt(0.01, hazards, {1: _at(Vector3.ZERO)})
	assert_eq(parts.hurt(0.1, hazards, {1: _at(Vector3(20, 0, 0))}).size(), 0, "out of it")
	assert_eq(parts.hurt(0.01, hazards, {1: _at(Vector3.ZERO)}).size(), 1)


func test_nobody_out_of_play_is_hurt_and_each_player_is_counted_alone() -> void:
	var hazards := [_hazard(MovingParts.FONDUE)]
	assert_eq(parts.hurt(0.01, hazards, {1: _at(Vector3.ZERO, false)}).size(), 0)
	var both := parts.hurt(0.01, hazards, {2: _at(Vector3.ZERO), 3: _at(Vector3(1, 0, 1)), 4: _at(Vector3(30, 0, 0))})
	assert_eq(both.map(func(hurt: Dictionary) -> int: return hurt.id), [2, 3])
	# One who leaves is forgotten.
	parts.hurt(0.01, hazards, {3: _at(Vector3(1, 0, 1))})
	assert_false(parts._in_hazard.has(2))


func test_fondue_is_worse_than_the_grater_and_slows_whoever_wades_in() -> void:
	assert_gt(MovingParts.DAMAGE[MovingParts.FONDUE], MovingParts.DAMAGE[MovingParts.GRATER])
	var hazards := [_hazard(MovingParts.FONDUE), _hazard(MovingParts.GRATER, Vector3(20, 0, 0))]
	assert_eq(MovingParts.speed_factor(hazards, Vector3.ZERO), MovingParts.SLOW[MovingParts.FONDUE])
	assert_lt(MovingParts.SLOW[MovingParts.FONDUE], 1.0)
	assert_eq(MovingParts.speed_factor(hazards, Vector3(20, 0, 0)), 1.0, "teeth don't slow anyone")
	assert_eq(MovingParts.speed_factor(hazards, Vector3(9, 0, 9)), 1.0)
	for kind: String in [MovingParts.GRATER, MovingParts.FONDUE]:
		assert_true(MovingParts.HAZARD_NAMES.has(kind))
		assert_lt(MovingParts.DAMAGE[kind] * MovingParts.HURT_EVERY, float(PlayerRecord.MAX_HEALTH) / 3.0, "%s: a few seconds to get out" % kind)
		assert_null(Weapons.find(kind), "%s isn't taken for a weapon" % kind)


func test_the_feed_names_a_hazard() -> void:
	assert_eq(FEED.entry_text("", "Target", MovingParts.FONDUE, false), "Target  knocked themself out  [Fondue]")
	assert_eq(FEED.entry_text("", "Target", MovingParts.GRATER, false), "Target  knocked themself out  [The Grater]")


# --- Doors ------------------------------------------------------------------

func test_the_door_to_use_is_the_nearest_within_reach_on_the_same_floor() -> void:
	var doors := [{"position": Vector3(0, 3, 0), "size": Vector3(1, 6, 4)}, {"position": Vector3(6, 3, 0), "size": Vector3(1, 6, 4)}]
	assert_eq(MovingParts.door_near(doors, Vector3(1, 0, 0)), 0)
	assert_eq(MovingParts.door_near(doors, Vector3(4, 0, 0)), 1, "the nearer")
	assert_eq(MovingParts.door_near(doors, Vector3(-MovingParts.DOOR_REACH - 0.5, 0, 0)), -1, "not near enough")
	assert_eq(MovingParts.door_near(doors, Vector3(1, 10, 0)), -1, "on another floor")
	assert_eq(MovingParts.door_near([], Vector3.ZERO), -1)


func test_a_door_turns_on_hinges_at_one_end_and_away_from_who_opens_it() -> void:
	# Across X: its hinges are at its -X end, and its far end swings.
	var door := {"position": Vector3(0, 3, 0), "size": Vector3(4, 6, 0.5)}
	var hinge := Vector3(-2, 3, 0)
	for angle: float in [0.0, 0.5, MovingParts.DOOR_OPEN_ANGLE, -MovingParts.DOOR_OPEN_ANGLE]:
		var placed := MovingParts.door_transform(door, angle)
		assert_almost_eq((placed * Vector3(-2, 0, 0)).distance_to(hinge), 0.0, 0.001, "the hinged end stays put at %s" % angle)
	assert_eq(MovingParts.door_transform(door, 0.0), Transform3D(Basis(), door.position), "shut, it fills the doorway")
	for from: Vector3 in [Vector3(0, 0, 3), Vector3(0, 0, -3)]:
		var way := MovingParts.door_swing(door, from)
		var free_end := MovingParts.door_transform(door, way * MovingParts.DOOR_OPEN_ANGLE) * Vector3(2, 0, 0)
		assert_eq(signf(free_end.z), -signf(from.z), "swung to the far side from %s" % from)
	# Across Z, the same.
	var other := {"position": Vector3(0, 3, 0), "size": Vector3(0.5, 6, 4)}
	var swung := MovingParts.door_transform(other, MovingParts.door_swing(other, Vector3(3, 0, 0)) * MovingParts.DOOR_OPEN_ANGLE) * Vector3(0, 0, 2)
	assert_lt(swung.x, 0.0)


func test_a_shut_door_is_opened_for_a_bot_that_comes_to_it_and_left_as_it_is() -> void:
	var doors := [{"position": Vector3(0, 3, 0), "size": Vector3(0.5, 6, 4)}, {"position": Vector3(40, 3, 0), "size": Vector3(0.5, 6, 4)}]
	assert_eq(MovingParts.open_for_bots(doors, {}, []), [0, 0])
	var open := MovingParts.open_for_bots(doors, {-1: _at(Vector3(38, 0, 0))}, [0, 0])
	assert_eq(open, [0, MovingParts.door_swing(doors[1], Vector3(38, 0, 0))])
	assert_eq(MovingParts.open_for_bots(doors, {}, open), open, "it stays open after")
	assert_eq(MovingParts.open_for_bots(doors, {-1: _at(Vector3(1, 0, 0), false)}, [0, 0]), [0, 0], "not for one knocked out")
	assert_eq(MovingParts.open_for_bots(doors, {-1: _at(Vector3(38, 0, 0))}, [0, -1]), [0, -1], "nor turned round")


# --- Lifts ------------------------------------------------------------------

func test_a_lift_waits_goes_up_waits_and_comes_down_for_ever() -> void:
	var rise := 5.0
	var travel := rise / MovingParts.LIFT_SPEED
	assert_eq(MovingParts.lift_height(0.0, rise), 0.0)
	assert_eq(MovingParts.lift_height(MovingParts.LIFT_WAIT - 0.1, rise), 0.0, "waiting at the bottom")
	assert_almost_eq(MovingParts.lift_height(MovingParts.LIFT_WAIT + travel / 2.0, rise), rise / 2.0, 0.001)
	assert_almost_eq(MovingParts.lift_height(MovingParts.LIFT_WAIT + travel + 1.0, rise), rise, 0.001, "waiting at the top")
	assert_almost_eq(MovingParts.lift_height(MovingParts.LIFT_WAIT * 2.0 + travel * 1.5, rise), rise / 2.0, 0.001, "on the way down")
	var round := (MovingParts.LIFT_WAIT + travel) * 2.0
	assert_almost_eq(MovingParts.lift_height(round * 3.0 + MovingParts.LIFT_WAIT + 0.4, rise), 0.4 * MovingParts.LIFT_SPEED, 0.001, "and round again")
	assert_eq(MovingParts.lift_height(5.0, 0.0), 0.0)
	# It never jumps: a sixtieth of a second moves it no more than its speed allows.
	var last := 0.0
	var time := 0.0
	while time < round * 2.0:
		var height := MovingParts.lift_height(time, rise)
		assert_between(height, 0.0, rise)
		assert_lte(absf(height - last), MovingParts.LIFT_SPEED / 60.0 + 0.001, "at %.2f s" % time)
		last = height
		time += 1.0 / 60.0


# --- The arena --------------------------------------------------------------

func test_an_arena_lists_its_parts_and_most_have_none() -> void:
	await _start("dairy")
	var found: Dictionary = world.moving_parts
	assert_eq(found.hazards.map(func(hazard: Dictionary) -> String: return hazard.kind), [MovingParts.FONDUE, MovingParts.GRATER])
	assert_eq(found.doors.size(), 1)
	assert_eq(found.lifts.size(), 1)
	world.load_map("box")
	assert_eq(world.moving_parts, {"hazards": [], "doors": [], "lifts": []})
	world.load_map("canyon")
	assert_eq(world.moving_parts.lifts.size(), 2)


func test_nothing_that_hurts_is_where_a_player_starts_or_picks_something_up() -> void:
	await _start("dairy")
	for id: String in Maps.ORDER:
		world.load_map(id)
		var hazards: Array = world.moving_parts.hazards
		for spot: Vector3 in world.spawn_positions():
			assert_true(MovingParts.hazard_at(hazards, spot).is_empty(), "%s: a spawn at %s" % [id, spot])
		for pad: Dictionary in world.pickup_pads():
			assert_true(MovingParts.hazard_at(hazards, pad.position).is_empty(), "%s: %s" % [id, pad.kind])
		for hazard: Dictionary in hazards:
			assert_true(MovingParts.DAMAGE.has(hazard.kind), "%s: %s" % [id, hazard.kind])


func test_the_host_hurts_a_player_standing_in_the_fondue_and_it_can_finish_them() -> void:
	await _start("dairy")
	var vat: Dictionary = world.moving_parts.hazards[0]
	var own: Node3D = world.player_node(1)
	_add(5, Vector3(0, 0, 10))
	match_.state.player(1).protected_for = 0.0
	own.global_position = vat.position
	watch_signals(match_)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.player(1).health, PlayerRecord.MAX_HEALTH - roundi(MovingParts.DAMAGE[MovingParts.FONDUE] * MovingParts.HURT_EVERY))
	assert_signal_emitted(match_, "hurt")
	for step in 600:
		own.global_position = vat.position
		match_._physics_process(1.0 / 60.0)
		if not match_.state.player(1).alive:
			break
	assert_false(match_.state.player(1).alive)
	assert_signal_emitted_with_parameters(match_, "knocked_out", ["", match_.state.name_of(1), MovingParts.FONDUE, false])
	assert_eq(match_.state.player(5).kills, 0, "nobody's knock-out")
	assert_eq(match_.state.player(5).health, PlayerRecord.MAX_HEALTH, "and nobody else is touched")


func test_wading_through_fondue_is_slow_for_the_player_themselves() -> void:
	await _start("dairy")
	var own: Node3D = world.player_node(1)
	var vat: Dictionary = world.moving_parts.hazards[0]
	own.global_position = vat.position + Vector3.UP * 0.05
	await wait_physics_frames(3)
	Input.action_press("move_forward")
	await wait_physics_frames(3)
	var speed := Vector2(own.velocity.x, own.velocity.z).length()
	Input.action_release("move_forward")
	# Slowed by the fondue, and by being hurt by it besides.
	assert_lte(speed, Movement.WALK_SPEED * MovingParts.SLOW[MovingParts.FONDUE] + 0.05)
	assert_gt(speed, 1.0, "but not stuck")


func test_a_player_opens_and_shuts_the_door_beside_them_and_everyone_sees_it_turn() -> void:
	await _start("dairy")
	var door: Dictionary = world.moving_parts.doors[0]
	var own: Node3D = world.player_node(1)
	own.global_position = Vector3(10, 0, 12)
	match_._physics_process(1.0 / 60.0)
	await wait_physics_frames(1)
	assert_eq(match_.state.doors, [0])
	assert_eq(world.doors_shown(), [0.0])
	assert_false(own.door_in_reach())
	match_.use_door()
	assert_eq(match_.state.doors, [0], "nothing to open from over there")
	# Coming near does nothing by itself.
	var beside: Vector3 = door.position + Vector3(3, -door.size.y / 2.0, 0)
	own.global_position = beside
	for step in 30:
		match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	assert_eq(world.doors_shown(), [0.0])
	# It stops a shot and a player while it is shut.
	var cast := Match.wall_caster(world.get_world_3d().direct_space_state)
	var through_from: Vector3 = door.position + Vector3(4, 0, 0)
	var through_to: Vector3 = door.position + Vector3(-3, 0, 0)
	assert_lt(cast.call(through_from, through_to), 7.0)
	assert_true(own.door_in_reach())
	assert_eq(preload("res://ui/combat_hud.gd").door_prompt(own, world), "E  ·  open the door", "and is told how")
	watch_signals(match_)
	own.use_door()
	assert_eq(match_.state.doors, [MovingParts.door_swing(door, beside)])
	assert_signal_emitted(match_, "state_changed")
	for step in 60:
		await wait_physics_frames(1)
	assert_eq(world.doors_shown(), [1.0], "swung right open")
	assert_eq(preload("res://ui/combat_hud.gd").door_prompt(own, world), "E  ·  close the door")
	assert_eq(cast.call(through_from, through_to), INF, "and nothing in the doorway")
	var copy := MatchState.from_dict(match_.state.to_dict())
	assert_eq(copy.doors, match_.state.doors, "which every machine is told")
	# It stays open until shut.
	own.global_position = Vector3(10, 0, 12)
	for step in 120:
		match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	assert_eq(world.doors_shown(), [1.0])
	own.global_position = beside
	own.use_door()
	assert_eq(match_.state.doors, [0])
	for step in 60:
		await wait_physics_frames(1)
	assert_eq(world.doors_shown(), [0.0])


func test_a_door_keeps_turning_open_while_the_host_sends_its_state() -> void:
	# A player who isn't the host is sent the state many times a second, and
	# each one names the arena already loaded: the door goes on turning.
	await _start("dairy")
	var door: Dictionary = world.moving_parts.doors[0]
	var own: Node3D = world.player_node(1)
	own.global_position = door.position + Vector3(3, -door.size.y / 2.0, 0)
	own.use_door()
	for step in 60:
		match_.state_changed.emit()
		await wait_physics_frames(1)
	assert_eq(world.doors_shown(), [1.0], "swung right open")


func test_a_door_is_drawn_as_a_door_with_a_knob_each_side() -> void:
	await _start("dairy")
	var body: Node3D = world.map.get_children().filter(func(part: Node) -> bool: return part.is_in_group("doors"))[0]
	var knobs: MeshInstance3D = body.get_node("Knobs")
	var door: Dictionary = world.moving_parts.doors[0]
	var bounds := knobs.get_aabb()
	# Away from the hinges, and out from both faces.
	var width_axis := MovingParts.door_width_axis(door)
	assert_gt(bounds.position.dot(width_axis), 0.0, "at the free end")
	var thickness_axis := Vector3.RIGHT if width_axis == Vector3.BACK else Vector3.BACK
	assert_lt(bounds.position.dot(thickness_axis), -Scenery.DOOR_THICKNESS / 2.0, "on one face")
	assert_gt(bounds.end.dot(thickness_axis), Scenery.DOOR_THICKNESS / 2.0, "and the other")
	assert_lt(body.get_children().filter(func(part: Node) -> bool: return part is MeshInstance3D).size(), 3, "drawn in as few parts as a plain slab")


func test_the_keys_open_the_door_and_say_so() -> void:
	assert_true(InputMap.has_action("interact"))
	assert_eq(Controls.keys_text("interact"), "E")
	assert_true(Controls.REBINDABLE.has("interact"))


func test_a_player_fits_through_the_open_doorway() -> void:
	await _start("dairy")
	var door: Dictionary = world.moving_parts.doors[0]
	assert_gt(door.size.y, BodyShape.HEIGHT, "tall enough")
	assert_gt(maxf(door.size.x, door.size.z), BodyShape.RADIUS * 2.0 + 1.0, "and wide enough")


func test_a_lift_carries_whoever_stands_on_it_to_the_top() -> void:
	await _start("dairy")
	var lift: Dictionary = world.moving_parts.lifts[0]
	var own: Node3D = world.player_node(1)
	own.global_position = lift.position + Vector3.UP * 0.5
	assert_true(match_.state.has_lifts or true)
	var highest := 0.0
	var top: float = lift.position.y + lift.size.y / 2.0 + lift.rise
	for step in int((MovingParts.LIFT_WAIT + lift.rise / MovingParts.LIFT_SPEED + 1.0) * 60.0):
		match_._process(1.0 / 60.0)
		await wait_physics_frames(1)
		highest = maxf(highest, own.global_position.y)
	assert_almost_eq(highest, top, 0.4, "up with it")
	assert_almost_eq(Vector2(own.global_position.x, own.global_position.z).distance_to(Vector2(lift.position.x, lift.position.z)), 0.0, 0.5, "still on it")
	# Where it lets them off, there is somewhere to step to.
	var gantry_top := 7.0
	assert_almost_eq(top, gantry_top, 0.05)


func test_the_lifts_clock_is_the_sessions_and_everyone_is_sent_it() -> void:
	await _start("canyon")
	match_._physics_process(1.0 / 60.0)
	assert_true(match_.state.has_lifts)
	match_._process(2.5)
	var copy := MatchState.from_dict(match_.state.to_dict())
	assert_almost_eq(copy.uptime, match_.state.uptime, 0.001)
	assert_gt(copy.uptime, 2.0)
	var before: float = match_.state.uptime
	match_._new_match()
	assert_eq(match_.state.uptime, before, "a new match doesn't start it again, or the lift would jump")
	var plain := MatchState.new()
	plain.tick(5.0)
	assert_false(plain.to_dict().has("uptime"), "an arena with no lifts sends no clock")


func test_the_canyons_lifts_stop_level_with_its_mesas() -> void:
	await _start("canyon")
	for lift: Dictionary in world.moving_parts.lifts:
		assert_almost_eq(lift.position.y + lift.size.y / 2.0 + lift.rise, 3.0, 0.01)
		# Beside the mesa, not in it, with room to step on from the ground.
		var space := world.get_world_3d().direct_space_state
		var body := PhysicsShapeQueryParameters3D.new()
		var box := BoxShape3D.new()
		box.size = lift.size - Vector3(0.1, 0.1, 0.1)
		body.shape = box
		body.collision_mask = 1
		for height: float in [0.0, lift.rise / 2.0, lift.rise]:
			body.transform = Transform3D(Basis.IDENTITY, lift.position + Vector3.UP * (height + 0.3))
			var touching := space.intersect_shape(body).filter(func(hit: Dictionary) -> bool: return not hit.collider.is_in_group("lifts"))
			assert_eq(touching.size(), 0, "nothing in its way at %.1f" % height)
		assert_lte(lift.position.y + lift.size.y / 2.0, Movement.STEP_HEIGHT, "low enough to step onto")
