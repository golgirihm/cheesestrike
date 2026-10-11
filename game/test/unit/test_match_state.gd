extends GutTest

var state: MatchState


func before_each() -> void:
	state = MatchState.new()


func test_players_are_added_once() -> void:
	var first := state.add_player(5)
	assert_same(state.add_player(5), first)
	assert_eq(state.players.size(), 1)


func test_a_new_player_is_called_the_fallback_name() -> void:
	state.add_player(5)
	assert_eq(state.name_of(5), PlayerName.FALLBACK)


func test_an_unknown_player_is_called_the_fallback_name() -> void:
	assert_eq(state.name_of(77), PlayerName.FALLBACK)


func test_renaming_cleans_the_name() -> void:
	state.add_player(5)
	assert_true(state.rename(5, "  Blue" + char(0x200B) + "  Vinny "))
	assert_eq(state.name_of(5), "Blue Vinny")


func test_renaming_to_nothing_keeps_the_old_name() -> void:
	state.add_player(5)
	state.rename(5, "Feta")
	assert_false(state.rename(5, " " + char(0x200B) + " "))
	assert_eq(state.name_of(5), "Feta")


func test_renaming_to_the_same_name_is_no_change() -> void:
	state.add_player(5)
	state.rename(5, "Feta")
	assert_false(state.rename(5, "Feta "))


func test_renaming_an_unknown_player_does_nothing() -> void:
	assert_false(state.rename(9, "Ghost"))
	assert_eq(state.players.size(), 0)


func test_removing_a_player_forgets_them() -> void:
	state.add_player(5)
	state.remove_player(5)
	assert_null(state.player(5))
	state.remove_player(5)


func test_the_state_survives_the_trip_to_other_players() -> void:
	state.add_player(1)
	state.add_player(123456)
	state.rename(1, "Host")
	state.rename(123456, "Joiner")
	# As an RPC would carry it: through Godot's own encoding.
	var sent: Variant = bytes_to_var(var_to_bytes(state.to_dict()))
	var copy := MatchState.from_dict(sent)
	assert_eq(copy.players.size(), 2)
	assert_eq(copy.name_of(1), "Host")
	assert_eq(copy.name_of(123456), "Joiner")


func test_a_malformed_state_gives_what_it_can() -> void:
	var copy := MatchState.from_dict({"players": [{"id": 3, "name": 7}, "junk", {"id": 4, "name": "Edam"}]})
	assert_eq(copy.name_of(3), PlayerName.FALLBACK)
	assert_eq(copy.name_of(4), "Edam")
	assert_eq(MatchState.from_dict({"players": "nope"}).players.size(), 0)
	assert_eq(MatchState.from_dict({}).players.size(), 0)


func test_a_new_player_is_in_play_with_full_health() -> void:
	var record := state.add_player(5)
	assert_true(record.alive)
	assert_eq(record.health, PlayerRecord.MAX_HEALTH)
	assert_eq(record.weapon, Weapons.STARTING)


func test_damage_takes_health() -> void:
	state.add_player(1)
	state.add_player(2)
	var result := state.damage(1, 2, 20.0)
	assert_eq(result.dealt, 20)
	assert_false(result.knocked_out)
	assert_eq(state.player(2).health, 80)


func test_damage_is_rounded_to_whole_points() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 34.6)
	assert_eq(state.player(2).health, 65)


func test_running_out_of_health_knocks_a_player_out_and_scores() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 60.0)
	var result := state.damage(1, 2, 60.0)
	assert_eq(result.dealt, 40, "only what was left is taken")
	assert_true(result.knocked_out)
	var victim := state.player(2)
	assert_false(victim.alive)
	assert_eq(victim.health, 0)
	assert_eq(victim.deaths, 1)
	assert_eq(victim.last_attacker, 1)
	assert_eq(victim.respawn_in, MatchState.RESPAWN_DELAY)
	assert_eq(state.player(1).kills, 1)


func test_a_player_who_is_out_takes_no_more_damage() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 200.0)
	var result := state.damage(1, 2, 50.0)
	assert_eq(result.dealt, 0)
	assert_eq(state.player(1).kills, 1, "no second kill")
	assert_eq(state.player(2).deaths, 1)


func test_players_cannot_hurt_themselves() -> void:
	state.add_player(1)
	assert_eq(state.damage(1, 1, 50.0).dealt, 0)
	assert_eq(state.player(1).health, PlayerRecord.MAX_HEALTH)


func test_damage_from_someone_who_has_left_still_counts() -> void:
	state.add_player(2)
	var result := state.damage(9, 2, 500.0)
	assert_true(result.knocked_out)
	assert_eq(state.player(2).deaths, 1)


func test_damage_to_an_unknown_player_does_nothing() -> void:
	state.add_player(1)
	assert_eq(state.damage(1, 9, 50.0).dealt, 0)


func test_zero_or_negative_damage_does_nothing() -> void:
	state.add_player(1)
	state.add_player(2)
	assert_eq(state.damage(1, 2, 0.0).dealt, 0)
	assert_eq(state.damage(1, 2, -30.0).dealt, 0)
	assert_eq(state.player(2).health, PlayerRecord.MAX_HEALTH)


func test_a_knocked_out_player_is_due_back_after_the_delay() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 200.0)
	assert_eq(state.tick(MatchState.RESPAWN_DELAY - 0.5), [] as Array[int])
	assert_eq(state.tick(0.5), [2] as Array[int])
	assert_eq(state.tick(0.5), [] as Array[int], "only reported once")


func test_coming_back_restores_health_with_protection() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 200.0)
	state.tick(MatchState.RESPAWN_DELAY)
	state.respawn(2)
	var record := state.player(2)
	assert_true(record.alive)
	assert_eq(record.health, PlayerRecord.MAX_HEALTH)
	assert_true(record.is_protected())
	assert_eq(record.deaths, 1, "the score is kept")


func test_a_protected_player_cannot_be_hurt() -> void:
	state.add_player(1)
	state.add_player(2)
	state.respawn(2)
	assert_eq(state.damage(1, 2, 50.0).dealt, 0)


func test_protection_wears_off() -> void:
	state.add_player(1)
	state.add_player(2)
	state.respawn(2)
	state.tick(MatchState.PROTECTION)
	assert_false(state.player(2).is_protected())
	assert_eq(state.damage(1, 2, 50.0).dealt, 50)


func test_health_and_score_survive_the_trip_to_other_players() -> void:
	state.add_player(1)
	state.add_player(2)
	state.damage(1, 2, 200.0)
	state.damage(2, 1, 30.0)
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict())))
	assert_eq(copy.player(1).health, 70)
	assert_eq(copy.player(1).kills, 1)
	assert_false(copy.player(2).alive)
	assert_eq(copy.player(2).deaths, 1)
	assert_eq(copy.player(2).last_attacker, 1)
	assert_almost_eq(copy.player(2).respawn_in, MatchState.RESPAWN_DELAY, 0.001)
