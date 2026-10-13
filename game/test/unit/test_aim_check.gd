extends GutTest

const WORLD := preload("res://world/world.tscn")
const AHEAD := Vector3(0, 0, -1)
const BEHIND := Vector3(0, 0, 1)


func test_facing_reads_as_the_way_a_player_looks() -> void:
	assert_almost_eq(AimCheck.forward(Vector2.ZERO), AHEAD, Vector3.ONE * 0.001)
	assert_almost_eq(AimCheck.forward(Vector2(PI / 2, 0)), Vector3(-1, 0, 0), Vector3.ONE * 0.001, "a quarter turn left")
	assert_almost_eq(AimCheck.forward(Vector2(0, PI / 2)), Vector3.UP, Vector3.ONE * 0.001, "looking up")
	# As a player's own head has it.
	var body := Node3D.new()
	var head := Node3D.new()
	body.add_child(head)
	add_child_autofree(body)
	body.rotation.y = 0.7
	head.rotation.x = -0.3
	assert_almost_eq(AimCheck.forward(Vector2(0.7, -0.3)), -head.global_basis.z, Vector3.ONE * 0.001)


func test_a_shot_where_the_player_faces_is_not_off() -> void:
	assert_almost_eq(AimCheck.off_by(AHEAD, [Vector2.ZERO]), 0.0, 0.001)
	assert_almost_eq(AimCheck.off_by(BEHIND, [Vector2.ZERO]), PI, 0.001)
	assert_almost_eq(AimCheck.off_by(BEHIND, [Vector2.ZERO, Vector2(PI, 0)]), 0.0, 0.001, "the nearest of the ways they faced counts")
	assert_eq(AimCheck.off_by(BEHIND, []), 0.0, "with nothing to go by, nothing is held against them")


func test_shots_with_recoil_and_spread_are_fine() -> void:
	var check := AimCheck.new()
	for i in 40:
		assert_true(check.allow(deg_to_rad(12.0)))
	assert_eq(check.share(), 0.0)


func test_one_quick_turn_and_shot_is_let_through() -> void:
	var check := AimCheck.new()
	for i in 10:
		check.allow(0.0)
	assert_true(check.allow(PI), "the turn may not have reached the host yet")
	assert_true(check.allow(0.0))


## The cheat: a game that fires at whoever is behind it without turning.
func test_firing_behind_without_turning_is_refused() -> void:
	var check := AimCheck.new()
	var allowed := 0
	for i in 30:
		if check.allow(PI):
			allowed += 1
	assert_lt(allowed, AimCheck.AT_LEAST, "only the first few, before there is anything to go by")
	assert_true(check.allow(0.1), "a shot where they face is still theirs to take")


## The same cheat, mixed in with honest shots so as to hide it.
func test_every_other_shot_fired_behind_is_refused() -> void:
	var check := AimCheck.new()
	var wild_allowed := 0
	for i in 40:
		check.allow(0.0)
		if check.allow(PI):
			wild_allowed += 1
	assert_lt(wild_allowed, 8, "most of the wild ones")


func test_a_cheat_is_forgiven_once_they_stop() -> void:
	var check := AimCheck.new()
	for i in 20:
		check.allow(PI)
	for i in AimCheck.REMEMBERED:
		check.allow(0.0)
	assert_true(check.allow(PI))


func test_history_gives_the_ways_a_player_faced_lately() -> void:
	var history := PositionHistory.new()
	history.push(1.0, Vector3.ZERO, Vector2(0.1, 0))
	history.push(1.2, Vector3.ZERO, Vector2(0.2, 0))
	history.push(1.4, Vector3.ZERO, Vector2(0.3, 0))
	history.push(1.6, Vector3.ZERO, Vector2(0.4, 0))
	assert_eq(history.facings_since(1.3), [Vector2(0.2, 0), Vector2(0.3, 0), Vector2(0.4, 0)], "and the one just before")
	assert_eq(history.facings_since(9.0), [Vector2(0.4, 0)], "the last, however old")
	assert_eq(PositionHistory.new().facings_since(0.0), [])


func test_the_host_refuses_shots_fired_behind_a_player_who_never_turns() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	var match_: Match = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	match_.add_player(2)
	var node: Node3D = world._create_player([2, Vector3.ZERO])
	world.players.add_child(node)
	for i in 12:
		match_.record_position(Match.now(), node.global_position, 2, Vector2.ZERO)
		match_._shot(2, node.head.global_position, BEHIND, i)
	assert_gt(match_.wild_shots, 0)
	var before := match_.wild_shots
	match_._shot(2, node.head.global_position, AHEAD, 99)
	assert_eq(match_.wild_shots, before, "a shot ahead is not held against them")
