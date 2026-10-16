class_name ProfileScreen
extends Control
## The player's own page: who they are, their level, and what they have
## done, a part at a time: their record, and their past matches.

signal closed

const RECORD := "record"
const MATCHES := "matches"
## The parts of the page, with what the button for each says.
const CHALLENGES := "challenges"
const ACHIEVEMENTS := "achievements"
const UNLOCKS := "unlocks"
const PARTS := {RECORD: "Record", CHALLENGES: "Challenges", ACHIEVEMENTS: "Feats", UNLOCKS: "To earn", MATCHES: "Matches"}
const WIDE := 440.0

## The part showing.
var part := RECORD
var _name: Label
var _rank: Label
var _bar: ProgressBar
var _level: Label
var _body: Label
var _tabs := {}
var _scroll: ScrollContainer


func _init() -> void:
	name = "ProfileScreen"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	theme = Look.theme()
	visible = false
	var centre := CenterContainer.new()
	centre.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(centre)
	var panel := PanelContainer.new()
	panel.name = "Panel"
	panel.custom_minimum_size = Vector2(WIDE, 0)
	centre.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 8)
	panel.add_child(margin)
	var rows := VBoxContainer.new()
	rows.add_theme_constant_override("separation", 8)
	margin.add_child(rows)
	_name = Label.new()
	_name.name = "ProfileName"
	_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	Look.heading(_name)
	rows.add_child(_name)
	_rank = Label.new()
	_rank.name = "ProfileRank"
	_rank.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	rows.add_child(_rank)
	_bar = ProgressBar.new()
	_bar.name = "LevelBar"
	_bar.max_value = 1.0
	_bar.step = 0.001
	_bar.show_percentage = false
	_bar.custom_minimum_size = Vector2(0, 14)
	rows.add_child(_bar)
	_level = Label.new()
	_level.name = "LevelText"
	_level.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_level.add_theme_color_override("font_color", Look.FADED)
	rows.add_child(_level)
	var tabs := HBoxContainer.new()
	rows.add_child(tabs)
	for id: String in PARTS:
		var tab := Button.new()
		tab.name = "Profile_" + id
		tab.text = PARTS[id]
		tab.toggle_mode = true
		tab.custom_minimum_size = Vector2(0, 40)
		tab.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		tab.pressed.connect(show_part.bind(id))
		tabs.add_child(tab)
		_tabs[id] = tab
	_scroll = ScrollContainer.new()
	_scroll.custom_minimum_size = Vector2(WIDE - 40.0, 250)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	rows.add_child(_scroll)
	_body = Label.new()
	_body.name = "ProfileBody"
	_body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_body.custom_minimum_size = Vector2(WIDE - 52.0, 0)
	_body.add_theme_font_size_override("font_size", 14)
	_scroll.add_child(_body)
	var done := Button.new()
	done.name = "ProfileDone"
	done.text = "Done"
	done.custom_minimum_size = Vector2(0, 44)
	done.pressed.connect(close)
	rows.add_child(done)


## What the part called `id` says, a line at a time, of what is kept in
## `progress` (see Progress).
static func lines(id: String, progress: Node) -> Array[String]:
	if id == MATCHES:
		var said: Array[String] = []
		for report: Dictionary in progress.history:
			said.append("%s\n     %s" % [MatchReport.line(report), when_text(report.when, int(Time.get_unix_time_from_system()))])
		return said if not said.is_empty() else (["No matches yet. Play one, and it will be here."] as Array[String])
	if id == CHALLENGES:
		return Challenges.lines(progress.challenges, progress.now())
	if id == ACHIEVEMENTS:
		return Achievements.lines(progress.achievements)
	if id == UNLOCKS:
		return Unlocks.lines(progress.level(), progress.achievements, progress.challenges_done())
	return Record.lines(progress.record)


## How long ago `when` was, as of `now`, both in seconds since 1970.
static func when_text(when: int, now: int) -> String:
	var ago := maxi(now - when, 0)
	if ago < 90:
		return "just now"
	if ago < 3600:
		return "%d minutes ago" % (ago / 60)
	if ago < 86400 * 2:
		return "%d hours ago" % maxi(ago / 3600, 1)
	return "%d days ago" % (ago / 86400)


## Opens the page.
func open() -> void:
	show_part(part)
	Look.arrive(self)


func close() -> void:
	hide()
	closed.emit()


## Shows the part called `id`, and the player as they now are above it.
func show_part(id: String) -> void:
	part = id if PARTS.has(id) else RECORD
	_name.text = Settings.get_value("name")
	var title: String = Settings.get_value(Looks.TITLE_SETTING)
	_rank.text = Looks.TITLE_NAMES[title] if title != Looks.TITLES[0] else "%s cheese" % Looks.CHEESE_NAMES[Settings.get_value("look_cheese")]
	_bar.value = Levels.progress(Progress.xp)
	_level.text = Levels.text(Progress.xp)
	for tab: String in _tabs:
		(_tabs[tab] as Button).set_pressed_no_signal(tab == part)
	_body.text = "\n".join(lines(part, Progress))
	_scroll.scroll_vertical = 0


## What the page is showing: {"name", "rank", "level", "part", "lines"}.
func showing() -> Dictionary:
	return {"name": _name.text, "rank": _rank.text, "level": _level.text, "part": part, "lines": Array(_body.text.split("\n", false))}
