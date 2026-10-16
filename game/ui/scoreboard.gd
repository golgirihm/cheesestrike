class_name Scoreboard
extends PanelContainer
## Everyone's score, shown while the scoreboard button is held: Tab, the
## gamepad's Back or View button, or SCORES on a touch screen.

## The columns every board has, the one a mode with an objective adds, and
## the last.
const COLUMNS := ["Player", "Knock-outs", "Assists", "Times out", "Damage"]
const OBJECTIVE := "Objective"
const PING := "Ping"
const OWN_COLOR := Color(1.0, 0.85, 0.35)
## The most bars a ping is shown with, and the pings, in milliseconds, up to
## which it has four, three and two of them.
const BARS := 4
const BAR_PINGS := [60, 110, 180]

var _grid: GridContainer


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 6)
	add_child(margin)
	_grid = GridContainer.new()
	_grid.columns = COLUMNS.size() + 1
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


## The columns of the board for a match in `state`: one for the objective
## only in a mode that has one.
static func columns(state: MatchState) -> Array:
	var has_objective := state != null and (state.settings.by_points() or state.capture != null)
	return COLUMNS + ([OBJECTIVE] if has_objective else []) + [PING]


## How many bars a ping of `milliseconds` is shown with: more for a better
## connection.
static func bars(milliseconds: int) -> int:
	for index in BAR_PINGS.size():
		if milliseconds <= BAR_PINGS[index]:
			return BARS - index
	return 1


## How many bars the player with record `record` has: the host all of them,
## and a bot or someone watching none.
static func bars_for(record: PlayerRecord) -> int:
	if record.watching or Bots.is_bot(record.id):
		return 0
	return BARS if record.id == 1 else bars(record.ping)


## How a player's name reads on the board: with a note if they are talking,
## or muted by whoever is looking.
static func name_text(record: PlayerRecord, talking: bool, muted: bool) -> String:
	if muted:
		return "%s  (muted)" % record.name
	return "%s  (talking)" % record.name if talking else record.name


## Fills the board from `state`, marking the row of player `own`. `stats` is
## what each player has done (see Stats) and `voice` who is talking and who
## is muted (see VoiceChat), if those are known.
func show_state(state: MatchState, own: int, stats: Stats = null, voice: VoiceChat = null) -> void:
	for child in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	var headings := columns(state)
	_grid.columns = headings.size()
	for heading: String in headings:
		_add_cell(heading, Color(1, 1, 1, 0.6))
	var talking: Array = voice.talking() if voice != null else []
	var team := MatchState.NO_TEAM
	for record in rows(state):
		if state.settings.is_teams() and record.team != team and record.team != MatchState.NO_TEAM:
			team = record.team
			var team_color := TeamColors.of(team)
			_add_cell(MatchState.TEAM_NAMES[team], team_color)
			_add_cell(str(state.team_score(team)), team_color)
			for rest in headings.size() - 2:
				_add_cell("", team_color)
		var color := OWN_COLOR if record.id == own else Color.WHITE
		var line := stats.of(record.id) if stats != null else Stats.blank()
		_add_cell(name_text(record, talking.has(record.id), voice != null and voice.is_muted(record.id)), color)
		_add_cell(str(record.kills), color)
		_add_cell(str(line.assists), color)
		_add_cell(str(record.deaths), color)
		_add_cell(str(line.damage), color)
		if headings.has(OBJECTIVE):
			_add_cell(str(roundi(line.objective)), color)
		_add_cell(ping_text(record, state), color, bars_for(record))


## The board as text, row by row, for the browser tests.
func rows_shown() -> Array:
	var cells := []
	for child in _grid.get_children():
		cells.append(child.text)
	var shown := []
	for start in range(0, cells.size(), _grid.columns):
		shown.append(cells.slice(start, start + _grid.columns))
	return shown


## How many bars each row's ping is drawn with, row by row, leaving out the
## headings and the teams' own rows.
func bars_shown() -> Array:
	var shown := []
	for child in _grid.get_children():
		if child is PingCell:
			shown.append(child.bars)
	return shown


## A ping, with bars after it for how good the connection is.
class PingCell:
	extends Label
	var bars := 0

	func _draw() -> void:
		# Four bars, each taller than the last, the ones it has lit.
		var left := size.x - 26.0
		for index in Scoreboard.BARS:
			var tall := 4.0 + index * 3.5
			draw_rect(Rect2(left + index * 6.0, size.y - 4.0 - tall, 4.0, tall), Color(1, 1, 1, 0.9) if index < bars else Color(1, 1, 1, 0.2))


func _add_cell(text: String, color: Color, bars := -1) -> void:
	var label := Label.new() if bars < 0 else PingCell.new()
	if bars >= 0:
		(label as PingCell).bars = bars
		# Room after the words for the bars.
		label.custom_minimum_size = Vector2(96, 0)
	label.text = text
	label.add_theme_color_override("font_color", color)
	label.add_theme_font_size_override("font_size", CombatHud.text_size(16))
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_grid.add_child(label)
