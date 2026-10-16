extends CenterContainer
## The settings screen: how looking feels, the field of view, loudness,
## graphics quality and the keys, and the character and weapons. Every change
## is saved and takes effect at once. Reached from the main menu and the
## in-game menu, over either.
##
## The settings are in groups, each under a tab across the top, and each
## setting is a row: what it is called on the left, and on the right a
## slider, a switch or a list of its choices.
##
## Each control is named after what it sets ("Setting_fov",
## "Key_move_forward"), and each tab after its group ("SettingsTab_sound"),
## which the browser tests use to find them.

signal closed

const SLIDERS := [
	["mouse_sensitivity", "Mouse look", 0.1, 4.0, 0.05, "%.2f×"],
	["stick_sensitivity", "Stick look", 0.1, 4.0, 0.05, "%.2f×"],
	["stick_dead_zone", "Stick dead zone", 0.0, 0.5, 0.01, "%d%%"],
	["touch_sensitivity", "Touch look", 0.1, 4.0, 0.05, "%.2f×"],
	["aim_sensitivity", "Look while aiming", 0.2, 2.0, 0.05, "%.2f×"],
	["scope_sensitivity", "Look through a scope", 0.2, 2.0, 0.05, "%.2f×"],
	["look_vertical", "Look up and down, against across", 0.2, 2.0, 0.05, "%.2f×"],
	["gyro_sensitivity", "Tilt to aim", 0.2, 3.0, 0.05, "%.2f×"],
	["fov", "Field of view", 60.0, 110.0, 1.0, "%d°"],
	["master_volume", "Volume", 0.0, 1.0, 0.05, "%d%%"],
	["music_volume", "Music", 0.0, 1.0, 0.05, "%d%%"],
	["effects_volume", "Effects", 0.0, 1.0, 0.05, "%d%%"],
	["weapons_volume", "Effects: weapons", 0.0, 1.0, 0.05, "%d%%"],
	["steps_volume", "Effects: footsteps", 0.0, 1.0, 0.05, "%d%%"],
	["voices_volume", "Effects: voices", 0.0, 1.0, 0.05, "%d%%"],
	["world_volume", "Effects: the arena", 0.0, 1.0, 0.05, "%d%%"],
	["interface_volume", "Hits and announcements", 0.0, 1.0, 0.05, "%d%%"],
	["touch_size", "Touch buttons", 0.7, 1.5, 0.05, "%.2f×"],
	["hud_scale", "Text size", 0.75, 1.5, 0.05, "%.2f×"],
	["menu_text", "Text size in menus", 1.0, 1.25, 0.05, "%.2f×"],
	["screen_shake", "Screen shake", 0.0, 1.0, 0.05, "%d%%"],
	["camera_motion", "Camera motion", 0.0, 1.0, 0.05, "%d%%"],
	["crosshair_size", "Crosshair: length", 2.0, 24.0, 1.0, "%d px"],
	["crosshair_gap", "Crosshair: gap", 0.0, 16.0, 1.0, "%d px"],
	["crosshair_thickness", "Crosshair: thickness", 1.0, 6.0, 1.0, "%d px"],
]
## The settings that are on or off, with what each says.
const TOGGLES := [
	["invert_look", "Invert looking up and down"],
	["high_contrast", "High contrast"],
	["less_motion", "Less motion: no shake, bob or sway, and softer flashes"],
	["aim_assist", "Gamepad: slow the view over an opponent"],
	["rumble", "Gamepad: rumble"],
	["haptics", "Phone: buzz when hurt"],
	["gyro_aim", "Aim by turning the phone"],
	["touch_left_handed", "Touch buttons on the left"],
	["touch_fullscreen", "Full screen when touched"],
	["captions", "Captions for sounds"],
	["sound_ring", "Show where sounds come from, round the crosshair"],
	["toggle_run", "Press once to run, again to stop"],
	["toggle_crouch", "Press once to crouch, again to stand"],
	["toggle_aim", "Press once to aim, again to stop"],
	["toggle_lean", "Press once to lean, again to stop"],
	["toggle_talk", "Press once to talk, again to stop"],
	["toggle_scoreboard", "Press once for the scoreboard, again to hide it"],
	["hold_to_fire", "Hold to keep firing a one-shot weapon"],
	["battery_saver", "Save battery: 30 frames a second at most"],
	["damage_numbers", "Show the damage of each hit"],
	["replays", "Show how I was knocked out, and the play of the match"],
	["show_fps", "Show frames a second"],
	["show_ping", "Show the ping"],
	["show_loss", "Show what goes astray on the way"],
	["show_graph", "Show a graph of each frame"],
	["crosshair_outline", "Crosshair: dark edge"],
	["crosshair_dot", "Crosshair: dot in the middle"],
	["mono_sound", "Mono sound"],
	["announcer", "The announcer's voice"],
	["voice_chat", "Talk with teammates (hold the Talk key)"],
	["text_chat", "Text chat"],
	["outlines", "Outline teammates, and make pickups glow"],
	["dynamic_resolution", "Lower the render scale to hold the frame rate"],
	["bloom", "Glow on bright things"],
	["colour_grade", "A cast of colour for each arena"],
	["vignette", "Darker corners of the view"],
]

## The parts of this screen that can be shown on their own: the character
## and weapons, as "Customise", and the rest, as "Settings".
const CUSTOMISE := "customise"
const SETTINGS := "settings"
const EVERYTHING := ""

## The groups of settings, each with what its tab says, in the order of the
## tabs: first those of "Settings", then those of "Customise".
const TABS := [
	["aim", "Aim"], ["controls", "Controls"], ["keys", "Keys"], ["sound", "Sound"], ["graphics", "Graphics"], ["hud", "HUD"], ["access", "Accessibility"],
	["character", "Character"], ["weapons", "Weapons"],
]
const CUSTOMISE_TABS := ["character", "weapons"]
## Which group each setting is in, by its key, in the order each group
## lists them.
const GROUP_OF := {
	"mouse_sensitivity": "aim", "stick_sensitivity": "aim", "stick_dead_zone": "aim", "touch_sensitivity": "aim", "aim_sensitivity": "aim",
	"scope_sensitivity": "aim", "look_vertical": "aim", "gyro_sensitivity": "aim", "fov": "aim", "invert_look": "aim", "aim_assist": "aim",
	"gyro_aim": "aim", StickAim.CURVE: "aim",
	"toggle_run": "controls", "toggle_crouch": "controls", "toggle_aim": "controls", "toggle_lean": "controls", "toggle_talk": "controls",
	"toggle_scoreboard": "controls", "hold_to_fire": "controls", "rumble": "controls", "haptics": "controls", "touch_left_handed": "controls",
	"touch_fullscreen": "controls", "touch_size": "controls",
	"master_volume": "sound", "music_volume": "sound", "effects_volume": "sound", "weapons_volume": "sound", "steps_volume": "sound",
	"voices_volume": "sound", "world_volume": "sound", "interface_volume": "sound", "mono_sound": "sound", "announcer": "sound",
	"voice_chat": "sound", "text_chat": "sound", Mix.RANGE_SETTING: "sound", SoundCues.HIT_SETTING: "sound",
	"battery_saver": "graphics", "dynamic_resolution": "graphics", "bloom": "graphics", "colour_grade": "graphics", "vignette": "graphics",
	"show_fps": "graphics", "show_ping": "graphics", "show_loss": "graphics", "show_graph": "graphics", FramePace.CAP: "graphics",
	"hud_scale": "hud", "damage_numbers": "hud", "replays": "hud", "outlines": "hud", Crosshair.SHAPE: "hud", Crosshair.COLOUR: "hud",
	"crosshair_size": "hud", "crosshair_gap": "hud", "crosshair_thickness": "hud", "crosshair_outline": "hud", "crosshair_dot": "hud",
	"high_contrast": "access", "less_motion": "access", "menu_text": "access", "screen_shake": "access", "camera_motion": "access",
	"captions": "access", "sound_ring": "access", TeamColors.PRESET: "access", TeamColors.OWN_CHEDDAR: "access", TeamColors.OWN_BRIE: "access",
	TeamColors.OWN_HIT: "access",
}

var _title: Label
## The part of the HUD being laid out, and the controls that do it.
var hud_part: String = HudLayout.PARTS.keys()[0]
const HUD_SLIDERS := [[HudLayout.X, "Across"], [HudLayout.Y, "Down"], [HudLayout.SCALE, "Size"], [HudLayout.OPACITY, "Opacity"]]
var _hud_part_button: OptionButton
var _hud_sliders := {}
var _hud_on: CheckButton
var _crosshair_code: LineEdit
var _crosshair_view: Control
var _scroll: ScrollContainer
var _done: Button
var _value_labels := {}
var _key_buttons := {}
# Each group's rows, and its tab, by the group's id; and the group shown.
var _groups := {}
var _tabs := {}
var _tab_row: HFlowContainer
## The group of settings being shown.
var group := "aim"
# Each list of choices: {"list": OptionButton, "ids": [...], "name": Callable
# (id) -> what it is called, "can": Callable (id) -> whether it can be
# chosen, "now": Callable () -> the id chosen}, by its control's name.
var _lists := {}
# The action waiting for its new key, or "".
var _waiting_for := ""


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	visible = false
	add_to_group("touch_blockers")
	_build()
	Settings.changed.connect(func(_key: String) -> void: refresh())
	refresh()


## Opens the screen with everything on it, as from the in-game menu.
func open() -> void:
	open_part(EVERYTHING)


## Opens the screen at one part of itself: CUSTOMISE for the character and
## weapons alone, SETTINGS for the rest, EVERYTHING for both.
func open_part(part: String) -> void:
	refresh()
	show()
	_done.grab_focus()
	_title.text = "Customise" if part == CUSTOMISE else "Settings"
	var first := ""
	for tab: Array in TABS:
		var shown: bool = part == EVERYTHING or (CUSTOMISE_TABS.has(tab[0]) == (part == CUSTOMISE))
		(_tabs[tab[0]] as Button).visible = shown
		if shown and first.is_empty():
			first = tab[0]
	show_group(first)


## Shows the group of settings `id` (see TABS), under its tab.
func show_group(id: String) -> void:
	group = id
	for each: String in _groups:
		(_groups[each] as Control).visible = each == id
		(_tabs[each] as Button).set_pressed_no_signal(each == id)
	_scroll.scroll_vertical = 0


## The group that the setting `key` is in, or "" if it has no control here.
func group_of(key: String) -> String:
	var control := find_child("Setting_" + key, true, false)
	if control == null:
		return ""
	for id: String in _groups:
		if (_groups[id] as Node).is_ancestor_of(control):
			return id
	return ""


func close() -> void:
	_waiting_for = ""
	hide()
	closed.emit()


## Brings every control in line with the settings as they are.
func refresh() -> void:
	for row: Array in SLIDERS:
		var slider: HSlider = find_child("Setting_" + row[0], true, false)
		if slider != null:
			slider.set_value_no_signal(Settings.get_value(row[0]))
		_show_value(row[0])
	for row: Array in TOGGLES:
		(find_child("Setting_" + row[0], true, false) as CheckButton).set_pressed_no_signal(Settings.get_value(row[0]))
	for id: String in _lists:
		_show_list(id)
	# The HUD's controls show the part that is chosen.
	if _hud_part_button != null:
		var how := HudLayout.part(Settings.get_value(HudLayout.SETTING), hud_part)
		for key: String in _hud_sliders:
			(_hud_sliders[key] as HSlider).set_value_no_signal(how[key])
		_hud_on.set_pressed_no_signal(how[HudLayout.ON])
	# The code follows the crosshair, unless it is being typed in.
	if _crosshair_code != null and not _crosshair_code.has_focus():
		_crosshair_code.text = Crosshair.encode(Crosshair.chosen())
		_crosshair_view.queue_redraw()
	for action: String in _key_buttons:
		_key_buttons[action].text = "Press a key…" if action == _waiting_for else Controls.keys_text(action)


## Changes `key` (see HudLayout) of the part of the HUD that is chosen.
func set_hud(key: String, value: Variant) -> void:
	Settings.set_value(HudLayout.SETTING, HudLayout.with(Settings.get_value(HudLayout.SETTING), hud_part, key, value))


## Makes the crosshair `code` describes the player's, if it is a code for
## one. Returns whether it was.
func use_crosshair_code(code: String) -> bool:
	var style := Crosshair.decode(code)
	if style.is_empty():
		return false
	_crosshair_code.release_focus()
	Crosshair.apply(style)
	return true


## Chooses `id` in the list of choices named `name`, such as
## "Setting_quality", as picking it from the list does.
func choose(name: String, id: Variant) -> void:
	var known: Dictionary = _lists[name]
	var index: int = known.ids.find(id)
	if index != -1 and not (known.list as OptionButton).is_item_disabled(index):
		(known.list as OptionButton).select(index)
		(known.list as OptionButton).item_selected.emit(index)


## The ids of the choices in the list named `name`, in order.
func choices_of(name: String) -> Array:
	return _lists[name].ids


## A small view of the crosshair as it is chosen.
class CrosshairView:
	extends Control

	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.35, 0.45, 0.4))
		Crosshair.draw(self, size / 2.0, Crosshair.chosen(), CombatHud.CROSSHAIR_GAP)


## The settings that are one of a few: the setting, what it is called,
## and its choices with what each is called.
const SOUND_CHOICES := [
	[Mix.RANGE_SETTING, "Sound for", {"headphones": "Headphones", "speakers": "Speakers", "night": "Night (quiet and even)"}],
	[SoundCues.HIT_SETTING, "Hit sound", SoundCues.HIT_SOUNDS],
	[Crosshair.SHAPE, "Crosshair", Crosshair.SHAPES],
	[Crosshair.COLOUR, "Crosshair colour", Crosshair.COLOUR_NAMES],
	[StickAim.CURVE, "Stick response", StickAim.CURVE_NAMES],
	[TeamColors.PRESET, "Colours", TeamColors.PRESET_NAMES],
	[FramePace.CAP, "Frames", FramePace.CAP_NAMES],
	[TeamColors.OWN_CHEDDAR, "Cheddar's colour", TeamColors.OWN_NAMES],
	[TeamColors.OWN_BRIE, "Brie's colour", TeamColors.OWN_NAMES],
	[TeamColors.OWN_HIT, "Hit mark colour", TeamColors.OWN_NAMES],
]


## What one of the choices that are one of a few says, such as "Hit sound:
## Bell".
static func sound_choice_text(key: String) -> String:
	for choice: Array in SOUND_CHOICES:
		if choice[0] == key:
			return "%s: %s" % [choice[1], choice[2].get(Settings.get_value(key), "")]
	return ""


## The value a slider shows, such as "1.25×" or "80%".
static func value_text(key: String, value: float) -> String:
	for row: Array in SLIDERS:
		if row[0] == key:
			var shown: float = value * 100.0 if row[5] == "%d%%" else value
			return row[5] % shown
	return str(value)


## The settings that make up a loadout, with what each is called and the
## weapons it can be.
const LOADOUT := {"loadout_main": ["Main weapon", Weapons.MAINS], "loadout_side": ["Sidearm", Weapons.SIDES], "loadout_attachment": ["Fitted to it", Attachments.IDS], "loadout_overload": ["Overload", Overload.IDS]}


## What one part of the loadout, as `id`, is called, such as "Swiss Shotgun".
static func loadout_choice_text(key: String, id: Variant) -> String:
	if key == "loadout_overload":
		return Overload.text(Overload.from_id(id))
	if key == "loadout_attachment":
		return Attachments.text(Attachments.from_id(id))
	return Weapons.find(id).display_name


## What one part of the loadout says, such as "Main weapon: Swiss Shotgun".
static func loadout_text(key: String) -> String:
	return "%s: %s" % [LOADOUT[key][0], loadout_choice_text(key, Settings.get_value(key))]


## What one of the character's looks says, such as "Hat: Crown".
static func look_text(key: String) -> String:
	var choice: Array = Looks.SETTINGS[key]
	return "%s: %s" % [choice[2], choice[1][Settings.get_value(key)]]


## The choice after `current` among `choices`, coming round.
static func next_choice(choices: Array, current: Variant) -> Variant:
	return choices[(choices.find(current) + 1) % choices.size()]


## The next graphics level after `current`, coming round.
static func next_quality(current: String) -> String:
	var levels := GraphicsQuality.LEVELS
	return levels[(levels.find(current) + 1) % levels.size()]


## Starts waiting for the next key pressed, to give it to `action`.
func rebind(action: String) -> void:
	_waiting_for = action
	refresh()


func is_waiting_for_key() -> bool:
	return not _waiting_for.is_empty()


func _input(event: InputEvent) -> void:
	if _waiting_for.is_empty() or not visible:
		return
	if event is InputEventKey and event.pressed and not event.echo:
		get_viewport().set_input_as_handled()
		if event.physical_keycode != KEY_ESCAPE:
			var bindings: Dictionary = Settings.get_value("bindings")
			bindings[_waiting_for] = event.physical_keycode
			Settings.set_value("bindings", bindings)
			Controls.apply(bindings)
		_waiting_for = ""
		refresh()
	elif event.is_action_pressed("ui_cancel"):
		get_viewport().set_input_as_handled()
		_waiting_for = ""
		refresh()


func _reset_keys() -> void:
	_waiting_for = ""
	Settings.set_value("bindings", {})
	Controls.apply({})
	refresh()


func _show_value(key: String) -> void:
	if _value_labels.has(key):
		_value_labels[key].text = value_text(key, Settings.get_value(key))


# Fills the list of choices `name` afresh, as each can read differently as
# things change, and picks out the one chosen. Not while it is open.
func _show_list(name: String) -> void:
	var known: Dictionary = _lists[name]
	var list: OptionButton = known.list
	if list.get_popup().visible:
		return
	list.clear()
	for id: Variant in known.ids:
		list.add_item(known.name.call(id))
		list.set_item_disabled(list.item_count - 1, not known.can.call(id))
	list.select(known.ids.find(known.now.call()))


func _build() -> void:
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(560, 0)
	add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 20)
	panel.add_child(margin)
	var outer := VBoxContainer.new()
	outer.add_theme_constant_override("separation", 10)
	margin.add_child(outer)
	var title := Label.new()
	title.name = "SettingsTitle"
	title.text = "Settings"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	Look.heading(title)
	outer.add_child(title)
	_title = title
	# A tab for each group, which wrap onto a second line if they must.
	_tab_row = HFlowContainer.new()
	_tab_row.name = "SettingsTabs"
	_tab_row.alignment = FlowContainer.ALIGNMENT_CENTER
	_tab_row.add_theme_constant_override("h_separation", 6)
	_tab_row.add_theme_constant_override("v_separation", 6)
	outer.add_child(_tab_row)
	var tabs := ButtonGroup.new()
	var holder := VBoxContainer.new()
	holder.name = "Groups"
	for tab: Array in TABS:
		var button := Button.new()
		button.name = "SettingsTab_" + tab[0]
		button.text = tab[1]
		button.toggle_mode = true
		button.button_group = tabs
		button.custom_minimum_size = Vector2(0, 38)
		button.pressed.connect(show_group.bind(tab[0]))
		_tab_row.add_child(button)
		_tabs[tab[0]] = button
		var rows := VBoxContainer.new()
		rows.name = "Group_" + tab[0]
		rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		rows.add_theme_constant_override("separation", 8)
		holder.add_child(rows)
		_groups[tab[0]] = rows
	outer.add_child(holder)
	_scroll = FitScroll.wrap(holder)

	for row: Array in SLIDERS:
		_add_slider(GROUP_OF.get(row[0], "hud"), row[0], row[1], row[2], row[3], row[4])
	for row: Array in TOGGLES:
		var toggle := CheckButton.new()
		toggle.name = "Setting_" + row[0]
		toggle.custom_minimum_size = Vector2(0, 40)
		toggle.toggled.connect(func(on: bool) -> void: Settings.set_value(row[0], on))
		_row(GROUP_OF.get(row[0], "access"), row[1], toggle, true)
	# The settings that are one of a few.
	for choice: Array in SOUND_CHOICES:
		var key: String = choice[0]
		var names: Dictionary = choice[2]
		_add_list(GROUP_OF.get(key, "access"), "Setting_" + key, choice[1], names.keys(),
			func(id: Variant) -> String: return names[id],
			func(id: Variant) -> void: Settings.set_value(key, id),
			func() -> Variant: return Settings.get_value(key))
	# The mouse made to turn as far as it does in another game: which game,
	# and how sensitive it is there.
	var match_row := HBoxContainer.new()
	var match_game := _add_list("aim", "Setting_" + Sensitivity.OTHER_GAME, "Match the mouse of", Sensitivity.OTHER_GAMES.keys(),
		func(id: Variant) -> String: return Sensitivity.OTHER_GAMES[id][0],
		func(id: Variant) -> void: Settings.set_value(Sensitivity.OTHER_GAME, id),
		func() -> Variant: return Settings.get_value(Sensitivity.OTHER_GAME))
	var match_line := match_game.get_parent()
	match_line.remove_child(match_game)
	match_line.add_child(match_row)
	match_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	match_row.size_flags_stretch_ratio = 1.2
	match_game.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	match_row.add_child(match_game)
	var theirs := LineEdit.new()
	theirs.name = "MatchSensitivity"
	theirs.placeholder_text = "Its sensitivity"
	theirs.custom_minimum_size = Vector2(120, 40)
	theirs.text_submitted.connect(func(text: String) -> void:
		var ours := Sensitivity.matching(Settings.get_value(Sensitivity.OTHER_GAME), text.to_float() if text.is_valid_float() else -1.0)
		if ours > 0.0:
			Settings.set_value("mouse_sensitivity", ours)
			theirs.clear()
		theirs.release_focus())
	match_row.add_child(theirs)

	# The crosshair as a code, to copy for someone or to paste in from them,
	# with the crosshair itself beside it.
	var code_row := HBoxContainer.new()
	code_row.add_theme_constant_override("separation", 8)
	_crosshair_view = CrosshairView.new()
	_crosshair_view.name = "CrosshairView"
	_crosshair_view.custom_minimum_size = Vector2(56, 56)
	code_row.add_child(_crosshair_view)
	_crosshair_code = LineEdit.new()
	_crosshair_code.name = "CrosshairCode"
	_crosshair_code.placeholder_text = "Crosshair code"
	_crosshair_code.max_length = 60
	_crosshair_code.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_crosshair_code.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_crosshair_code.text_submitted.connect(use_crosshair_code)
	code_row.add_child(_crosshair_code)
	var copy_code := Button.new()
	copy_code.name = "CrosshairCopy"
	copy_code.text = "Copy"
	copy_code.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	copy_code.pressed.connect(func() -> void:
		if DisplayServer.has_feature(DisplayServer.FEATURE_CLIPBOARD):
			DisplayServer.clipboard_set(Crosshair.encode(Crosshair.chosen())))
	code_row.add_child(copy_code)
	_row("hud", "Crosshair code", code_row)
	# The HUD, a part at a time: which part, then where, how big, how
	# faint, and whether it is there at all.
	_heading("hud", "The HUD, a part at a time")
	_hud_part_button = _add_list("hud", "HudPart", "Part", HudLayout.PARTS.keys(),
		func(id: Variant) -> String: return HudLayout.PARTS[id],
		func(id: Variant) -> void:
			hud_part = id
			refresh(),
		func() -> Variant: return hud_part)
	for row: Array in HUD_SLIDERS:
		var slider := HSlider.new()
		slider.name = "Hud_" + row[0]
		slider.min_value = HudLayout.RANGES[row[0]][0]
		slider.max_value = HudLayout.RANGES[row[0]][1]
		slider.step = 0.05
		slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		slider.custom_minimum_size = Vector2(0, 32)
		slider.value_changed.connect(func(value: float) -> void: set_hud(row[0], value))
		_row("hud", row[1], slider)
		_hud_sliders[row[0]] = slider
	_hud_on = CheckButton.new()
	_hud_on.name = "Hud_on"
	_hud_on.custom_minimum_size = Vector2(0, 40)
	_hud_on.toggled.connect(func(on: bool) -> void: set_hud(HudLayout.ON, on))
	_row("hud", "Show this part", _hud_on, true)
	var presets := HBoxContainer.new()
	for preset: Array in [["HudMinimal", "Minimal HUD", HudLayout.minimal()], ["HudStandard", "Usual HUD", HudLayout.standard()]]:
		var button := Button.new()
		button.name = preset[0]
		button.text = preset[1]
		button.custom_minimum_size = Vector2(0, 40)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.pressed.connect(func() -> void: Settings.set_value(HudLayout.SETTING, preset[2]))
		presets.add_child(button)
	(_groups["hud"] as Control).add_child(presets)

	# The graphics: a level, and under it each part on its own, which is
	# either as the level has it or chosen.
	var quality := _add_list("graphics", "Setting_quality", "Quality", GraphicsQuality.LEVELS,
		func(id: Variant) -> String: return GraphicsQuality.NAMES[id],
		func(id: Variant) -> void: Settings.set_value("quality", id),
		func() -> Variant: return GraphicsQuality.level(Settings.get_value("quality")))
	(_groups["graphics"] as Control).move_child(quality.get_parent(), 0)
	for part: String in GraphicsQuality.ORDER:
		var known: Dictionary = GraphicsQuality.PARTS[part]
		var setting: String = known.setting
		var line := _add_list("graphics", "Setting_" + setting, known.name, [""] + known.choices.keys(),
			func(id: Variant) -> String:
				var level := GraphicsQuality.level(Settings.get_value("quality"))
				return "As %s: %s" % [GraphicsQuality.NAMES[level], known.choices[GraphicsQuality.resolve(part, level, "")]] if id == "" else known.choices[id],
			func(id: Variant) -> void: Settings.set_value(setting, id),
			func() -> Variant: return Settings.get_value(setting) if known.choices.has(Settings.get_value(setting)) else "")
		(_groups["graphics"] as Control).move_child(line.get_parent(), GraphicsQuality.ORDER.find(part) + 1)

	# What "Customise" shows: the weapons, and the character.
	_heading("weapons", "Your weapons, from when you next come back")
	for key: String in LOADOUT:
		_add_list("weapons", "Setting_" + key, LOADOUT[key][0], LOADOUT[key][1],
			func(id: Variant) -> String: return loadout_choice_text(key, id),
			func(id: Variant) -> void: Settings.set_value(key, id),
			func() -> Variant: return Settings.get_value(key))
	_heading("character", "Your character, as others see it")
	for key: String in Looks.SETTINGS:
		var kind: String = Looks.KIND_OF[key]
		var names: Dictionary = Looks.SETTINGS[key][1]
		# Everything, but only what the player has earned can be chosen;
		# the rest says what it takes.
		_add_list("character", "Setting_" + key, Looks.SETTINGS[key][2], Looks.SETTINGS[key][0],
			func(id: Variant) -> String:
				var needs := Unlocks.needs_text(kind, id)
				return names[id] if Unlocks.mine(kind, id) or needs.is_empty() else "%s  ·  %s" % [names[id], needs],
			func(id: Variant) -> void: Settings.set_value(key, id),
			func() -> Variant: return Settings.get_value(key),
			func(id: Variant) -> bool: return Unlocks.mine(kind, id))

	for action: String in Controls.REBINDABLE:
		var button := Button.new()
		button.name = "Key_" + action
		button.custom_minimum_size = Vector2(160, 36)
		button.pressed.connect(rebind.bind(action))
		_row("keys", Controls.REBINDABLE[action], button)
		_key_buttons[action] = button
	var reset := Button.new()
	reset.name = "SettingsResetKeys"
	reset.text = "Reset keys"
	reset.custom_minimum_size = Vector2(0, 36)
	reset.pressed.connect(_reset_keys)
	(_groups["keys"] as Control).add_child(reset)

	# Each group's settings in the order GROUP_OF has them, before anything
	# else it holds.
	var order := GROUP_OF.keys()
	for id: String in _groups:
		var keyed: Array = (_groups[id] as Node).get_children().filter(func(line: Node) -> bool: return order.has(_key_of(line)))
		keyed.sort_custom(func(a: Node, b: Node) -> bool: return order.find(_key_of(a)) < order.find(_key_of(b)))
		for index in keyed.size():
			(_groups[id] as Node).move_child(keyed[index], index)

	_done = Button.new()
	_done.name = "SettingsDone"
	_done.text = "Done"
	_done.custom_minimum_size = Vector2(0, 44)
	_done.pressed.connect(close)
	outer.add_child(_done)
	show_group(group)


# The setting a row is for, or "".
static func _key_of(line: Node) -> String:
	for child in line.get_children():
		if child.name.begins_with("Setting_"):
			return child.name.trim_prefix("Setting_")
		for inner in child.get_children():
			if inner.name.begins_with("Setting_"):
				return inner.name.trim_prefix("Setting_")
	return ""


# A row of group `id`: what it sets, `text`, on the left, and `control` on
# the right. A switch sits at the far right.
func _row(id: String, text: String, control: Control, switch := false) -> HBoxContainer:
	var line := HBoxContainer.new()
	line.add_theme_constant_override("separation", 12)
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(170, 0)
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	label.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	line.add_child(label)
	control.size_flags_horizontal = Control.SIZE_SHRINK_END if switch else Control.SIZE_EXPAND_FILL
	control.size_flags_stretch_ratio = 1.2
	line.add_child(control)
	(_groups[id] as Control).add_child(line)
	return line


func _heading(id: String, text: String) -> void:
	var heading := Label.new()
	heading.text = text
	heading.add_theme_font_size_override("font_size", 20)
	heading.add_theme_color_override("font_color", Look.CHEDDAR)
	(_groups[id] as Control).add_child(heading)


# A list of choices, as a row of group `id`: `ids` in order, each called
# what `name` says, `pick` called with the one chosen, `now` saying which is
# chosen, and `can` whether one may be.
func _add_list(id: String, control_name: String, text: String, ids: Array, name: Callable, pick: Callable, now: Callable, can := Callable()) -> OptionButton:
	var list := OptionButton.new()
	list.name = control_name
	list.custom_minimum_size = Vector2(0, 40)
	list.fit_to_longest_item = false
	list.clip_text = true
	list.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_lists[control_name] = {"list": list, "ids": ids, "name": name, "now": now,
		"can": can if can.is_valid() else func(_id: Variant) -> bool: return true}
	list.item_selected.connect(func(index: int) -> void: pick.call(ids[index]))
	Look.list_opens_at_choice(list)
	_row(id, text, list)
	_show_list(control_name)
	return list


func _add_slider(id: String, key: String, text: String, low: float, high: float, step: float) -> void:
	var holder := HBoxContainer.new()
	var slider := HSlider.new()
	slider.name = "Setting_" + key
	slider.min_value = low
	slider.max_value = high
	slider.step = step
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slider.custom_minimum_size = Vector2(0, 32)
	slider.value_changed.connect(func(value: float) -> void: Settings.set_value(key, value))
	holder.add_child(slider)
	var shown := Label.new()
	shown.custom_minimum_size = Vector2(60, 0)
	shown.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	holder.add_child(shown)
	_value_labels[key] = shown
	_row(id, text, holder)


## Every control that can be pressed or dragged and is in view, scrolled
## into the visible part of the list if it is in the list, for the browser tests.
func controls() -> Array[Control]:
	var found: Array[Control] = []
	var shown := _scroll.get_global_rect()
	for node in find_children("*", "BaseButton", true, false) + find_children("*", "Range", true, false):
		if not (node is Control) or not node.is_visible_in_tree() or node is ScrollBar:
			continue
		if _scroll.is_ancestor_of(node) and not shown.has_point(node.get_global_rect().get_center()):
			continue
		found.append(node)
	return found
