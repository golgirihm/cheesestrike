extends GutTest
## A match from start to finish: limits, the clock, winners and teams.

var state: MatchState


func _free_for_all(kill_limit := 3, minutes := 5) -> MatchState:
	return MatchState.new(MatchSettings.from_dict({"mode": "ffa", "kill_limit": kill_limit, "minutes": minutes}))


func _teams(kill_limit := 3) -> MatchState:
	return MatchState.new(MatchSettings.from_dict({"mode": "teams", "kill_limit": kill_limit}))


func _knock_out(attacker: int, victim: int) -> Dictionary:
	state.player(victim).protected_for = 0.0
	state.player(victim).alive = true
	state.player(victim).health = PlayerRecord.MAX_HEALTH
	return state.damage(attacker, victim, 500.0)


func before_each() -> void:
	state = _free_for_all()
	for id in [1, 2, 3]:
		state.add_player(id)


func test_a_match_starts_playing_with_the_full_time() -> void:
	assert_true(state.is_playing())
	assert_eq(state.time_left, 300.0)


func test_reaching_the_kill_limit_ends_the_match() -> void:
	_knock_out(1, 2)
	_knock_out(1, 3)
	var result := _knock_out(1, 2)
	assert_true(result.ended)
	assert_false(state.is_playing())
	assert_eq(state.winners, [1] as Array[int])
	assert_eq(state.result_text(), "%s wins!" % state.name_of(1))
	assert_eq(state.time_left, MatchState.INTERMISSION)


func test_nobody_can_be_hurt_between_matches() -> void:
	state.end_match()
	assert_eq(state.damage(1, 2, 50.0).dealt, 0)


func test_running_out_of_time_ends_the_match_with_the_leader_winning() -> void:
	_knock_out(2, 1)
	state.tick(299.0)
	assert_true(state.is_playing())
	state.tick(1.0)
	assert_false(state.is_playing())
	assert_eq(state.winners, [2] as Array[int])


func test_level_at_the_end_goes_to_sudden_death() -> void:
	_knock_out(1, 3)
	_knock_out(2, 3)
	state.tick(300.0)
	assert_true(state.is_playing(), "not over yet")
	assert_true(state.overtime)
	assert_true(state.just_overtime, "for the host to tell everyone")
	assert_eq(state.time_left, MatchState.OVERTIME)


func test_the_next_score_in_sudden_death_wins() -> void:
	_knock_out(1, 3)
	_knock_out(2, 3)
	state.tick(300.0)
	var result := _knock_out(2, 1)
	assert_true(result.ended)
	assert_false(state.is_playing())
	assert_false(state.overtime)
	assert_eq(state.winners, [2] as Array[int])


func test_a_score_that_leaves_it_level_does_not_end_sudden_death() -> void:
	state = _free_for_all(10)
	for id in [1, 2, 3]:
		state.add_player(id)
	_knock_out(1, 3)
	_knock_out(1, 3)
	_knock_out(2, 3)
	_knock_out(2, 3)
	state.tick(300.0)
	assert_true(state.overtime)
	_knock_out(3, 1)
	assert_true(state.is_playing(), "the two in front are still level")


func test_sudden_death_that_runs_out_level_is_a_draw() -> void:
	state.rename(1, "Brie")
	state.rename(2, "Edam")
	_knock_out(1, 3)
	_knock_out(2, 3)
	state.tick(300.0)
	state.tick(MatchState.OVERTIME)
	assert_false(state.is_playing(), "a clear end")
	assert_eq(state.winners, [1, 2] as Array[int])
	assert_eq(state.result_text(), "Draw: Brie and Edam")


func test_nobody_scoring_goes_to_sudden_death_and_then_nobody_wins() -> void:
	state.tick(300.0)
	assert_true(state.overtime)
	state.tick(MatchState.OVERTIME)
	assert_false(state.is_playing())
	assert_eq(state.winners.size(), 0)
	assert_eq(state.result_text(), "Nobody wins")


func test_teams_level_at_the_end_play_sudden_death_too() -> void:
	state = _teams(10)
	for id in [1, 2]:
		state.add_player(id)
	state.tick(state.time_left)
	assert_true(state.overtime)
	_knock_out(1, 2)
	assert_false(state.is_playing())
	assert_eq(state.winners, [state.player(1).team] as Array[int])


func test_a_new_match_is_not_in_sudden_death() -> void:
	state.tick(300.0)
	state.tick(MatchState.OVERTIME)
	state.start_match()
	assert_false(state.overtime)
	assert_true(state.started)


func test_a_match_waits_for_a_second_player() -> void:
	state = _free_for_all()
	assert_true(state.is_warm_up())
	state.add_player(1)
	assert_true(state.is_warm_up())
	state.tick(60.0)
	assert_eq(state.time_left, 300.0, "the clock stands still")
	state.add_player(2)
	assert_false(state.is_warm_up())
	assert_true(state.just_started, "for the host to tell everyone")
	state.tick(60.0)
	assert_eq(state.time_left, 240.0)


func test_nothing_done_in_the_warm_up_counts() -> void:
	state = _free_for_all(1)
	state.add_player(1)
	state.player(1).overload = 80.0
	state.damage(1, 1, 500.0, true)
	assert_eq(state.player(1).deaths, 1)
	assert_true(state.is_playing(), "and a warm-up never ends")
	state.respawn(1)
	state.add_player(2)
	assert_eq(state.player(1).deaths, 0)
	assert_eq(state.player(1).overload, 0.0)
	assert_true(state.player(1).alive, "they stay where they are, in play")


func test_a_warm_up_with_one_player_runs_no_clock_however_long() -> void:
	state = _free_for_all()
	state.add_player(1)
	state.tick(10000.0)
	assert_true(state.is_playing())
	assert_false(state.overtime)


func test_a_match_that_has_begun_carries_on_when_someone_leaves() -> void:
	state = _free_for_all()
	state.add_player(1)
	state.add_player(2)
	state.remove_player(2)
	assert_false(state.is_warm_up())
	state.tick(10.0)
	assert_eq(state.time_left, 290.0)
	# But the next match waits again, and one player can't go to overtime.
	state.tick(290.0)
	assert_false(state.is_playing(), "nobody to be level with")
	state.start_match()
	assert_true(state.is_warm_up())


func test_warm_up_and_sudden_death_survive_the_trip() -> void:
	state = _free_for_all()
	state.add_player(1)
	assert_true(MatchState.from_dict(state.to_dict()).is_warm_up())
	state.add_player(2)
	var copy := MatchState.from_dict(state.to_dict())
	assert_false(copy.is_warm_up())
	assert_false(copy.overtime)
	state.tick(300.0)
	assert_true(MatchState.from_dict(state.to_dict()).overtime)


func test_the_next_match_is_due_after_the_break() -> void:
	state.end_match()
	assert_false(state.match_due())
	state.tick(MatchState.INTERMISSION - 0.1)
	assert_false(state.match_due())
	state.tick(0.1)
	assert_true(state.match_due())


func test_a_new_match_clears_the_scores_and_resets_the_clock() -> void:
	_knock_out(1, 2)
	_knock_out(2, 3)
	state.end_match()
	state.tick(MatchState.INTERMISSION)
	state.start_match()
	assert_true(state.is_playing())
	assert_eq(state.time_left, 300.0)
	assert_eq(state.winners.size(), 0)
	for record: PlayerRecord in state.players.values():
		assert_eq(record.kills, 0)
		assert_eq(record.deaths, 0)
	assert_eq(state.name_of(1), state.players[1].name, "names stay")


func test_a_player_leaving_mid_match_does_not_break_the_ending() -> void:
	_knock_out(3, 1)
	state.remove_player(3)
	state.tick(300.0)
	assert_eq(state.winners.size(), 0, "the leader left, and nobody else scored")


func test_the_state_of_a_match_survives_the_trip() -> void:
	_knock_out(1, 2)
	state.end_match()
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict())))
	assert_false(copy.is_playing())
	assert_eq(copy.winners, [1] as Array[int])
	assert_almost_eq(copy.time_left, MatchState.INTERMISSION, 0.001)
	assert_eq(copy.settings.kill_limit, 3)


func test_teams_are_filled_evenly() -> void:
	state = _teams()
	for id in range(1, 7):
		state.add_player(id)
	var counts := [0, 0]
	for record: PlayerRecord in state.players.values():
		counts[record.team] += 1
	assert_eq(counts, [3, 3])
	assert_eq(state.player(1).team, MatchState.CHEDDAR, "the first player starts Cheddar")


func test_a_newcomer_joins_the_smaller_team() -> void:
	state = _teams()
	state.add_player(1)
	state.add_player(2)
	state.add_player(3)
	state.remove_player(state.players.values().filter(func(r: PlayerRecord) -> bool: return r.team == MatchState.BRIE)[0].id)
	assert_eq(state.add_player(9).team, MatchState.BRIE)


func test_with_teams_level_a_newcomer_helps_the_team_behind() -> void:
	state = _teams(10)
	state.add_player(1)
	state.add_player(2)
	var cheddar := 1 if state.player(1).team == MatchState.CHEDDAR else 2
	var brie := 3 - cheddar
	_knock_out(cheddar, brie)
	assert_eq(state.add_player(5).team, MatchState.BRIE)


func test_free_for_all_has_no_teams() -> void:
	assert_eq(state.player(1).team, MatchState.NO_TEAM)
	assert_false(state.are_allies(1, 2))


func test_teammates_cannot_hurt_each_other() -> void:
	state = _teams()
	for id in [1, 2, 3, 4]:
		state.add_player(id)
	var mates := state.players.values().filter(func(r: PlayerRecord) -> bool: return r.team == MatchState.CHEDDAR)
	assert_true(state.are_allies(mates[0].id, mates[1].id))
	assert_eq(state.damage(mates[0].id, mates[1].id, 50.0).dealt, 0)


func test_team_knock_outs_count_toward_the_team_and_win() -> void:
	state = _teams(2)
	for id in [1, 2, 3, 4]:
		state.add_player(id)
	var cheddar: Array = state.players.values().filter(func(r: PlayerRecord) -> bool: return r.team == MatchState.CHEDDAR)
	var brie: Array = state.players.values().filter(func(r: PlayerRecord) -> bool: return r.team == MatchState.BRIE)
	_knock_out(brie[0].id, cheddar[0].id)
	_knock_out(brie[1].id, cheddar[1].id)
	assert_eq(state.team_kills, [0, 2])
	assert_false(state.is_playing())
	assert_eq(state.winners, [MatchState.BRIE] as Array[int])
	assert_eq(state.result_text(), "Brie wins!")


func test_a_new_match_clears_the_team_scores() -> void:
	state = _teams(5)
	state.add_player(1)
	state.add_player(2)
	_knock_out(1, 2)
	state.start_match()
	assert_eq(state.team_kills, [0, 0])
	assert_ne(state.player(1).team, MatchState.NO_TEAM, "teams stay")
