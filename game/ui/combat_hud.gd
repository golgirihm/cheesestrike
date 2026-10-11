extends Control
## What a player needs on screen in a fight: a crosshair, their health and
## ammunition, a mark when their shot hits, a red edge pointing at whoever just
## hurt them, and a notice while they are knocked out.
##
## Drawn over the whole screen. bind() gives it the world, whose Match says what
## the host decided and who this player is.

const CROSSHAIR_GAP := 5.0
const CROSSHAIR_LENGTH := 9.0
const CROSSHAIR_COLOR := Color(1, 1, 1, 0.9)
const HIT_MARKER_LIFE := 0.25
const HIT_MARKER_SIZE := 11.0
const HIT_COLOR := Color(1, 1, 1)
const HEADSHOT_COLOR := Color(1.0, 0.8, 0.2)
const KNOCKOUT_COLOR := Color(1.0, 0.3, 0.25)
const DAMAGE_INDICATOR_LIFE := 1.2
const DAMAGE_INDICATOR_ARC := deg_to_rad(40.0)
const DAMAGE_COLOR := Color(1.0, 0.15, 0.1, 0.8)
const LOW_HEALTH := 30

var world: Node3D
## Hit markers and damage indicators shown so far, for the browser tests.
var hits_shown := 0
var hurts_shown := 0

var _hit_left := 0.0
var _hit_color := HIT_COLOR
# Each entry: {"from": Vector3, "left": seconds}.
var _indicators: Array[Dictionary] = []
var _health_bar: ProgressBar
var _health_label: Label
var _ammo_label: Label
var _weapon_label: Label
var _knocked_out: PanelContainer
var _knocked_out_title: Label
var _knocked_out_detail: Label
var kill_feed: VBoxContainer
var scoreboard: PanelContainer
var _match_label: Label
var _result: PanelContainer
var _result_title: Label
var _result_detail: Label


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build()


## Shows the fight of the world `of`.
func bind(of: Node3D) -> void:
	world = of
	world.match_.hit_confirmed.connect(_on_hit_confirmed)
	world.match_.hurt.connect(_on_hurt)
	world.match_.knocked_out.connect(kill_feed.add)
	world.match_.state_changed.connect(_refresh_scoreboard)


## Clears what is left from a session that has ended.
func reset() -> void:
	kill_feed.clear()
	_indicators.clear()
	_hit_left = 0.0
	scoreboard.hide()


## How far round from straight ahead `from` lies, for a player at `at` facing
## `yaw`: 0 ahead, PI/2 to the right, -PI/2 to the left, PI behind.
static func damage_direction(at: Vector3, yaw: float, from: Vector3) -> float:
	var to := (from - at) * Vector3(1, 0, 1)
	var forward := Vector3(-sin(yaw), 0, -cos(yaw))
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	return atan2(to.dot(right), to.dot(forward))


## Seconds as minutes and seconds, such as "7:05".
static func clock_text(seconds: float) -> String:
	var whole := ceili(maxf(seconds, 0.0))
	return "%d:%02d" % [whole / 60, whole % 60]


## The line at the top of the screen: the score to beat, or the teams' scores,
## and the time left.
static func match_text(state: MatchState) -> String:
	if not state.is_playing():
		return "Match over"
	var clock := clock_text(state.time_left)
	if state.settings.is_teams():
		return "Cheddar %d  –  %d Brie   ·   %s" % [state.team_kills[MatchState.CHEDDAR], state.team_kills[MatchState.BRIE], clock]
	return "First to %d   ·   %s" % [state.settings.kill_limit, clock]


static func ammo_text(weapon: WeaponState) -> String:
	if weapon.is_reloading():
		return "Reloading…"
	return "%d / %d" % [weapon.ammo, weapon.stats.magazine]


## The notice for a player who is knocked out: who did it and when they're back.
static func knocked_out_text(state: MatchState, id: int) -> Array:
	var record := state.player(id)
	if record == null or record.alive:
		return ["", ""]
	var by := "Knocked out by %s" % state.name_of(record.last_attacker) if state.player(record.last_attacker) != null else "Knocked out"
	return [by, "Back in %d…" % ceili(record.respawn_in) if record.respawn_in > 0.0 else "Coming back…"]


func health_text() -> String:
	return _health_label.text


func ammo_shown() -> String:
	return _ammo_label.text if _ammo_label.visible else ""


func knocked_out_shown() -> String:
	return _knocked_out_title.text if _knocked_out.visible else ""


func weapon_shown() -> String:
	return _weapon_label.text


func match_shown() -> String:
	return _match_label.text


func result_shown() -> String:
	return _result_title.text if _result.visible else ""


func indicators_showing() -> int:
	return _indicators.size()


func _process(delta: float) -> void:
	if world == null:
		return
	_hit_left = maxf(_hit_left - delta, 0.0)
	for indicator in _indicators:
		indicator.left -= delta
	_indicators = _indicators.filter(func(indicator: Dictionary) -> bool: return indicator.left > 0.0)

	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
	var record: PlayerRecord = world.match_.own_record()
	var health := record.health if record != null else PlayerRecord.MAX_HEALTH
	_health_bar.value = health
	_health_label.text = str(health)
	_health_bar.modulate = KNOCKOUT_COLOR if health <= LOW_HEALTH else Color.WHITE
	if own != null:
		_ammo_label.text = ammo_text(own.weapon)
		_weapon_label.text = own.weapon.stats.display_name
	var state: MatchState = world.match_.state
	_match_label.text = match_text(state)
	_result.visible = not state.is_playing()
	if _result.visible:
		_result_title.text = state.result_text()
		_result_detail.text = "Next match in %d…" % ceili(state.time_left)
	var wants_scores := Input.is_action_pressed("scoreboard") or _touch_held("scores") or _result.visible
	if wants_scores != scoreboard.visible:
		scoreboard.visible = wants_scores
		_refresh_scoreboard()
	var notice: Array = knocked_out_text(state, multiplayer.get_unique_id()) if record != null else ["", ""]
	_knocked_out.visible = not notice[0].is_empty() and not _result.visible
	_knocked_out_title.text = notice[0]
	_knocked_out_detail.text = notice[1]
	queue_redraw()


func _touch_held(button: String) -> bool:
	var controls := get_tree().get_first_node_in_group("touch_controls")
	return controls != null and controls.held.has(button)


func _refresh_scoreboard() -> void:
	if scoreboard.visible and world != null:
		scoreboard.show_state(world.match_.state, multiplayer.get_unique_id())


func _on_hit_confirmed(_victim: int, _dealt: int, knocked_out: bool, head: bool) -> void:
	hits_shown += 1
	_hit_left = HIT_MARKER_LIFE
	_hit_color = KNOCKOUT_COLOR if knocked_out else (HEADSHOT_COLOR if head else HIT_COLOR)


func _on_hurt(_attacker: int, from: Vector3, _dealt: int) -> void:
	hurts_shown += 1
	_indicators.append({"from": from, "left": DAMAGE_INDICATOR_LIFE})


func _draw() -> void:
	if world == null or _knocked_out.visible:
		return
	var centre := size / 2.0
	for direction in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
		draw_line(centre + direction * CROSSHAIR_GAP, centre + direction * (CROSSHAIR_GAP + CROSSHAIR_LENGTH), CROSSHAIR_COLOR, 2.0, true)
	if _hit_left > 0.0:
		var color := _hit_color
		color.a = _hit_left / HIT_MARKER_LIFE
		for corner in [Vector2(1, 1), Vector2(1, -1), Vector2(-1, 1), Vector2(-1, -1)]:
			draw_line(centre + corner * 4.0, centre + corner * HIT_MARKER_SIZE, color, 2.5, true)

	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own == null:
		return
	var radius := minf(size.x, size.y) * 0.42
	for indicator in _indicators:
		var angle := damage_direction(own.global_position, own.rotation.y, indicator.from)
		var color := DAMAGE_COLOR
		color.a *= indicator.left / DAMAGE_INDICATOR_LIFE
		# Angles here run clockwise from straight up.
		var start := angle - PI / 2.0 - DAMAGE_INDICATOR_ARC / 2.0
		draw_arc(centre, radius, start, start + DAMAGE_INDICATOR_ARC, 24, color, 10.0, true)


func _build() -> void:
	var health := HBoxContainer.new()
	health.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT, Control.PRESET_MODE_MINSIZE, 20)
	health.grow_vertical = Control.GROW_DIRECTION_BEGIN
	health.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(health)
	_health_bar = ProgressBar.new()
	_health_bar.max_value = PlayerRecord.MAX_HEALTH
	_health_bar.show_percentage = false
	_health_bar.custom_minimum_size = Vector2(160, 18)
	_health_bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_health_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	health.add_child(_health_bar)
	_health_label = _outlined_label(22)
	health.add_child(_health_label)

	var arms := VBoxContainer.new()
	arms.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT, Control.PRESET_MODE_MINSIZE, 20)
	arms.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	arms.grow_vertical = Control.GROW_DIRECTION_BEGIN
	arms.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(arms)
	_weapon_label = _outlined_label(15)
	_weapon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_weapon_label)
	_ammo_label = _outlined_label(22)
	_ammo_label.name = "Ammo"
	_ammo_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_ammo_label)

	kill_feed = preload("res://ui/kill_feed.gd").new()
	kill_feed.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT, Control.PRESET_MODE_MINSIZE, 16)
	kill_feed.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	add_child(kill_feed)

	var middle := CenterContainer.new()
	middle.set_anchors_preset(Control.PRESET_FULL_RECT)
	middle.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(middle)
	_knocked_out = PanelContainer.new()
	_knocked_out.visible = false
	_knocked_out.mouse_filter = Control.MOUSE_FILTER_IGNORE
	middle.add_child(_knocked_out)
	var rows := VBoxContainer.new()
	rows.add_theme_constant_override("separation", 6)
	_knocked_out.add_child(rows)
	_knocked_out_title = _outlined_label(24)
	_knocked_out_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	rows.add_child(_knocked_out_title)
	_knocked_out_detail = _outlined_label(18)
	_knocked_out_detail.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	rows.add_child(_knocked_out_detail)

	_match_label = _outlined_label(18)
	_match_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP, Control.PRESET_MODE_MINSIZE, 10)
	_match_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_match_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(_match_label)

	var board_holder := CenterContainer.new()
	board_holder.set_anchors_preset(Control.PRESET_FULL_RECT)
	board_holder.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(board_holder)
	var board_rows := VBoxContainer.new()
	board_rows.add_theme_constant_override("separation", 10)
	board_rows.mouse_filter = Control.MOUSE_FILTER_IGNORE
	board_holder.add_child(board_rows)
	_result = PanelContainer.new()
	_result.visible = false
	_result.mouse_filter = Control.MOUSE_FILTER_IGNORE
	board_rows.add_child(_result)
	var result_rows := VBoxContainer.new()
	_result.add_child(result_rows)
	_result_title = _outlined_label(30)
	_result_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	result_rows.add_child(_result_title)
	_result_detail = _outlined_label(16)
	_result_detail.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	result_rows.add_child(_result_detail)
	scoreboard = preload("res://ui/scoreboard.gd").new()
	scoreboard.visible = false
	board_rows.add_child(scoreboard)


static func _outlined_label(font_size: int) -> Label:
	var label := Label.new()
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 6)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return label
