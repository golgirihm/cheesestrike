extends GutTest

const BOARD := preload("res://ui/scoreboard.gd")

var state: MatchState


func before_each() -> void:
	state = MatchState.new()


func _player(id: int, name: String, kills: int, deaths: int) -> void:
	var record := state.add_player(id)
	record.name = name
	record.kills = kills
	record.deaths = deaths


func _names(rows: Array[PlayerRecord]) -> Array:
	return rows.map(func(record: PlayerRecord) -> String: return record.name)


func test_most_knock_outs_comes_first() -> void:
	_player(1, "Brie", 2, 0)
	_player(2, "Edam", 5, 3)
	_player(3, "Feta", 0, 0)
	assert_eq(_names(BOARD.rows(state)), ["Edam", "Brie", "Feta"])


func test_level_on_knock_outs_fewer_times_out_comes_first() -> void:
	_player(1, "Brie", 3, 4)
	_player(2, "Edam", 3, 1)
	assert_eq(_names(BOARD.rows(state)), ["Edam", "Brie"])


func test_level_on_both_goes_by_name() -> void:
	_player(1, "gouda", 1, 1)
	_player(2, "Brie", 1, 1)
	_player(3, "Cheddar 10", 1, 1)
	_player(4, "Cheddar 9", 1, 1)
	assert_eq(_names(BOARD.rows(state)), ["Brie", "Cheddar 9", "Cheddar 10", "gouda"])


func test_an_empty_session_has_no_rows() -> void:
	assert_eq(BOARD.rows(state).size(), 0)


func test_the_board_shows_a_heading_and_a_row_per_player() -> void:
	_player(1, "Brie", 2, 1)
	_player(2, "Edam", 0, 2)
	var board: PanelContainer = BOARD.new()
	add_child_autofree(board)
	board.show_state(state, 1)
	state.player(1).ping = 0
	state.player(2).ping = 48
	board.show_state(state, 1)
	assert_eq(board.rows_shown(), [BOARD.columns(state), ["Brie", "2", "0", "1", "0", "host"], ["Edam", "0", "0", "2", "0", "48 ms"]])
	state.player(2).kills = 7
	board.show_state(state, 1)
	assert_eq(board.rows_shown()[1], ["Edam", "7", "0", "2", "0", "48 ms"], "refilled, not added to")
	assert_eq(board.rows_shown().size(), 3)


func test_a_team_match_lists_each_team_under_its_score() -> void:
	state = MatchState.new(MatchSettings.from_dict({"mode": "teams"}))
	for id in [1, 2, 3, 4]:
		state.add_player(id)
		state.player(id).name = "P%d" % id
	state.team_kills = [3, 5]
	var board: PanelContainer = BOARD.new()
	add_child_autofree(board)
	board.show_state(state, 1)
	var shown: Array = board.rows_shown()
	assert_eq(shown[1], ["Cheddar", "3", "", "", "", ""])
	assert_eq(shown.size(), 1 + 2 + 4, "a heading, two teams and four players")
	var brie_at := shown.find(["Brie", "5", "", "", "", ""])
	assert_eq(brie_at, 4, "Cheddar's two players come before Brie")


func test_teams_come_first_in_the_rows() -> void:
	state = MatchState.new(MatchSettings.from_dict({"mode": "teams"}))
	for id in [1, 2]:
		state.add_player(id)
	state.player(2).kills = 9
	var rows := BOARD.rows(state)
	assert_eq(rows[0].team, MatchState.CHEDDAR)


func test_the_columns_are_the_score_what_was_done_and_the_ping() -> void:
	assert_eq(BOARD.columns(state), ["Player", "Knock-outs", "Assists", "Times out", "Damage", "Ping"])
	assert_eq(BOARD.columns(null), BOARD.columns(state))
	# A mode with an objective has a column for it; the others don't.
	for mode: String in MatchSettings.MODES:
		var of_mode := MatchState.new(MatchSettings.from_dict({"mode": mode}))
		var has: bool = of_mode.settings.by_points() or of_mode.capture != null
		assert_eq(BOARD.columns(of_mode).has(BOARD.OBJECTIVE), has, mode)
		assert_eq(BOARD.columns(of_mode)[-1], BOARD.PING, mode)
	assert_true(BOARD.columns(MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.HILL}))).has(BOARD.OBJECTIVE))


func test_each_row_shows_what_the_player_has_done() -> void:
	_player(1, "Brie", 2, 1)
	_player(2, "Edam", 0, 2)
	var stats := Stats.new()
	stats.hit(1, 2, 60, "blaster", false, 1.0)
	stats.hit(2, 1, 35, "pistol", false, 2.0)
	stats.hit(1, 2, 40, "blaster", true, 3.0)
	var third := Stats.new()
	third.hit(2, 3, 10, "pistol", false, 1.0)
	third.hit(1, 3, 90, "blaster", true, 2.0)
	var board: Scoreboard = BOARD.new()
	add_child_autofree(board)
	board.show_state(state, 1, stats)
	assert_eq(board.rows_shown()[1].slice(0, 5), ["Brie", "2", "0", "1", "100"])
	assert_eq(board.rows_shown()[2].slice(0, 5), ["Edam", "0", "0", "2", "35"])
	board.show_state(state, 1, third)
	assert_eq(board.rows_shown()[2][2], "1", "an assist")
	# With an objective, what was done for it.
	state = MatchState.new(MatchSettings.from_dict({"mode": MatchSettings.HILL}))
	state.add_player(1)
	var held := Stats.new()
	held.objective(1, 12.6)
	board.show_state(state, 1, held)
	var at: int = BOARD.columns(state).find(BOARD.OBJECTIVE)
	assert_eq(board.rows_shown()[2][at], "13")
	assert_eq(board.rows_shown()[0].size(), 7)
	assert_eq(board.rows_shown()[1].size(), 7, "the team's own row is as wide")


func test_a_ping_is_shown_as_bars_more_for_a_better_one() -> void:
	assert_eq(BOARD.bars(0), 4)
	assert_eq(BOARD.bars(60), 4)
	assert_eq(BOARD.bars(61), 3)
	assert_eq(BOARD.bars(110), 3)
	assert_eq(BOARD.bars(150), 2)
	assert_eq(BOARD.bars(181), 1)
	assert_eq(BOARD.bars(5000), 1, "never none: they are still there")
	_player(1, "Brie", 0, 0)
	_player(2, "Edam", 0, 0)
	_player(3, "Feta", 0, 0)
	state.player(2).ping = 48
	state.player(3).ping = 240
	assert_eq(BOARD.bars_for(state.player(1)), BOARD.BARS, "the host has no way to go")
	state.player(3).watching = true
	assert_eq(BOARD.bars_for(state.player(3)), 0)
	state.player(3).watching = false
	var bot := PlayerRecord.new(-2)
	assert_eq(BOARD.bars_for(bot), 0)
	var board: Scoreboard = BOARD.new()
	add_child_autofree(board)
	board.show_state(state, 1)
	assert_eq(board.bars_shown(), [4, 4, 1], "one for each player's row")


func test_who_is_talking_or_muted_is_marked_by_their_name() -> void:
	_player(1, "Brie", 0, 0)
	assert_eq(BOARD.name_text(state.player(1), false, false), "Brie")
	assert_eq(BOARD.name_text(state.player(1), true, false), "Brie  (talking)")
	assert_eq(BOARD.name_text(state.player(1), false, true), "Brie  (muted)")
	assert_eq(BOARD.name_text(state.player(1), true, true), "Brie  (muted)", "muted is what matters to whoever muted them")
	_player(2, "Edam", 0, 0)
	_player(3, "Feta", 0, 0)
	var voice := VoiceChat.new()
	add_child_autofree(voice)
	voice.set_process(false)
	var packet := PackedByteArray([128, 140, 120])
	var wanted: bool = Settings.get_value(VoiceChat.SETTING)
	Settings.set_value(VoiceChat.SETTING, true)
	voice.hear(2, packet)
	voice.set_muted(3, true)
	var board: Scoreboard = BOARD.new()
	add_child_autofree(board)
	board.show_state(state, 1, null, voice)
	assert_eq(board.rows_shown().map(func(row: Array) -> String: return row[0]).slice(1), ["Brie", "Edam  (talking)", "Feta  (muted)"])
	Settings.set_value(VoiceChat.SETTING, wanted)
