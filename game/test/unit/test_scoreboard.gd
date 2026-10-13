extends GutTest

const BOARD := preload("res://ui/scoreboard.gd")

var state: MatchState


func before_each() -> void:
	state = MatchState.new()


func _player(id: int, name: String, kills: int, deaths: int) -> void:
	# Past the warm-up, in which nothing counts.
	state.started = true
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
	assert_eq(board.rows_shown(), [BOARD.COLUMNS, ["Brie", "2", "1", "host"], ["Edam", "0", "2", "48 ms"]])
	state.player(2).kills = 7
	board.show_state(state, 1)
	assert_eq(board.rows_shown()[1], ["Edam", "7", "2", "48 ms"], "refilled, not added to")
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
	assert_eq(shown[1], ["Cheddar", "3", "", ""])
	assert_eq(shown.size(), 1 + 2 + 4, "a heading, two teams and four players")
	var brie_at := shown.find(["Brie", "5", "", ""])
	assert_eq(brie_at, 4, "Cheddar's two players come before Brie")


func test_teams_come_first_in_the_rows() -> void:
	state = MatchState.new(MatchSettings.from_dict({"mode": "teams"}))
	for id in [1, 2]:
		state.add_player(id)
	state.player(2).kills = 9
	var rows := BOARD.rows(state)
	assert_eq(rows[0].team, MatchState.CHEDDAR)
