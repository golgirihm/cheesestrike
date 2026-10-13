extends GutTest

const WORLD := preload("res://world/world.tscn")

var spots: Array[Vector3] = [Vector3(-10, 0, -10), Vector3(10, 0, 10), Vector3(-10, 0, 10), Vector3(10, 0, -10)]


# An enemy at `at`, facing the point `toward`.
func _enemy(at: Vector3, toward: Vector3) -> Dictionary:
	var way := toward - at
	return {"at": at, "yaw": atan2(-way.x, -way.z)}


func test_the_spot_farthest_from_the_only_other_player_is_chosen() -> void:
	var others: Array[Vector3] = [Vector3(9, 0, 9)]
	assert_eq(SpawnChooser.farthest(spots, others), 0)


func test_it_is_judged_by_the_nearest_other_player() -> void:
	# The corner at (10, -10) is far from one player but next to the other.
	var others: Array[Vector3] = [Vector3(-9, 0, -9), Vector3(9, 0, -8)]
	assert_eq(SpawnChooser.farthest(spots, others), 2)


func test_with_nobody_else_any_spot_will_do() -> void:
	var rng := RandomNumberGenerator.new()
	var seen := {}
	var best := {}
	for i in 50:
		rng.seed = i
		seen[SpawnChooser.farthest(spots, [], rng)] = true
		best[SpawnChooser.best(spots, [], [], [], Callable(), rng)] = true
	assert_eq(seen.size(), spots.size())
	assert_eq(best.size(), spots.size())


func test_no_spots_gives_none() -> void:
	assert_eq(SpawnChooser.farthest([], [Vector3.ZERO]), -1)
	assert_eq(SpawnChooser.best([], [_enemy(Vector3.ZERO, Vector3.FORWARD)], [], []), -1)


func test_with_nothing_else_to_go_by_the_best_spot_is_the_farthest_from_enemies() -> void:
	# Facing away from every spot, and too far to be watching any.
	var enemies := [_enemy(Vector3(9, 0, 9), Vector3(99, 0, 99))]
	assert_eq(SpawnChooser.best(spots, enemies, [], []), 0)
	var two := [_enemy(Vector3(-9, 0, -9), Vector3(-99, 0, -99)), _enemy(Vector3(9, 0, -8), Vector3(99, 0, -8))]
	assert_eq(SpawnChooser.best(spots, two, [], []), 2)


func test_a_spot_an_enemy_nearby_is_looking_at_is_watched() -> void:
	var spot := Vector3(0, 0, -10)
	assert_true(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, spot)]))
	assert_false(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, Vector3(0, 0, 10))]), "their back is to it")
	assert_false(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, Vector3(10, 0, 0))]), "it is off to their side")
	assert_true(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, Vector3(3, 0, -10))]), "near enough the middle of their view")
	var far := Vector3(0, 0, -SpawnChooser.WATCH_RANGE - 5.0)
	assert_false(SpawnChooser.watched(far, [_enemy(Vector3.ZERO, far)]), "too far off to matter")
	var wall := func(_from: Vector3, _to: Vector3) -> bool: return false
	assert_false(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, spot)], wall), "something is in the way")
	var open := func(_from: Vector3, _to: Vector3) -> bool: return true
	assert_true(SpawnChooser.watched(spot, [_enemy(Vector3.ZERO, spot)], open))
	assert_false(SpawnChooser.watched(spot, []))


func test_a_watched_spot_is_passed_over_for_one_a_little_nearer_but_unseen() -> void:
	# An enemy in the middle looks north: the far north spot is watched, the
	# nearer south one is behind them.
	var two: Array[Vector3] = [Vector3(0, 0, -18), Vector3(0, 0, 12)]
	var looking_north := [_enemy(Vector3.ZERO, Vector3(0, 0, -18))]
	assert_eq(SpawnChooser.best(two, looking_north, [], []), 1)
	var looking_east := [_enemy(Vector3.ZERO, Vector3(18, 0, 0))]
	assert_eq(SpawnChooser.best(two, looking_east, [], []), 0, "unwatched, the farther is better again")


func test_a_spot_where_there_was_just_fighting_is_passed_over() -> void:
	var two: Array[Vector3] = [Vector3(0, 0, -18), Vector3(0, 0, 14)]
	var enemies := [_enemy(Vector3.ZERO, Vector3(50, 0, 0))]
	assert_eq(SpawnChooser.best(two, enemies, [], []), 0)
	assert_eq(SpawnChooser.best(two, enemies, [], [Vector3(1, 0, -17)]), 1)
	assert_eq(SpawnChooser.best(two, enemies, [], [Vector3(0, 0, -18 - SpawnChooser.FIGHT_REACH - 1.0)]), 0, "fighting far enough off doesn't count")
	# The nearer the fighting, the more it counts.
	var near := SpawnChooser.score(two[0], enemies, [], [two[0]])
	var by := SpawnChooser.score(two[0], enemies, [], [two[0] + Vector3(5, 0, 0)])
	assert_lt(near, by)
	assert_lt(by, SpawnChooser.score(two[0], enemies, [], []))


func test_with_teams_a_spot_near_a_teammate_is_better() -> void:
	var two: Array[Vector3] = [Vector3(-12, 0, 0), Vector3(12, 0, 0)]
	var enemies := [_enemy(Vector3(0, 0, 40), Vector3(0, 0, 99))]
	assert_eq(SpawnChooser.score(two[0], enemies, [], []), SpawnChooser.score(two[1], enemies, [], []), "nothing between them otherwise")
	assert_eq(SpawnChooser.best(two, enemies, [Vector3(11, 0, 2)], []), 1)
	assert_eq(SpawnChooser.best(two, enemies, [Vector3(-11, 0, 2)], []), 0)


func test_being_near_a_teammate_never_outweighs_being_under_an_enemys_nose() -> void:
	var two: Array[Vector3] = [Vector3(-30, 0, 0), Vector3(12, 0, 0)]
	var enemies := [_enemy(Vector3(14, 0, 3), Vector3(99, 0, 3))]
	assert_eq(SpawnChooser.best(two, enemies, [Vector3(12, 0, 2)], []), 0)


func test_further_than_far_enough_is_no_better() -> void:
	var enemies := [_enemy(Vector3.ZERO, Vector3(0, 0, 99))]
	var far := SpawnChooser.score(Vector3(0, 0, -SpawnChooser.FAR_ENOUGH), enemies, [], [])
	assert_eq(SpawnChooser.score(Vector3(0, 0, -SpawnChooser.FAR_ENOUGH * 3.0), enemies, [], []), far)


func test_each_team_starts_on_the_half_of_the_spots_nearer_its_base() -> void:
	var bases := [Vector3(-20, 0, 0), Vector3(20, 0, 0)]
	var all: Array[Vector3] = [Vector3(-12, 0, 5), Vector3(12, 0, 5), Vector3(-18, 0, 0), Vector3(18, 0, 0), Vector3(-1, 0, 9), Vector3(1, 0, -9)]
	var sides := SpawnChooser.by_side(all, bases)
	assert_eq(sides[0].size(), 3)
	assert_eq(sides[1].size(), 3)
	for spot: Vector3 in sides[0]:
		assert_lt(spot.x, 0.0)
	for spot: Vector3 in sides[1]:
		assert_gt(spot.x, 0.0)
	assert_eq(sides[0][0], Vector3(-18, 0, 0), "the nearest to the base first")
	# All on one side of the arena: still shared out evenly.
	var lopsided: Array[Vector3] = [Vector3(-12, 0, 5), Vector3(-14, 0, 5), Vector3(-18, 0, 0), Vector3(-10, 0, 0)]
	sides = SpawnChooser.by_side(lopsided, bases)
	assert_eq(sides[0].size(), 2)
	assert_eq(sides[1].size(), 2)


# --- The host --------------------------------------------------------------

func _start(mode: String) -> Node3D:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)
	return world


func _add(world: Node3D, id: int, at: Vector3) -> Node3D:
	world.match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


func test_the_host_keeps_a_returning_player_away_from_other_peoples_fights_but_not_their_own() -> void:
	var world: Node3D = await _start("ffa")
	var match_: Match = world.match_
	var own: Node3D = world.player_node(1)
	own.global_position = Vector3(-12, 0.1, -12)
	own.rotation.y = 0.0
	_add(world, 5, Vector3(12, 0.1, 12))
	# Their own knock-out, where they stood: no reason not to come back
	# there, with the one who did it far off.
	match_.state.player(5).protected_for = 0.0
	match_._harm(1, 5, 500.0, Vector3.ZERO, "blaster", false)
	assert_eq(match_._fights.size(), 1)
	# Where the host sends them (their own machine would move them there).
	match_._respawn(5)
	assert_almost_eq(match_._sent_to[5], Vector3(12, 0.1, 12), Vector3.ONE * 0.5, "the farthest spot, as ever")
	# Someone else's, right there: somewhere else is better.
	match_._fights.append({"where": Vector3(12, 0, 12), "at": match_._clock, "victim": 99})
	match_._respawn(5)
	assert_gt(match_._sent_to[5].distance_to(Vector3(12, 0.1, 12)), 3.0)
	# Old fighting is forgotten.
	match_._clock += Match.FIGHT_MEMORY + 1.0
	match_._respawn(5)
	assert_almost_eq(match_._sent_to[5], Vector3(12, 0.1, 12), Vector3.ONE * 0.5)


func test_a_new_match_forgets_the_fighting_of_the_last() -> void:
	var world: Node3D = await _start("ffa")
	world.match_._fights.append({"where": Vector3.ZERO, "at": 0.0, "victim": 9})
	world.match_._new_match()
	assert_eq(world.match_._fights.size(), 0)


func test_in_capture_each_team_starts_on_its_own_side() -> void:
	var world: Node3D = await _start("ctf")
	var match_: Match = world.match_
	for id: int in [5, 6, 7]:
		_add(world, id, Vector3.ZERO)
	match_._new_match()
	var bases: Array = world.objective_spots().bases
	for id: int in [1, 5, 6, 7]:
		var team: int = match_.state.player(id).team
		var at: Vector3 = world.player_node(id).global_position if id == 1 else match_._sent_to[id]
		assert_lt(at.distance_to(bases[team]), at.distance_to(bases[1 - team]), "%d starts nearer their own base" % id)
