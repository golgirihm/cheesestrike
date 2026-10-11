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
	assert_eq(board.rows_shown(), [BOARD.COLUMNS, ["Brie", "2", "1"], ["Edam", "0", "2"]])
	state.player(2).kills = 7
	board.show_state(state, 1)
	assert_eq(board.rows_shown()[1], ["Edam", "7", "2"], "refilled, not added to")
	assert_eq(board.rows_shown().size(), 3)
