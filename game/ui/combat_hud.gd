class_name CombatHud
extends Control
## What a player needs on screen in a fight: a crosshair, their health and
## ammunition, a mark when their shot hits, a red edge pointing at whoever just
## hurt them, and a notice while they are knocked out.
##
## Hit marks differ in shape as well as colour, so colour is never the only
## clue: a hit is four ticks, a head hit longer, thicker ticks, and a
## knock-out the ticks in a ring. Text is sized by the "hud_scale" setting.
##
## Drawn over the whole screen. bind() gives it the world, whose Match says what
## the host decided and who this player is.

## The least the crosshair opens. It opens as wide as the weapon's shots
## spread: wide from the hip, and tight down the sights.
const CROSSHAIR_GAP := 5.0
const CROSSHAIR_LENGTH := 9.0
const CROSSHAIR_COLOR := Color(1, 1, 1, 0.9)
const HIT_MARKER_LIFE := 0.25
const HIT_MARKER_SIZE := 11.0
const HIT_COLOR := Color(1, 1, 1)
const HEADSHOT_COLOR := Color(0.94, 0.89, 0.26)
const KNOCKOUT_COLOR := Color(0.84, 0.37, 0.0)
## The kinds of hit mark.
const HIT := "hit"
const HEADSHOT := "head"
const KNOCKOUT := "knockout"
const DAMAGE_INDICATOR_LIFE := 1.2
const DAMAGE_INDICATOR_ARC := deg_to_rad(40.0)
const DAMAGE_COLOR := Color(1.0, 0.15, 0.1, 0.8)
const LOW_HEALTH := 30

var world: Node3D
## Hit markers and damage indicators shown so far, for the browser tests.
var hits_shown := 0
var hurts_shown := 0

## The kind of the latest hit mark.
var hit_kind := ""
var captions: VBoxContainer

var _hit_left := 0.0
## Seconds a hit's damage stays written by the crosshair, drifting upward.
const DAMAGE_NUMBER_LIFE := 0.7
# Each entry: {"text": String, "left": seconds, "kind": one of the hit kinds}.
var _damage_numbers: Array[Dictionary] = []
# Each entry: {"from": Vector3, "left": seconds}.
var _indicators: Array[Dictionary] = []
var _overload_bar: ProgressBar
var _overload_label: Label
var _health_bar: ProgressBar
var _health_label: Label
var _armour_label: Label
var _ammo_label: Label
var _reserve_label: Label
var _grenade_label: Label
var _weapon_label: Label
var _pickup_label: Label
var _power_label: Label
# What everyone is told when a power-up is taken, and seconds it stays up.
var _announce_label: Label
var _announce_left := 0.0
# How long each wait that is counted aloud had left when last looked at.
var _waits := {}
const ANNOUNCE_LIFE := 3.5
var _pickup_left := 0.0
## Pickups this player has collected, for the browser tests.
var pickups_shown := 0
var _knocked_out: PanelContainer
var _knocked_out_title: Label
var _knocked_out_detail: Label
var kill_feed: VBoxContainer
var scoreboard: PanelContainer
var _match_label: Label
var _objective_label: Label
# The name of where the last player to fall was, for the feed.
var _fell_at := ""
# The name of the part of the arena this player is in.
var _place_label: Label
# What is being voted on, with a button for each choice for those without
# a keyboard.
var _vote_panel: VBoxContainer
var _vote_label: Label
var _vote_buttons: Array[Button] = []
# What the clip being shown again is.
var _replay_label: Label
# Who a player out of play is watching, and how to change it.
var _watch_label: Label
# What the tutorial asks the player to do next.
var _tutorial_label: Label
# What each weapon has done, on the practice range.
var _practice_label: Label
var _result: PanelContainer
var _result_title: Label
var _result_detail: Label
var _result_tally: Label


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)
	_build()
	Settings.changed.connect(_on_setting_changed)


## Shows the fight of the world `of`.
func bind(of: Node3D) -> void:
	world = of
	captions.world = of
	world.match_.hit_confirmed.connect(_on_hit_confirmed)
	world.match_.hurt.connect(_on_hurt)
	# Where a player falls is heard just before who did it and how.
	world.match_.fell.connect(func(victim: int, _weapon: String, _from: Vector3) -> void:
		var fallen: Node3D = world.player_node(victim)
		_fell_at = world.place_at(fallen.global_position) if fallen != null else "")
	world.match_.knocked_out.connect(func(attacker: String, victim: String, weapon: String, head: bool) -> void:
		kill_feed.add(attacker, victim, weapon, head, _fell_at)
		_fell_at = "")
	world.match_.picked_up.connect(_on_picked_up)
	world.match_.power_taken.connect(_on_power_taken)
	world.match_.announced.connect(_on_announced)
	# What the announcer says is written too, for those who can't hear it.
	world.announcer.said.connect(func(line: String, show: bool) -> void:
		if show:
			_on_announced(line, MatchState.NO_TEAM))
	world.match_.state_changed.connect(_refresh_scoreboard)


## Clears what is left from a session that has ended.
func reset() -> void:
	kill_feed.clear()
	_pickup_left = 0.0
	_indicators.clear()
	_damage_numbers.clear()
	_hit_left = 0.0
	scoreboard.hide()
	captions.clear()


## A font size of `base` at the size the player has chosen for text.
static func text_size(base: int) -> int:
	return roundi(base * Settings.get_value("hud_scale"))


## The kind of hit mark for a hit that `knocked_out` and was to the `head`.
static func hit_kind_for(knocked_out: bool, head: bool) -> String:
	if knocked_out:
		return KNOCKOUT
	return HEADSHOT if head else HIT


## How far round from straight ahead `from` lies, for a player at `at` facing
## `yaw`: 0 ahead, PI/2 to the right, -PI/2 to the left, PI behind.
static func damage_direction(at: Vector3, yaw: float, from: Vector3) -> float:
	var to := (from - at) * Vector3(1, 0, 1)
	var forward := Vector3(-sin(yaw), 0, -cos(yaw))
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	return atan2(to.dot(right), to.dot(forward))


## How far from the middle of a view `height` pixels tall, seen with a field
## of view of `fov` degrees, the edge of a cone `spread` degrees wide falls.
static func crosshair_gap(spread: float, fov: float, height: float) -> float:
	var reach := tan(deg_to_rad(spread) / 2.0) / tan(deg_to_rad(fov) / 2.0) * height / 2.0
	return clampf(reach, CROSSHAIR_GAP, height * 0.4)


## Seconds as minutes and seconds, such as "7:05".
static func clock_text(seconds: float) -> String:
	var whole := ceili(maxf(seconds, 0.0))
	return "%d:%02d" % [whole / 60, whole % 60]


## The line at the top of the screen: the score to beat, or the teams' scores,
## and the time left.
static func match_text(state: MatchState) -> String:
	if not state.is_playing():
		return "Match over"
	if state.settings.is_practice():
		return "Practice range"
	if state.is_warm_up():
		return "Warm-up   ·   waiting for players"
	var clock := clock_text(state.time_left)
	if state.overtime:
		clock = "Sudden death  %s" % clock
	if state.settings.is_teams():
		return "Cheddar %d  –  %d Brie   ·   %s" % [state.team_score(MatchState.CHEDDAR), state.team_score(MatchState.BRIE), clock]
	return "First to %d   ·   %s" % [state.settings.limit(), clock]


## What the Overload meter says: how full it is, that it's ready, or how long
## an Overload has left.
static func overload_text(record: PlayerRecord) -> String:
	if record == null:
		return ""
	if Overload.is_building(record):
		return "OVERLOAD…"
	if record.overload_left > 0.0:
		return "OVERLOAD  %ds" % ceili(record.overload_left)
	if Overload.is_ready(record):
		return "Overload ready: Q"
	return "Overload %d%%" % floori(record.overload / Overload.METER_FULL * 100.0)


static func ammo_text(weapon: WeaponState) -> String:
	if weapon.is_reloading():
		return "Reloading…"
	return "%d / %d" % [weapon.ammo, weapon.stats.magazine]


## What is left to reload with: a number, or a sign that it never runs out.
static func reserve_text(weapon: WeaponState) -> String:
	return "+ ∞" if weapon.stats.unlimited() else "+ %d" % weapon.reserve


## The notice for a player who is knocked out: who did it and when they're back.
static func knocked_out_text(state: MatchState, id: int) -> Array:
	var record := state.player(id)
	if record == null or record.alive or record.watching:
		return ["", ""]
	var by := "Knocked out by %s" % state.name_of(record.last_attacker) if state.player(record.last_attacker) != null else "Knocked out"
	if record.last_attacker == id:
		by = "You knocked yourself out"
	return [by, "Back in %d…" % ceili(record.respawn_in) if record.respawn_in > 0.0 else "Coming back…"]


## Whether the crosshair is drawn: not when aiming, when the weapon's own
## sights are what the player aims with.
func crosshair_shown() -> bool:
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if world != null and multiplayer.multiplayer_peer != null else null
	return own == null or not own.is_aiming()


func health_text() -> String:
	return _health_label.text


func ammo_shown() -> String:
	return _ammo_label.text if _ammo_label.visible else ""


## How many grenades are left, as one mark for each.
static func grenade_text(record: PlayerRecord) -> String:
	if record == null:
		return ""
	return "Grenades  " + "●".repeat(record.grenades) + "○".repeat(Weapons.GRENADES - record.grenades)


func grenades_shown() -> String:
	return _grenade_label.text


## What the host is shown of each weapon's match, or "" when it isn't.
func tally_shown() -> String:
	return _result_tally.text if _result.visible and _result_tally.visible else ""


## What is shown of a player's armour: how much, or nothing when they have none.
static func armour_text(record: PlayerRecord) -> String:
	return "+ %d rind" % record.armour if record != null and record.armour > 0 else ""


func armour_shown() -> String:
	return _armour_label.text


func reserve_shown() -> String:
	return _reserve_label.text


func knocked_out_shown() -> String:
	return _knocked_out_title.text if _knocked_out.visible else ""


func weapon_shown() -> String:
	return _weapon_label.text


func overload_shown() -> String:
	return _overload_label.text


func match_shown() -> String:
	return _match_label.text


func result_shown() -> String:
	return _result_title.text if _result.visible else ""


func indicators_showing() -> int:
	return _indicators.size()


func _process(delta: float) -> void:
	if world == null:
		return
	_count_down()
	_hit_left = maxf(_hit_left - delta, 0.0)
	for number in _damage_numbers:
		number.left -= delta
	_damage_numbers = _damage_numbers.filter(func(number: Dictionary) -> bool: return number.left > 0.0)
	_pickup_left = maxf(_pickup_left - delta, 0.0)
	_pickup_label.visible = _pickup_left > 0.0
	_announce_left = maxf(_announce_left - delta, 0.0)
	_announce_label.visible = _announce_left > 0.0
	for indicator in _indicators:
		indicator.left -= delta
	_indicators = _indicators.filter(func(indicator: Dictionary) -> bool: return indicator.left > 0.0)

	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
	var record: PlayerRecord = world.match_.own_record()
	var health := record.health if record != null else PlayerRecord.MAX_HEALTH
	_health_bar.max_value = world.match_.state.settings.health
	_health_bar.value = health
	_overload_label.text = overload_text(record)
	_overload_bar.value = record.overload if record != null else 0.0
	if record != null and record.overload_left > 0.0:
		_overload_bar.value = record.overload_left / Overload.DURATIONS[record.overload_kind] * Overload.METER_FULL
	var ready := Overload.is_ready(record) or (record != null and record.overload_left > 0.0)
	_overload_bar.modulate = Color(1.0, 0.6, 0.15) if ready else Color(1, 1, 1, 0.8)
	_health_label.text = str(health)
	_armour_label.text = armour_text(record)
	_power_label.text = Powers.text(record)
	if record != null and Powers.is_kind(record.power):
		_power_label.add_theme_color_override("font_color", Powers.COLOURS[record.power])
	_grenade_label.text = grenade_text(record)
	_health_bar.modulate = KNOCKOUT_COLOR if health <= LOW_HEALTH else Color.WHITE
	_place_label.text = world.place_at(own.global_position) if own != null and record != null and record.alive else ""
	if own != null:
		_ammo_label.text = ammo_text(own.weapon)
		_reserve_label.text = reserve_text(own.weapon)
		_reserve_label.modulate = KNOCKOUT_COLOR if own.weapon.reserve <= 0 and not own.weapon.stats.unlimited() else Color.WHITE
		_weapon_label.text = own.weapon.stats.display_name
	var state: MatchState = world.match_.state
	_match_label.text = match_text(state)
	var ballot: Ballot = state.ballot
	_vote_panel.visible = ballot != null
	if ballot != null:
		_vote_label.text = "\n".join(ballot.lines(state.name_of))
		for index in _vote_buttons.size():
			_vote_buttons[index].visible = index < ballot.options() and (_touch_shown() or Input.mouse_mode != Input.MOUSE_MODE_CAPTURED)
	_replay_label.text = world.replay_view.title
	_replay_label.visible = world.replay_view.playing
	_watch_label.text = world.watch_camera.text(_touch_shown()) if not world.replay_view.playing else ""
	_watch_label.visible = not _watch_label.text.is_empty()
	_practice_label.visible = state.settings.is_practice() and state.is_playing()
	if _practice_label.visible:
		_practice_label.text = "\n".join(world.match_.tally.practice_lines())
	_objective_label.text = state.objective_text(multiplayer.get_unique_id() if multiplayer.multiplayer_peer != null else 0)
	_result.visible = not state.is_playing()
	if _result.visible:
		_result_title.text = state.result_text()
		_result_detail.text = "Next match in %d…" % ceili(state.time_left)
		# The host sees what each weapon did, for judging their balance.
		var lines: Array[String] = world.match_.tally.lines() if world.match_.is_host() else ([] as Array[String])
		_result_tally.text = "\n".join(lines)
		_result_tally.visible = not lines.is_empty()
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


# Counts the last seconds aloud of the two waits a player watches: to come
# back after being knocked out, and to the end of the match.
func _count_down() -> void:
	var record: PlayerRecord = world.match_.own_record() if world.match_.has_state() else null
	var waits := {"respawn": record.respawn_in if record != null and not record.alive else 0.0,
		"match": world.match_.state.time_left if record != null and world.match_.state.phase == MatchState.PLAYING and not world.match_.state.is_warm_up() else 0.0}
	for wait: String in waits:
		if UiSounds.counts(_waits.get(wait, 0.0), waits[wait]):
			Sounds.play(UiSounds.COUNT, -8.0)
	_waits = waits


func _on_announced(text: String, team: int) -> void:
	_announce_label.text = text
	_announce_label.add_theme_color_override("font_color", TeamColors.of(team))
	_announce_left = ANNOUNCE_LIFE


## What is shown of the vote being held, a line at a time, or nothing.
func vote_shown() -> Array:
	return Array(_vote_label.text.split("\n", false)) if _vote_panel.visible else []


func place_shown() -> String:
	return _place_label.text


func replay_shown() -> String:
	return _replay_label.text if _replay_label.visible else ""


func watching_shown() -> String:
	return _watch_label.text if _watch_label.visible else ""


func _touch_shown() -> bool:
	var controls := get_tree().get_first_node_in_group("touch_controls") if is_inside_tree() else null
	return controls != null and controls.shown


## Shows what the tutorial asks next, or nothing for "".
func show_tutorial(text: String) -> void:
	_tutorial_label.text = text
	_tutorial_label.visible = not text.is_empty()


func tutorial_shown() -> String:
	return _tutorial_label.text if _tutorial_label.visible else ""


## What the practice range says each weapon has done, a line for each.
func practice_shown() -> Array:
	return Array(_practice_label.text.split("\n", false)) if _practice_label.visible else []


func objective_shown() -> String:
	return _objective_label.text


func _on_power_taken(id: int, power: int) -> void:
	var said := Powers.announcement(world.match_.state.name_of(id), power)
	if said.is_empty():
		return
	_announce_label.text = said
	_announce_label.add_theme_color_override("font_color", Powers.COLOURS[power])
	_announce_left = ANNOUNCE_LIFE


## What is written across the screen about a power-up just taken, or "".
func announcement_shown() -> String:
	return _announce_label.text if _announce_label.visible else ""


func power_shown() -> String:
	return _power_label.text


func _on_picked_up(id: int, kind: String) -> void:
	if multiplayer.multiplayer_peer == null or id != multiplayer.get_unique_id():
		return
	pickups_shown += 1
	_pickup_label.text = "+ %s" % Pickups.display_name(kind)
	_pickup_left = 2.0


func _on_hit_confirmed(_victim: int, _dealt: int, knocked_out: bool, head: bool) -> void:
	hits_shown += 1
	_hit_left = HIT_MARKER_LIFE
	hit_kind = hit_kind_for(knocked_out, head)
	if Settings.get_value("damage_numbers"):
		_damage_numbers.append({"text": str(_dealt), "left": DAMAGE_NUMBER_LIFE, "kind": hit_kind})
		while _damage_numbers.size() > 6:
			_damage_numbers.pop_front()


## The damage numbers showing, oldest first.
func damage_numbers_shown() -> Array[String]:
	var texts: Array[String] = []
	for number in _damage_numbers:
		texts.append(number.text)
	return texts


func _on_hurt(_attacker: int, from: Vector3, _dealt: int) -> void:
	hurts_shown += 1
	_indicators.append({"from": from, "left": DAMAGE_INDICATOR_LIFE})


## Whether the view is through a scope, with all but a circle blacked out.
func scope_shown() -> bool:
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if world != null and multiplayer.multiplayer_peer != null else null
	return own != null and own.is_scoped()


## The radius of a scope's view on a screen `view` in size.
static func scope_radius(view: Vector2) -> float:
	return minf(view.x, view.y) * 0.44


func _draw_scope(own: Node3D) -> void:
	var centre := size / 2.0
	var radius := scope_radius(size)
	# A ring wide enough to reach the corners blacks out the rest.
	var width := size.length()
	draw_arc(centre, radius + width / 2.0, 0.0, TAU, 96, Color.BLACK, width, true)
	draw_arc(centre, radius, 0.0, TAU, 96, Color(0.05, 0.05, 0.05), 6.0, true)
	for direction in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
		draw_line(centre + direction * 10.0, centre + direction * radius, Color.BLACK, 1.5, true)
	draw_circle(centre, 1.5, HEADSHOT_COLOR)
	# The breath left to hold, as an arc under the middle.
	var air: float = own.breath.air
	var arc := PI * 0.3 * air
	draw_arc(centre, radius * 0.5, PI / 2.0 - arc, PI / 2.0 + arc, 24, Color(1, 1, 1, 0.75 if own.breath.holding else 0.35), 3.0, true)


func _draw() -> void:
	if world == null or _knocked_out.visible:
		return
	var centre := size / 2.0
	var own: Node3D = world.player_node(multiplayer.get_unique_id())
	if own != null and own.is_scoped():
		_draw_scope(own)
	var gap := crosshair_gap(own.shot_spread(), own.camera.fov, size.y) if own != null else CROSSHAIR_GAP
	if crosshair_shown():
		for direction in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
			draw_line(centre + direction * gap, centre + direction * (gap + CROSSHAIR_LENGTH), CROSSHAIR_COLOR, 2.0, true)
		draw_circle(centre, 1.5, CROSSHAIR_COLOR)
	if _hit_left > 0.0:
		var color: Color = {HIT: HIT_COLOR, HEADSHOT: HEADSHOT_COLOR, KNOCKOUT: KNOCKOUT_COLOR}[hit_kind]
		color.a = _hit_left / HIT_MARKER_LIFE
		var reach := HIT_MARKER_SIZE * (1.0 if hit_kind == HIT else 1.4)
		var width := 2.5 if hit_kind == HIT else 3.5
		for corner in [Vector2(1, 1), Vector2(1, -1), Vector2(-1, 1), Vector2(-1, -1)]:
			draw_line(centre + corner * 4.0, centre + corner * reach, color, width, true)
		if hit_kind == KNOCKOUT:
			draw_arc(centre, reach + 4.0, 0.0, TAU, 32, color, 2.0, true)
	var font := get_theme_default_font()
	var font_size := text_size(18)
	for index in _damage_numbers.size():
		var number: Dictionary = _damage_numbers[index]
		var age: float = 1.0 - number.left / DAMAGE_NUMBER_LIFE
		var tint: Color = {HIT: HIT_COLOR, HEADSHOT: HEADSHOT_COLOR, KNOCKOUT: KNOCKOUT_COLOR}[number.kind]
		tint.a = 1.0 - age * age
		# Each a little to one side of the last, so a burst of hits can be read.
		var at := centre + Vector2(26.0 + (index % 3) * 22.0, -18.0 - age * 34.0)
		draw_string_outline(font, at, number.text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, 5, Color(0, 0, 0, tint.a))
		draw_string(font, at, number.text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, tint)

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


func _on_setting_changed(key: String) -> void:
	if key != "hud_scale":
		return
	for label in find_children("*", "Label", true, false):
		if label.has_meta("base_font_size"):
			label.add_theme_font_size_override("font_size", text_size(label.get_meta("base_font_size")))


func _build() -> void:
	var status := VBoxContainer.new()
	status.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT, Control.PRESET_MODE_MINSIZE, 20)
	status.grow_vertical = Control.GROW_DIRECTION_BEGIN
	status.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(status)
	_power_label = _outlined_label(18)
	status.add_child(_power_label)
	_overload_label = _outlined_label(14)
	status.add_child(_overload_label)
	_overload_bar = ProgressBar.new()
	_overload_bar.max_value = Overload.METER_FULL
	_overload_bar.show_percentage = false
	_overload_bar.custom_minimum_size = Vector2(160, 8)
	_overload_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	status.add_child(_overload_bar)
	var health := HBoxContainer.new()
	health.mouse_filter = Control.MOUSE_FILTER_IGNORE
	status.add_child(health)
	_health_bar = ProgressBar.new()
	_health_bar.max_value = PlayerRecord.MAX_HEALTH
	_health_bar.show_percentage = false
	_health_bar.custom_minimum_size = Vector2(160, 18)
	_health_bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_health_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	health.add_child(_health_bar)
	_health_label = _outlined_label(22)
	health.add_child(_health_label)
	_armour_label = _outlined_label(15)
	_armour_label.add_theme_color_override("font_color", Color(0.95, 0.6, 0.55))
	health.add_child(_armour_label)

	_announce_label = _outlined_label(26)
	_announce_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP, Control.PRESET_MODE_MINSIZE, 0)
	_announce_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_announce_label.offset_top = 96.0
	_announce_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_announce_label.visible = false
	add_child(_announce_label)

	_place_label = _outlined_label(14)
	_place_label.set_anchors_and_offsets_preset(Control.PRESET_TOP_LEFT, Control.PRESET_MODE_MINSIZE, 20)
	_place_label.offset_top = 44.0
	_place_label.add_theme_color_override("font_color", Color(1, 1, 1, 0.8))
	add_child(_place_label)
	_vote_panel = VBoxContainer.new()
	_vote_panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_LEFT, Control.PRESET_MODE_MINSIZE, 20)
	_vote_panel.grow_vertical = Control.GROW_DIRECTION_BOTH
	_vote_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_vote_panel.visible = false
	add_child(_vote_panel)
	_vote_label = _outlined_label(16)
	_vote_panel.add_child(_vote_label)
	var row := HBoxContainer.new()
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_vote_panel.add_child(row)
	for option in 3:
		var button := Button.new()
		button.name = "Vote%d" % (option + 1)
		button.text = " %d " % (option + 1)
		button.custom_minimum_size = Vector2(52, 44)
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(func() -> void:
			if world != null:
				world.match_.cast(option))
		row.add_child(button)
		_vote_buttons.append(button)
	_replay_label = _outlined_label(24)
	_replay_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP, Control.PRESET_MODE_MINSIZE, 10)
	_replay_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_replay_label.offset_top = 64.0
	_replay_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_replay_label.add_theme_color_override("font_color", Color(1.0, 0.9, 0.75))
	_replay_label.visible = false
	add_child(_replay_label)
	_watch_label = _outlined_label(18)
	_watch_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM, Control.PRESET_MODE_MINSIZE, 10)
	_watch_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_watch_label.grow_vertical = Control.GROW_DIRECTION_BEGIN
	_watch_label.offset_bottom = -96.0
	_watch_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_watch_label.visible = false
	add_child(_watch_label)
	_tutorial_label = _outlined_label(22)
	_tutorial_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP, Control.PRESET_MODE_MINSIZE, 10)
	_tutorial_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_tutorial_label.offset_top = 132.0
	_tutorial_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_tutorial_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tutorial_label.custom_minimum_size = Vector2(520, 0)
	_tutorial_label.add_theme_color_override("font_color", Color(0.75, 1.0, 0.8))
	_tutorial_label.visible = false
	add_child(_tutorial_label)
	_practice_label = _outlined_label(14)
	_practice_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_RIGHT, Control.PRESET_MODE_MINSIZE, 20)
	_practice_label.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	_practice_label.grow_vertical = Control.GROW_DIRECTION_BOTH
	_practice_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_practice_label.visible = false
	add_child(_practice_label)

	var arms := VBoxContainer.new()
	arms.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT, Control.PRESET_MODE_MINSIZE, 20)
	arms.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	arms.grow_vertical = Control.GROW_DIRECTION_BEGIN
	arms.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(arms)
	_pickup_label = _outlined_label(16)
	_pickup_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_pickup_label.add_theme_color_override("font_color", Color(1.0, 0.85, 0.35))
	_pickup_label.visible = false
	arms.add_child(_pickup_label)
	_weapon_label = _outlined_label(15)
	_weapon_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_weapon_label)
	_ammo_label = _outlined_label(22)
	_ammo_label.name = "Ammo"
	_ammo_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_ammo_label)
	_reserve_label = _outlined_label(15)
	_reserve_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_reserve_label)
	_grenade_label = _outlined_label(15)
	_grenade_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	arms.add_child(_grenade_label)

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
	_objective_label = _outlined_label(14)
	_objective_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP, Control.PRESET_MODE_MINSIZE, 10)
	_objective_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_objective_label.offset_top = 40.0
	_objective_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(_objective_label)

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
	_result_tally = _outlined_label(12)
	_result_tally.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_result_tally.visible = false
	result_rows.add_child(_result_tally)
	scoreboard = preload("res://ui/scoreboard.gd").new()
	scoreboard.visible = false
	board_rows.add_child(scoreboard)

	captions = preload("res://ui/captions.gd").new()
	captions.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM, Control.PRESET_MODE_MINSIZE, 70)
	captions.grow_horizontal = Control.GROW_DIRECTION_BOTH
	captions.grow_vertical = Control.GROW_DIRECTION_BEGIN
	add_child(captions)


static func _outlined_label(font_size: int) -> Label:
	var label := Label.new()
	label.set_meta("base_font_size", font_size)
	label.add_theme_font_size_override("font_size", text_size(font_size))
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 6)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return label
