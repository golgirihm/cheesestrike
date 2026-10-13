extends GutTest


func test_the_defaults_are_a_free_for_all_to_20_in_8_minutes_for_16() -> void:
	var settings := MatchSettings.new()
	assert_eq(settings.mode, MatchSettings.FREE_FOR_ALL)
	assert_eq(settings.kill_limit, 20)
	assert_eq(settings.minutes, 8)
	assert_eq(settings.max_players, 16)
	assert_false(settings.is_teams())


func test_settings_survive_the_trip() -> void:
	var settings := MatchSettings.from_dict({"mode": "teams", "kill_limit": 30, "minutes": 5, "max_players": 6, "map": "box", "bots": 3, "bot_skill": "hard"})
	var copy := MatchSettings.from_dict(bytes_to_var(var_to_bytes(settings.to_dict())))
	assert_eq(copy.to_dict(), {"mode": "teams", "kill_limit": 30, "minutes": 5, "max_players": 6, "map": "box", "bots": 3, "bot_skill": "hard"})
	assert_true(copy.is_teams())


func test_numbers_from_json_arrive_as_decimals_and_are_fine() -> void:
	var settings := MatchSettings.from_dict(JSON.parse_string('{"kill_limit": 10, "minutes": 3, "max_players": 4}'))
	assert_eq(settings.kill_limit, 10)
	assert_eq(settings.max_players, 4)


func test_out_of_range_numbers_are_brought_into_range() -> void:
	var settings := MatchSettings.from_dict({"kill_limit": -5, "minutes": 9999, "max_players": 1})
	assert_eq(settings.kill_limit, 1)
	assert_eq(settings.minutes, 60)
	assert_eq(settings.max_players, MatchSettings.MIN_PLAYERS)
	assert_eq(MatchSettings.from_dict({"max_players": 64}).max_players, MatchSettings.MAX_PLAYERS)


func test_nonsense_gives_the_defaults() -> void:
	var settings := MatchSettings.from_dict({"mode": "tag", "kill_limit": "ten", "minutes": NAN, "max_players": null, "map": "moon"})
	assert_eq(settings.to_dict(), MatchSettings.new().to_dict())
	assert_eq(MatchSettings.from_dict("teams").to_dict(), MatchSettings.new().to_dict())
	assert_eq(MatchSettings.from_dict(null).to_dict(), MatchSettings.new().to_dict())


func test_every_menu_choice_is_within_range() -> void:
	for limit in MatchSettings.KILL_LIMITS:
		assert_eq(MatchSettings.from_dict({"kill_limit": limit}).kill_limit, limit)
	for minutes in MatchSettings.MINUTES:
		assert_eq(MatchSettings.from_dict({"minutes": minutes}).minutes, minutes)


func test_the_description_says_the_mode_goal_and_length() -> void:
	assert_eq(MatchSettings.new().describe(), "Free-for-all · first to 20 · 8 min · Cheddar Canyon")
	var teams := MatchSettings.from_dict({"mode": "teams", "kill_limit": 50, "minutes": 15, "map": "box"})
	assert_eq(teams.describe(), "Cheddar vs Brie · first to 50 · 15 min · Brie Box")


func test_the_server_and_the_game_agree_on_the_defaults() -> void:
	# signaling/server.py keeps its own copy of these, as DEFAULT_SETTINGS.
	var server := FileAccess.get_file_as_string(ProjectSettings.globalize_path("res://").path_join("../signaling/server.py"))
	if server.is_empty():
		pass_test("the server's source isn't reachable from here")
		return
	var defaults := MatchSettings.new()
	assert_string_contains(server, '"mode": "%s", "kill_limit": %d, "minutes": %d, "max_players": MAX_PLAYERS, "map": "%s"' % [defaults.mode, defaults.kill_limit, defaults.minutes, defaults.map])
	# It knows the first arenas, and is told one of them whatever is played.
	assert_string_contains(server, 'MAPS = ("%s")' % '", "'.join(Maps.LISTED))
	for id: String in Maps.ORDER:
		assert_has(Maps.LISTED, MatchSettings.from_dict({"map": id}).advertised().map, id)
		assert_eq(MatchSettings.from_dict({"map": id}).to_dict().map, id, "and players are told the real one")
