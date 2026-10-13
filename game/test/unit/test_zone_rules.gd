extends GutTest
## Hold the hill and Three points, as rules alone.

const SPOTS := [Vector3(-10, 0, 0), Vector3(0, 0, 0), Vector3(10, 0, 0)]
const CHEDDAR := MatchState.CHEDDAR
const BRIE := MatchState.BRIE

var rules: ZoneRules


func before_each() -> void:
	rules = ZoneRules.new()


func _at(at: Vector3, team: int, alive := true) -> Dictionary:
	return {"at": at, "team": team, "alive": alive}


func test_who_holds_a_zone() -> void:
	var spot: Vector3 = SPOTS[1]
	assert_eq(ZoneRules.holder(spot, {}), ZoneRules.NOBODY)
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3(1, 0, 1), CHEDDAR)}), CHEDDAR)
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3(1, 0, 1), CHEDDAR), 2: _at(Vector3(-1, 0, 0), CHEDDAR)}), CHEDDAR, "two of a team")
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3(1, 0, 1), CHEDDAR), 2: _at(Vector3(-1, 0, 0), BRIE)}), ZoneRules.CONTESTED)
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3(ZoneRules.RADIUS + 0.5, 0, 0), BRIE)}), ZoneRules.NOBODY, "outside it")
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3(0, ZoneRules.HEIGHT + 1.0, 0), BRIE)}), ZoneRules.NOBODY, "on a floor above")
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3.ZERO, BRIE, false)}), ZoneRules.NOBODY, "knocked out")
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3.ZERO, MatchState.NO_TEAM)}), ZoneRules.NOBODY, "on no team")
	assert_eq(ZoneRules.holder(spot, {1: _at(Vector3.ZERO, BRIE), 2: _at(Vector3.ZERO, CHEDDAR, false)}), BRIE, "the knocked out don't contest it")


func test_the_team_alone_on_the_hill_scores_each_second() -> void:
	var standing := {1: _at(SPOTS[0], BRIE)}
	var total := 0
	for step in 50:
		var result := rules.update_hill(0.1, SPOTS, standing)
		total += result.points[BRIE]
		assert_eq(result.points[CHEDDAR], 0)
	assert_between(total, 4, 5, "about five seconds' worth, in whole points")


func test_nobody_scores_on_a_contested_or_empty_hill() -> void:
	for standing: Dictionary in [{}, {1: _at(SPOTS[0], BRIE), 2: _at(SPOTS[0], CHEDDAR)}, {1: _at(SPOTS[1], BRIE)}]:
		rules = ZoneRules.new()
		for step in 30:
			assert_eq(rules.update_hill(0.1, SPOTS, standing).points, [0, 0])


func test_the_hill_moves_on_in_turn_and_comes_round() -> void:
	assert_eq(rules.hill, 0)
	assert_false(rules.update_hill(ZoneRules.HILL_TIME - 1.0, SPOTS, {}).moved)
	assert_true(rules.update_hill(1.5, SPOTS, {}).moved)
	assert_eq(rules.hill, 1)
	assert_almost_eq(rules.hill_left, ZoneRules.HILL_TIME, 0.001)
	rules.update_hill(ZoneRules.HILL_TIME, SPOTS, {})
	rules.update_hill(ZoneRules.HILL_TIME, SPOTS, {})
	assert_eq(rules.hill, 0, "round again")
	# Standing where it was scores nothing once it has gone.
	rules.update_hill(ZoneRules.HILL_TIME, SPOTS, {})
	assert_eq(rules.update_hill(3.0, SPOTS, {1: _at(SPOTS[0], BRIE)}).points, [0, 0])


func test_a_hill_with_one_place_stays_and_one_with_none_does_nothing() -> void:
	assert_false(rules.update_hill(ZoneRules.HILL_TIME + 1.0, [SPOTS[0]], {}).moved)
	assert_eq(rules.hill, 0)
	assert_eq(rules.update_hill(5.0, [], {1: _at(Vector3.ZERO, BRIE)}).points, [0, 0])


func test_a_point_is_taken_by_standing_in_it_alone_for_a_while() -> void:
	var standing := {1: _at(SPOTS[2], CHEDDAR)}
	var result := rules.update_points(ZoneRules.TAKE_TIME * 0.5, SPOTS, standing)
	assert_eq(result.taken, [])
	assert_eq(rules.owners[2], ZoneRules.NOBODY)
	assert_eq(rules.taking[2], CHEDDAR)
	assert_almost_eq(rules.progress[2], 0.5, 0.001)
	result = rules.update_points(ZoneRules.TAKE_TIME * 0.6, SPOTS, standing)
	assert_eq(result.taken, [[2, CHEDDAR]])
	assert_eq(rules.owners[2], CHEDDAR)
	assert_eq(rules.progress[2], 0.0)


func test_taking_stops_while_contested_and_comes_undone_when_left() -> void:
	var one := {1: _at(SPOTS[0], BRIE)}
	rules.update_points(ZoneRules.TAKE_TIME * 0.5, SPOTS, one)
	var both := {1: _at(SPOTS[0], BRIE), 2: _at(SPOTS[0], CHEDDAR)}
	rules.update_points(ZoneRules.TAKE_TIME, SPOTS, both)
	assert_almost_eq(rules.progress[0], 0.5, 0.001, "held where it was")
	rules.update_points(ZoneRules.TAKE_TIME * 0.25, SPOTS, {})
	assert_almost_eq(rules.progress[0], 0.25, 0.001, "and slipping back")
	rules.update_points(ZoneRules.TAKE_TIME, SPOTS, {})
	assert_eq(rules.progress[0], 0.0)
	assert_eq(rules.taking[0], ZoneRules.NOBODY)
	assert_eq(rules.owners[0], ZoneRules.NOBODY)


func test_the_other_team_starts_from_nothing() -> void:
	rules.update_points(ZoneRules.TAKE_TIME * 0.75, SPOTS, {1: _at(SPOTS[0], BRIE)})
	rules.update_points(ZoneRules.TAKE_TIME * 0.5, SPOTS, {2: _at(SPOTS[0], CHEDDAR)})
	assert_eq(rules.taking[0], CHEDDAR)
	assert_almost_eq(rules.progress[0], 0.5, 0.001)


func test_a_point_stays_its_owners_when_they_walk_away_and_keeps_scoring() -> void:
	rules.update_points(ZoneRules.TAKE_TIME + 0.1, SPOTS, {1: _at(SPOTS[1], BRIE)})
	var total := 0
	for step in 100:
		total += rules.update_points(0.1, SPOTS, {}).points[BRIE]
	assert_eq(rules.owners[1], BRIE)
	assert_between(total, 4, 6, "ten seconds at half a point a second")


func test_more_points_score_faster_and_each_team_for_its_own() -> void:
	rules.owners = [CHEDDAR, CHEDDAR, BRIE] as Array[int]
	var totals := [0, 0]
	for step in 200:
		var points: Array = rules.update_points(0.1, SPOTS, {}).points
		totals[0] += points[0]
		totals[1] += points[1]
	assert_almost_eq(float(totals[CHEDDAR]), 20.0, 1.0)
	assert_almost_eq(float(totals[BRIE]), 10.0, 1.0)


func test_an_owner_standing_in_their_own_point_changes_nothing() -> void:
	rules.owners[0] = BRIE
	var result := rules.update_points(ZoneRules.TAKE_TIME * 2.0, SPOTS, {1: _at(SPOTS[0], BRIE)})
	assert_eq(result.taken, [])
	assert_eq(rules.progress[0], 0.0)


func test_the_points_say_whose_they_are() -> void:
	assert_eq(rules.points_text(), "A – · B – · C –")
	rules.owners = [CHEDDAR, ZoneRules.NOBODY, BRIE] as Array[int]
	assert_eq(rules.points_text(), "A Cheddar · B – · C Brie")


func test_the_zones_survive_the_trip_and_nonsense_does_not() -> void:
	rules.owners = [CHEDDAR, ZoneRules.NOBODY, BRIE] as Array[int]
	rules.taking[1] = BRIE
	rules.progress[1] = 0.4
	rules.hill = 2
	rules.hill_left = 12.5
	var copy := ZoneRules.from_array(rules.to_array())
	assert_eq(copy.owners, rules.owners)
	assert_eq(copy.taking, rules.taking)
	assert_almost_eq(copy.progress[1], 0.4, 0.001)
	assert_eq(copy.hill, 2)
	assert_almost_eq(copy.hill_left, 12.5, 0.001)
	for odd: Variant in [null, 3, [], [0, 1.0, [0], [0, 1], [0.0]], ["a", 1.0, [], [], []], [0, 1.0, [0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0]]]:
		var made := ZoneRules.from_array(odd)
		assert_eq(made.owners.size(), 3, str(odd))
	var wild := ZoneRules.from_array([-4, 9999.0, [7, "x", -1], [0, 5, null], [3.0, "y", NAN]])
	assert_eq(wild.hill, 0)
	assert_eq(wild.hill_left, ZoneRules.HILL_TIME)
	assert_eq(wild.owners, [ZoneRules.NOBODY, ZoneRules.NOBODY, ZoneRules.NOBODY] as Array[int])
	assert_eq(wild.taking, [CHEDDAR, ZoneRules.NOBODY, ZoneRules.NOBODY] as Array[int])
	assert_eq(wild.progress, [1.0, 0.0, 0.0] as Array[float])
