extends PanelContainer
## Everyone's score, shown while the scoreboard button is held: Tab, the
## gamepad's Back or View button, or SCORES on a touch screen.

const COLUMNS := ["Player", "Knock-outs", "Times out", "Ping"]
const OWN_COLOR := Color(1.0, 0.85, 0.35)

var _grid: GridContainer


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 18)
	add_child(margin)
	_grid = GridContainer.new()
	_grid.columns = COLUMNS.size()
	_grid.add_theme_constant_override("h_separation", 28)
	_grid.add_theme_constant_override("v_separation", 6)
	margin.add_child(_grid)


## The rows of the scoreboard, best first: most knock-outs, then fewest times
## knocked out, then by name. In a team match, team by team, Cheddar first.
static func rows(state: MatchState) -> Array[PlayerRecord]:
	var sorted: Array[PlayerRecord] = []
	for record: PlayerRecord in state.players.values():
		sorted.append(record)
	sorted.sort_custom(func(a: PlayerRecord, b: PlayerRecord) -> bool:
		# Those only watching come last.
		if a.watching != b.watching:
			return b.watching
		if a.team != b.team:
			return a.team < b.team
		if a.kills != b.kills:
			return a.kills > b.kills
		if a.deaths != b.deaths:
			return a.deaths < b.deaths
		return a.name.naturalnocasecmp_to(b.name) < 0)
	return sorted


## How a player's ping reads: "host" for the host, which has none.
static func ping_text(record: PlayerRecord, _state: MatchState) -> String:
	if record.watching:
		return "watching"
	if Bots.is_bot(record.id):
		return "bot"
	return "host" if record.id == 1 else "%d ms" % record.ping


## Fills the board from `state`, marking the row of player `own`.
func show_state(state: MatchState, own: int) -> void:
	for child in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	for heading: String in COLUMNS:
		_add_cell(heading, Color(1, 1, 1, 0.6))
	var team := MatchState.NO_TEAM
	for record in rows(state):
		if state.settings.is_teams() and record.team != team and record.team != MatchState.NO_TEAM:
			team = record.team
			var team_color := TeamColors.of(team)
			_add_cell(MatchState.TEAM_NAMES[team], team_color)
			_add_cell(str(state.team_score(team)), team_color)
			_add_cell("", team_color)
			_add_cell("", team_color)
		var color := OWN_COLOR if record.id == own else Color.WHITE
		_add_cell(record.name, color)
		_add_cell(str(record.kills), color)
		_add_cell(str(record.deaths), color)
		_add_cell(ping_text(record, state), color)


## The board as text, row by row, for the browser tests.
func rows_shown() -> Array:
	var cells := []
	for child in _grid.get_children():
		cells.append(child.text)
	var shown := []
	for start in range(0, cells.size(), COLUMNS.size()):
		shown.append(cells.slice(start, start + COLUMNS.size()))
	return shown


func _add_cell(text: String, color: Color) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", color)
	label.add_theme_font_size_override("font_size", CombatHud.text_size(16))
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_grid.add_child(label)
