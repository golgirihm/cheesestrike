extends GutTest


func _session(code: String, players: int, mode := MatchSettings.FREE_FOR_ALL, map := "box", most := 8) -> Dictionary:
	return {"code": code, "players": players, "settings": {"mode": mode, "map": map, "max_players": most}}


func _codes(list: Array) -> Array:
	return list.map(func(session: Dictionary) -> String: return session.code)


func test_the_fullest_come_first_unless_asked_otherwise() -> void:
	var list := [_session("AAAA", 2), _session("BBBB", 5), _session("CCCC", 1)]
	assert_eq(_codes(SessionList.arranged(list)), ["BBBB", "AAAA", "CCCC"])
	assert_eq(_codes(SessionList.arranged(list, SessionList.EMPTIEST)), ["CCCC", "AAAA", "BBBB"])


func test_sessions_can_be_sorted_by_arena() -> void:
	var list := [_session("AAAA", 2, MatchSettings.FREE_FOR_ALL, Maps.ORDER[1]), _session("BBBB", 5, MatchSettings.FREE_FOR_ALL, Maps.ORDER[0])]
	var names := SessionList.arranged(list, SessionList.BY_ARENA).map(func(session: Dictionary) -> String:
		return Maps.NAMES[session.settings.map])
	var sorted := names.duplicate()
	sorted.sort()
	assert_eq(names, sorted)


func test_sessions_can_be_filtered_by_mode_arena_and_room() -> void:
	var other_mode: String = MatchSettings.MODES.filter(func(mode: String) -> bool: return mode != MatchSettings.FREE_FOR_ALL and mode != MatchSettings.PRACTICE)[0]
	var list := [_session("AAAA", 2), _session("BBBB", 8), _session("CCCC", 1, other_mode), _session("DDDD", 1, MatchSettings.FREE_FOR_ALL, Maps.ORDER[0])]
	assert_eq(_codes(SessionList.arranged(list, SessionList.FULLEST, other_mode)), ["CCCC"])
	assert_eq(_codes(SessionList.arranged(list, SessionList.FULLEST, SessionList.ANY, Maps.ORDER[0])), ["DDDD"])
	assert_false(_codes(SessionList.arranged(list, SessionList.FULLEST, SessionList.ANY, SessionList.ANY, true)).has("BBBB"), "full")


func test_play_now_joins_the_fullest_with_room() -> void:
	var list := [_session("AAAA", 2), _session("BBBB", 8), _session("CCCC", 5)]
	assert_eq(SessionList.play_now(list).code, "CCCC")


func test_play_now_prefers_the_mode_the_player_would_host() -> void:
	var other_mode: String = MatchSettings.MODES.filter(func(mode: String) -> bool: return mode != MatchSettings.FREE_FOR_ALL and mode != MatchSettings.PRACTICE)[0]
	var list := [_session("AAAA", 6), _session("BBBB", 1, other_mode)]
	assert_eq(SessionList.play_now(list, other_mode).code, "BBBB")
	assert_eq(SessionList.play_now([_session("AAAA", 6)], other_mode).code, "AAAA", "but any with room beats none")


func test_play_now_with_nothing_open_means_hosting() -> void:
	assert_eq(SessionList.play_now([]), {})
	assert_eq(SessionList.play_now([_session("AAAA", 8)]), {})


func test_nonsense_from_the_server_is_left_out() -> void:
	var list := [null, "AAAA", {"code": "BBBB", "players": "lots"}]
	assert_eq(SessionList.arranged(list).size(), 1)
	assert_eq(SessionList.players(list[2]), 0)


func test_choices_go_round() -> void:
	assert_eq(SessionList.next_of(SessionList.SORTS, SessionList.FULLEST), SessionList.SORTS[1])
	assert_eq(SessionList.next_of(SessionList.SORTS, SessionList.SORTS[-1]), SessionList.SORTS[0])
