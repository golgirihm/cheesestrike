extends GutTest
## Capture the cheese wheel, as rules alone.

const BASES := [Vector3(-20, 0, 0), Vector3(20, 0, 0)]
const CHEDDAR := MatchState.CHEDDAR
const BRIE := MatchState.BRIE

var rules: CaptureRules
var standing: Dictionary


func before_each() -> void:
	rules = CaptureRules.new()
	rules.reset(BASES)
	# Player 1 is Cheddar's, 2 Brie's, each at home; 3 is Cheddar's too.
	standing = {1: _at(BASES[CHEDDAR], CHEDDAR), 2: _at(BASES[BRIE], BRIE), 3: _at(Vector3(0, 0, 9), CHEDDAR)}
	assert_gt(CaptureRules.REACH, BodyShape.RADIUS * 2.0, "a player standing on a wheel can't keep another from it")


func _at(at: Vector3, team: int, alive := true) -> Dictionary:
	return {"at": at, "team": team, "alive": alive}


func _types(events: Array) -> Array:
	return events.map(func(event: Dictionary) -> String: return event.type)


func test_both_wheels_start_at_home_and_nobody_has_one() -> void:
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary])
	for team in 2:
		assert_eq(rules.wheels[team].status, CaptureRules.HOME)
		assert_eq(rules.wheels[team].at, BASES[team])
	assert_eq(rules.carried_by(1), -1)


func test_a_player_takes_the_other_teams_wheel_by_reaching_it() -> void:
	standing[1].at = BASES[BRIE] + Vector3(1, 0, 0)
	var events := rules.update(0.1, BASES, standing)
	assert_eq(events, [{"type": CaptureRules.TAKEN, "wheel": BRIE, "by": 1}] as Array[Dictionary])
	assert_eq(rules.carried_by(1), BRIE)
	assert_eq(rules.wheels[BRIE].status, CaptureRules.CARRIED)


func test_nobody_takes_their_own_wheel_from_home() -> void:
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary], "each stands on their own")
	assert_eq(rules.carried_by(1), -1)
	assert_eq(rules.carried_by(2), -1)


func test_a_wheel_goes_where_its_carrier_does() -> void:
	standing[1].at = BASES[BRIE]
	rules.update(0.1, BASES, standing)
	standing[1].at = Vector3(5, 0, 3)
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary])
	assert_eq(rules.wheels[BRIE].at, Vector3(5, 0, 3))


func test_bringing_it_home_scores_and_sends_it_back() -> void:
	standing[1].at = BASES[BRIE]
	rules.update(0.1, BASES, standing)
	standing[1].at = BASES[CHEDDAR] + Vector3(0, 0, 2)
	var events := rules.update(0.1, BASES, standing)
	assert_eq(events, [{"type": CaptureRules.CAPTURED, "wheel": BRIE, "by": 1}] as Array[Dictionary])
	assert_eq(rules.wheels[BRIE].status, CaptureRules.HOME)
	assert_eq(rules.wheels[BRIE].at, BASES[BRIE])
	assert_eq(rules.carried_by(1), -1)
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary], "once")


func test_it_cannot_score_while_their_own_wheel_is_away() -> void:
	standing[1].at = BASES[BRIE]
	standing[2].at = BASES[CHEDDAR]
	assert_eq(_types(rules.update(0.1, BASES, standing)), [CaptureRules.TAKEN, CaptureRules.TAKEN])
	standing[1].at = BASES[CHEDDAR]
	standing[2].at = Vector3(0, 0, -9)
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary], "home, but their own is gone")
	assert_eq(rules.carried_by(1), BRIE)


func test_a_carrier_knocked_out_drops_it_where_they_were() -> void:
	standing[1].at = BASES[BRIE]
	rules.update(0.1, BASES, standing)
	standing[1].at = Vector3(4, 0, 4)
	rules.update(0.1, BASES, standing)
	standing[1].alive = false
	var events := rules.update(0.1, BASES, standing)
	assert_eq(events, [{"type": CaptureRules.FELL, "wheel": BRIE, "by": 1}] as Array[Dictionary])
	assert_eq(rules.wheels[BRIE].status, CaptureRules.DROPPED)
	assert_eq(rules.wheels[BRIE].at, Vector3(4, 0, 4))
	assert_eq(rules.carried_by(1), -1)


func test_a_carrier_who_leaves_drops_it_too() -> void:
	standing[1].at = BASES[BRIE]
	rules.update(0.1, BASES, standing)
	standing.erase(1)
	assert_eq(_types(rules.update(0.1, BASES, standing)), [CaptureRules.FELL])


func test_its_own_team_sends_a_dropped_wheel_home_by_touching_it() -> void:
	standing[1].at = Vector3(4, 0, 4)
	rules.wheels[BRIE] = {"status": CaptureRules.DROPPED, "carrier": 0, "at": Vector3(4, 0, 4), "left": 10.0}
	standing[1].alive = false
	standing[2].at = Vector3(4, 0, 5)
	var events := rules.update(0.1, BASES, standing)
	assert_eq(events, [{"type": CaptureRules.RETURNED, "wheel": BRIE, "by": 2}] as Array[Dictionary])
	assert_eq(rules.wheels[BRIE].at, BASES[BRIE])


func test_the_other_team_takes_a_dropped_wheel_up_again() -> void:
	rules.wheels[BRIE] = {"status": CaptureRules.DROPPED, "carrier": 0, "at": Vector3(4, 0, 4), "left": 10.0}
	standing[3].at = Vector3(4, 0, 4.5)
	assert_eq(rules.update(0.1, BASES, standing), [{"type": CaptureRules.TAKEN, "wheel": BRIE, "by": 3}] as Array[Dictionary])


func test_a_dropped_wheel_left_alone_goes_home_in_time() -> void:
	rules.wheels[BRIE] = {"status": CaptureRules.DROPPED, "carrier": 0, "at": Vector3(4, 0, 4), "left": CaptureRules.RETURN_AFTER}
	assert_eq(rules.update(CaptureRules.RETURN_AFTER - 0.5, BASES, standing), [] as Array[Dictionary])
	assert_eq(rules.update(1.0, BASES, standing), [{"type": CaptureRules.RETURNED, "wheel": BRIE, "by": 0}] as Array[Dictionary])
	assert_eq(rules.wheels[BRIE].status, CaptureRules.HOME)


func test_the_knocked_out_and_those_on_no_team_touch_nothing() -> void:
	standing[1] = _at(BASES[BRIE], CHEDDAR, false)
	standing[9] = _at(BASES[BRIE], MatchState.NO_TEAM)
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary])


func test_a_wheel_on_another_floor_is_out_of_reach() -> void:
	standing[1].at = BASES[BRIE] + Vector3(0, 5, 0)
	assert_eq(rules.update(0.1, BASES, standing), [] as Array[Dictionary])


func test_a_carrier_is_slowed() -> void:
	assert_lt(CaptureRules.CARRY_SPEED, 1.0)
	var state := MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.CAPTURE}))
	var record := state.add_player(1)
	assert_eq(state.speed_factor(record), 1.0)
	state.capture.wheels[BRIE].status = CaptureRules.CARRIED
	state.capture.wheels[BRIE].carrier = 1
	assert_eq(state.speed_factor(record), CaptureRules.CARRY_SPEED)
	assert_eq(MatchState.new().speed_factor(record), 1.0, "only in this mode")


func test_the_wheels_survive_the_trip_and_nonsense_does_not() -> void:
	standing[1].at = BASES[BRIE]
	rules.update(0.1, BASES, standing)
	var copy := CaptureRules.from_array(rules.to_array())
	assert_eq(copy.carried_by(1), BRIE)
	assert_eq(copy.wheels[CHEDDAR].status, CaptureRules.HOME)
	for odd: Variant in [null, 7, [], [1, 2], [[9, 0, Vector3.ZERO, 0.0], "x"], [[0, 0, Vector3(INF, 0, 0), 0.0], [0, "a", Vector3.ZERO, 0.0]]]:
		var made := CaptureRules.from_array(odd)
		assert_eq(made.wheels.size(), 2)
		for team in 2:
			assert_eq(made.wheels[team].status, CaptureRules.HOME)
