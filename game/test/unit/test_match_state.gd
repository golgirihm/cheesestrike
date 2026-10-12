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
	var good := PlayerRecord.new(4)
	good.name = "Edam"
	var packed := good.to_packed()
	var copy := MatchState.from_dict({"players": [[7, packed[1]], "junk", packed, ["Short", PackedInt32Array([3, 2])]]})
	assert_eq(copy.players.size(), 1)
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


func test_players_cannot_hurt_themselves_with_a_shot() -> void:
	state.add_player(1)
	assert_eq(state.damage(1, 1, 50.0).dealt, 0)
	assert_eq(state.player(1).health, PlayerRecord.MAX_HEALTH)


func test_a_burst_of_their_own_hurts_a_player_and_fills_no_meter() -> void:
	state.add_player(1)
	assert_eq(state.damage(1, 1, 50.0, true).dealt, 50)
	assert_eq(state.player(1).health, PlayerRecord.MAX_HEALTH - 50)
	assert_eq(state.player(1).overload, 0.0)


func test_knocking_themself_out_costs_a_player_a_death_and_scores_nothing() -> void:
	state.add_player(1)
	var result := state.damage(1, 1, 500.0, true)
	assert_true(result.knocked_out)
	var record := state.player(1)
	assert_false(record.alive)
	assert_eq(record.deaths, 1)
	assert_eq(record.kills, 0)
	assert_eq(record.last_attacker, 1)


func test_a_teammates_burst_still_cannot_hurt() -> void:
	var teams := MatchState.new(MatchSettings.from_dict({"mode": "teams"}))
	teams.add_player(1)
	teams.add_player(2)
	teams.add_player(3)
	assert_true(teams.are_allies(1, 3))
	assert_eq(teams.damage(1, 3, 50.0, true).dealt, 0)


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


func test_a_new_player_carries_only_the_starting_weapon() -> void:
	assert_eq(state.add_player(5).weapons, [Weapons.STARTING])


func test_being_knocked_out_loses_picked_up_weapons() -> void:
	state.add_player(1)
	var record := state.add_player(2)
	record.weapons = ["blaster", "launcher"]
	record.weapon = "launcher"
	state.damage(1, 2, 500.0)
	assert_eq(record.weapons, [Weapons.STARTING])
	assert_eq(record.weapon, Weapons.STARTING)


func test_carried_weapons_survive_the_trip_and_nonsense_is_dropped() -> void:
	var record := state.add_player(2)
	record.weapons = ["blaster", "grater"]
	record.weapon = "grater"
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict()))).player(2)
	assert_eq(copy.weapons, ["blaster", "grater"])
	assert_eq(copy.weapon, "grater")
	var odd := PlayerRecord.from_dict({"id": 3, "weapons": ["spoon", 4], "weapon": "spoon"})
	assert_eq(odd.weapons, [Weapons.STARTING])
	assert_eq(odd.weapon, Weapons.STARTING)


func test_every_part_of_a_record_survives_packing() -> void:
	var record := PlayerRecord.new(1987654321)
	record.name = "Packed Paneer"
	record.health = 37
	record.alive = false
	record.respawn_in = 2.345
	record.protected_for = 1.5
	record.kills = 12
	record.deaths = 9
	record.last_attacker = 1234567
	record.weapons = ["blaster", "grater", "launcher"]
	record.weapon = "grater"
	record.team = MatchState.BRIE
	record.overload = 66.25
	record.overload_left = 4.5
	record.melted_for = 0.75
	record.ping = 123
	var copy := PlayerRecord.from_packed(bytes_to_var(var_to_bytes(record.to_packed())))
	assert_eq(copy.to_dict(), record.to_dict())


func test_a_packed_record_with_nonsense_is_set_right() -> void:
	var numbers := PackedInt32Array()
	numbers.resize(15)
	numbers.fill(-5)
	var copy := PlayerRecord.from_packed(["X", numbers])
	assert_eq(copy.health, 0)
	assert_eq(copy.respawn_in, 0.0)
	assert_eq(copy.team, MatchState.NO_TEAM)
	assert_eq(copy.overload, 0.0)
	assert_has(Weapons.ORDER, copy.weapon)
	assert_true(copy.weapons.has(copy.weapon))
	assert_null(PlayerRecord.from_packed("nope"))


func test_a_full_session_is_small_enough_to_send_often() -> void:
	# The host sends this to each of up to fifteen others, up to ten times a
	# second; at this size that is under 400 kB/s in all.
	var full := MatchState.new()
	full.set_pickup_count(6)
	for i in MatchSettings.MAX_PLAYERS:
		var record := full.add_player(2000000000 + i)
		record.name = "Mozzarella %d" % i
		record.weapons = ["blaster", "shotgun", "grater", "launcher"]
		record.respawn_in = 1.234
		record.overload = 55.5
	assert_lt(var_to_bytes(full.to_dict()).size(), 2500)
