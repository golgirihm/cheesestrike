extends GutTest

var state: MatchState


func before_each() -> void:
	state = MatchState.new()
	for id in [1, 2, 3]:
		state.add_player(id)


func _fill(id: int) -> void:
	state.player(id).overload = Overload.METER_FULL


func test_dealing_damage_fills_the_meter() -> void:
	state.damage(1, 2, 40.0)
	assert_eq(state.player(1).overload, 40.0 * Overload.CHARGE_PER_DAMAGE)
	assert_eq(state.player(2).overload, 0.0, "taking damage doesn't")


func test_two_knock_outs_fill_it() -> void:
	state.damage(1, 2, 500.0)
	state.damage(1, 3, 500.0)
	assert_eq(state.player(1).overload, Overload.METER_FULL)


func test_the_meter_stops_at_full() -> void:
	for victim in [2, 3]:
		state.damage(1, victim, 500.0)
		state.respawn(victim)
		state.tick(MatchState.PROTECTION)
	state.damage(1, 2, 500.0)
	assert_eq(state.player(1).overload, Overload.METER_FULL)


func test_it_can_only_be_set_off_when_full() -> void:
	state.player(1).overload = 99.0
	assert_false(state.set_off_overload(1))
	_fill(1)
	assert_true(state.set_off_overload(1))
	assert_eq(state.player(1).overload, 0.0, "it empties")
	assert_eq(state.player(1).overload_left, Overload.DURATION)


func test_it_cannot_be_set_off_twice_at_once_or_while_out() -> void:
	_fill(1)
	state.set_off_overload(1)
	_fill(1)
	assert_false(state.set_off_overload(1), "already in one")
	_fill(2)
	state.damage(1, 2, 500.0)
	_fill(2)
	assert_false(state.set_off_overload(2), "knocked out")


func test_it_cannot_be_set_off_between_matches() -> void:
	_fill(1)
	state.end_match()
	assert_false(state.set_off_overload(1))


func test_it_wears_off() -> void:
	_fill(1)
	state.set_off_overload(1)
	state.tick(Overload.DURATION - 0.1)
	assert_gt(state.player(1).overload_left, 0.0)
	state.tick(0.1)
	assert_eq(state.player(1).overload_left, 0.0)


func test_damage_during_an_overload_does_not_fill_the_meter() -> void:
	_fill(1)
	state.set_off_overload(1)
	state.damage(1, 2, 40.0)
	assert_eq(state.player(1).overload, 0.0)


func test_hits_during_an_overload_melt_the_target_for_a_moment() -> void:
	_fill(1)
	state.set_off_overload(1)
	state.damage(1, 2, 10.0)
	assert_eq(state.player(2).melted_for, Overload.MELT_DURATION)
	state.tick(Overload.MELT_DURATION)
	assert_eq(state.player(2).melted_for, 0.0)


func test_hits_without_an_overload_do_not_melt() -> void:
	state.damage(1, 2, 10.0)
	assert_eq(state.player(2).melted_for, 0.0)


func test_it_does_no_extra_damage() -> void:
	_fill(1)
	state.set_off_overload(1)
	assert_eq(state.damage(1, 2, 20.0).dealt, 20)


func test_being_knocked_out_halves_the_meter_and_ends_it_all() -> void:
	state.player(2).overload = 80.0
	state.damage(1, 2, 500.0)
	assert_eq(state.player(2).overload, 80.0 * Overload.KEPT_ON_KNOCK_OUT)
	_fill(3)
	state.set_off_overload(3)
	state.damage(1, 3, 500.0)
	assert_eq(state.player(3).overload_left, 0.0)


func test_a_new_match_empties_every_meter() -> void:
	_fill(1)
	state.set_off_overload(1)
	state.damage(1, 2, 10.0)
	state.start_match()
	for id in [1, 2]:
		assert_eq(state.player(id).overload, 0.0)
		assert_eq(state.player(id).overload_left, 0.0)
		assert_eq(state.player(id).melted_for, 0.0)


func test_speed_goes_up_in_an_overload_and_down_when_melted() -> void:
	var record := state.player(1)
	assert_eq(Overload.speed_factor(record), 1.0)
	record.overload_left = 1.0
	assert_eq(Overload.speed_factor(record), Overload.SPEED_BOOST)
	record.melted_for = 1.0
	assert_almost_eq(Overload.speed_factor(record), Overload.SPEED_BOOST * Overload.MELT_SLOW, 0.0001)
	assert_eq(Overload.speed_factor(null), 1.0)


func test_the_meter_survives_the_trip() -> void:
	state.player(1).overload = 42.0
	state.player(2).overload_left = 3.0
	state.player(3).melted_for = 1.5
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict())))
	assert_eq(copy.player(1).overload, 42.0)
	assert_eq(copy.player(2).overload_left, 3.0)
	assert_eq(copy.player(3).melted_for, 1.5)
	assert_eq(PlayerRecord.from_dict({"id": 1, "overload": 9000.0}).overload, Overload.METER_FULL)


func test_the_meter_reads_as_a_share_then_ready_then_counting_down() -> void:
	const HUD := preload("res://ui/combat_hud.gd")
	var record := state.player(1)
	record.overload = 37.5
	assert_eq(HUD.overload_text(record), "Overload 37%")
	_fill(1)
	assert_eq(HUD.overload_text(record), "Overload ready: Q")
	state.set_off_overload(1)
	state.tick(2.5)
	assert_eq(HUD.overload_text(record), "OVERLOAD  6s")
