extends GutTest
## Players who lose their connection and come back. Offline, this process is
## the host; other players' introductions are made by calling what their RPC
## would.

const MATCH := preload("res://match/match.gd")

var match_: Match


func before_each() -> void:
	match_ = MATCH.new()
	add_child_autofree(match_)
	match_.start()
	match_.add_player(1)


func _join(id: int, key: String) -> PlayerRecord:
	match_._known_as(id, key)
	match_.add_player(id)
	return match_.state.player(id)


func test_a_player_back_within_the_window_gets_their_score_back() -> void:
	var record := _join(5, "tab-a")
	record.kills = 4
	record.deaths = 2
	record.overload = 30.0
	match_.remove_player(5)
	var back := _join(9, "tab-a")
	assert_eq(back.kills, 4)
	assert_eq(back.deaths, 2)
	assert_eq(back.overload, 30.0)


func test_their_key_arriving_after_they_join_still_counts() -> void:
	_join(5, "tab-a").kills = 3
	match_.remove_player(5)
	match_.add_player(9)
	match_._known_as(9, "tab-a")
	assert_eq(match_.state.player(9).kills, 3)


func test_someone_else_does_not_get_their_score() -> void:
	_join(5, "tab-a").kills = 4
	match_.remove_player(5)
	assert_eq(_join(9, "tab-b").kills, 0)


func test_a_score_is_only_given_back_once() -> void:
	_join(5, "tab-a").kills = 4
	match_.remove_player(5)
	_join(9, "tab-a")
	match_.remove_player(9)
	match_._departed.clear()
	assert_eq(_join(11, "tab-a").kills, 0)


func test_after_the_window_the_score_is_gone() -> void:
	_join(5, "tab-a").kills = 4
	match_.remove_player(5)
	match_._departed["tab-a"].at -= Match.REJOIN_WINDOW + 1.0
	assert_eq(_join(9, "tab-a").kills, 0)


func test_a_key_already_in_use_is_ignored() -> void:
	_join(5, "tab-a")
	match_._known_as(9, "tab-a")
	assert_false(match_._keys.has(9))


func test_overlong_or_empty_keys_are_ignored() -> void:
	match_._known_as(5, "")
	match_._known_as(6, "x".repeat(Match.MAX_KEY_LENGTH + 1))
	assert_false(match_._keys.has(5))
	assert_false(match_._keys.has(6))


func test_a_rejoining_team_player_keeps_their_team() -> void:
	match_.start(MatchSettings.from_dict({"mode": "teams"}))
	match_.add_player(1)
	var record := _join(5, "tab-a")
	var team := record.team
	match_.remove_player(5)
	_join(6, "tab-other")
	assert_eq(_join(9, "tab-a").team, team)


func test_the_key_is_the_same_each_time_on_this_machine() -> void:
	assert_eq(WebPage.player_key(), WebPage.player_key())
	assert_false(WebPage.player_key().is_empty())


func test_rejoining_is_offered_after_a_lost_connection_but_not_a_closed_session() -> void:
	var main_script := preload("res://main.gd")
	assert_true(main_script.can_rejoin("The connection to the host was lost.", "ABCD"))
	assert_false(main_script.can_rejoin("The host closed the session.", "ABCD"))
	assert_false(main_script.can_rejoin("The connection to the host was lost.", ""), "the host itself")
	assert_false(main_script.can_rejoin("", "ABCD"), "chose to leave")
