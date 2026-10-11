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
