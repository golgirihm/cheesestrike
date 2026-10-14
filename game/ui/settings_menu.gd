extends CenterContainer
## The settings screen: how looking feels, the field of view, loudness,
## graphics quality and the keys. Every change is saved and takes effect at
## once. Reached from the main menu and the in-game menu, over either.
##
## Each control is named after what it sets ("Setting_fov",
## "Key_move_forward"), which the browser tests use to find them.

signal closed

const SLIDERS := [
	["mouse_sensitivity", "Mouse look", 0.1, 4.0, 0.05, "%.2f×"],
	["stick_sensitivity", "Stick look", 0.1, 4.0, 0.05, "%.2f×"],
	["stick_dead_zone", "Stick dead zone", 0.0, 0.5, 0.01, "%d%%"],
	["touch_sensitivity", "Touch look", 0.1, 4.0, 0.05, "%.2f×"],
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
	["screen_shake", "Screen shake", 0.0, 1.0, 0.05, "%d%%"],
	["camera_motion", "Camera motion", 0.0, 1.0, 0.05, "%d%%"],
	["crosshair_size", "Crosshair: length", 2.0, 24.0, 1.0, "%d px"],
	["crosshair_gap", "Crosshair: gap", 0.0, 16.0, 1.0, "%d px"],
	["crosshair_thickness", "Crosshair: thickness", 1.0, 6.0, 1.0, "%d px"],
]
## The settings that are on or off, with what each says.
const TOGGLES := [
	["invert_look", "Invert looking up and down"],
	["less_motion", "Less motion: no shake, bob or sway, and softer flashes"],
	["aim_assist", "Gamepad: slow the view over an opponent"],
	["rumble", "Gamepad: rumble"],
	["gyro_aim", "Aim by turning the phone"],
	["touch_left_handed", "Touch buttons on the left"],
	["touch_fullscreen", "Full screen when touched"],
	["captions", "Captions for sounds"],
	["toggle_run", "Press once to run, again to stop"],
	["toggle_crouch", "Press once to crouch, again to stand"],
	["toggle_aim", "Press once to aim, again to stop"],
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
	["outlines", "Outline teammates and nearby pickups"],
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

var _rows: VBoxContainer
var _title: Label
## The part of the HUD being laid out, and the controls that do it.
var hud_part: String = HudLayout.PARTS.keys()[0]
const HUD_SLIDERS := [[HudLayout.X, "Across"], [HudLayout.Y, "Down"], [HudLayout.SCALE, "Size"], [HudLayout.OPACITY, "Opacity"]]
var _hud_part_button: Button
var _hud_sliders := {}
var _hud_on: CheckButton
var _crosshair_code: LineEdit
var _crosshair_view: Control
var _customise_rows: Array[Control] = []
var _scroll: ScrollContainer
var _done: Button
var _value_labels := {}
var _key_buttons := {}
# The action waiting for its new key, or "".
var _waiting_for := ""


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	visible = false
	add_to_group("touch_blockers")
	_build()
	Settings.changed.connect(func(_key: String) -> void: refresh())
	refresh()


func open() -> void:
	refresh()
	show()
	_done.grab_focus()


## Opens the screen at one part of itself: CUSTOMISE for the character and
## weapons alone, SETTINGS for the rest, EVERYTHING for both.
func open_part(part: String) -> void:
	open()
	_title.text = "Customise" if part == CUSTOMISE else "Settings"
	for row: Control in _rows.get_children():
		row.visible = part == EVERYTHING or (_customise_rows.has(row) == (part == CUSTOMISE))


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
	for choice: Array in SOUND_CHOICES:
		(find_child("Setting_" + choice[0], true, false) as Button).text = sound_choice_text(choice[0])
	# The HUD's controls show the part that is chosen.
	if _hud_part_button != null:
		_hud_part_button.text = "HUD part: %s" % HudLayout.PARTS[hud_part]
		var how := HudLayout.part(Settings.get_value(HudLayout.SETTING), hud_part)
		for key: String in _hud_sliders:
			(_hud_sliders[key] as HSlider).set_value_no_signal(how[key])
		_hud_on.set_pressed_no_signal(how[HudLayout.ON])
	# The code follows the crosshair, unless it is being typed in.
	if _crosshair_code != null and not _crosshair_code.has_focus():
		_crosshair_code.text = Crosshair.encode(Crosshair.chosen())
		_crosshair_view.queue_redraw()
	(find_child("Setting_quality", true, false) as Button).text = "Graphics: %s" % GraphicsQuality.NAMES[GraphicsQuality.level(Settings.get_value("quality"))]
	for part: String in GraphicsQuality.ORDER:
		var setting: String = GraphicsQuality.PARTS[part].setting
		(find_child("Setting_" + setting, true, false) as Button).text = GraphicsQuality.part_text(part, GraphicsQuality.level(Settings.get_value("quality")), Settings.get_value(setting))
	for key: String in Looks.SETTINGS:
		(find_child("Setting_" + key, true, false) as Button).text = look_text(key)
	for key: String in LOADOUT:
		(find_child("Setting_" + key, true, false) as Button).text = loadout_text(key)
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


## A small view of the crosshair as it is chosen.
class CrosshairView:
	extends Control

	func _draw() -> void:
		draw_rect(Rect2(Vector2.ZERO, size), Color(0.35, 0.45, 0.4))
		Crosshair.draw(self, size / 2.0, Crosshair.chosen(), CombatHud.CROSSHAIR_GAP)


## The sound settings that are one of a few: the setting, what it is called,
## and its choices with what each is called.
const SOUND_CHOICES := [
	[Mix.RANGE_SETTING, "Sound for", {"headphones": "Headphones", "speakers": "Speakers", "night": "Night (quiet and even)"}],
	[SoundCues.HIT_SETTING, "Hit sound", SoundCues.HIT_SOUNDS],
	[Crosshair.SHAPE, "Crosshair", Crosshair.SHAPES],
	[Crosshair.COLOUR, "Crosshair colour", Crosshair.COLOUR_NAMES],
	[StickAim.CURVE, "Stick response", StickAim.CURVE_NAMES],
]


## What the button for one of the sound choices says, such as "Hit sound:
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


## What the button for one part of the loadout says, such as "Main weapon:
## Swiss Shotgun".
static func loadout_text(key: String) -> String:
	if key == "loadout_overload":
		return "%s: %s" % [LOADOUT[key][0], Overload.text(Overload.from_id(Settings.get_value(key)))]
	if key == "loadout_attachment":
		return "%s: %s" % [LOADOUT[key][0], Attachments.text(Attachments.from_id(Settings.get_value(key)))]
	return "%s: %s" % [LOADOUT[key][0], Weapons.find(Settings.get_value(key)).display_name]


## What the button for one of the character's looks says, such as "Hat: Crown".
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


func _build() -> void:
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(420, 0)
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

	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(380, 400)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	outer.add_child(scroll)
	_scroll = scroll
	_rows = VBoxContainer.new()
	_rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_rows.add_theme_constant_override("separation", 8)
	scroll.add_child(_rows)

	for row: Array in SLIDERS:
		_add_slider(row[0], row[1], row[2], row[3], row[4])
	for row: Array in TOGGLES:
		var toggle := CheckButton.new()
		toggle.name = "Setting_" + row[0]
		toggle.text = row[1]
		toggle.custom_minimum_size = Vector2(0, 40)
		toggle.toggled.connect(func(on: bool) -> void: Settings.set_value(row[0], on))
		_rows.add_child(toggle)
	# The sound choices that are one of a few.
	for choice: Array in SOUND_CHOICES:
		var button := Button.new()
		button.name = "Setting_" + choice[0]
		button.custom_minimum_size = Vector2(0, 40)
		button.pressed.connect(func() -> void:
			Settings.set_value(choice[0], next_choice(choice[2].keys(), Settings.get_value(choice[0]))))
		_rows.add_child(button)
	# The crosshair as a code, to copy for someone or to paste in from them,
	# with the crosshair itself beside it.
	var code_row := HBoxContainer.new()
	_rows.add_child(code_row)
	_crosshair_view = CrosshairView.new()
	_crosshair_view.name = "CrosshairView"
	_crosshair_view.custom_minimum_size = Vector2(64, 64)
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
	# The HUD, a part at a time: which part, then where, how big, how
	# faint, and whether it is there at all.
	var hud_title := Label.new()
	hud_title.text = "The HUD, a part at a time"
	hud_title.add_theme_font_size_override("font_size", 20)
	_rows.add_child(hud_title)
	_hud_part_button = Button.new()
	_hud_part_button.name = "HudPart"
	_hud_part_button.custom_minimum_size = Vector2(0, 40)
	_hud_part_button.pressed.connect(func() -> void:
		hud_part = next_choice(HudLayout.PARTS.keys(), hud_part)
		refresh())
	_rows.add_child(_hud_part_button)
	for row: Array in HUD_SLIDERS:
		var line := HBoxContainer.new()
		_rows.add_child(line)
		var label := Label.new()
		label.text = row[1]
		label.custom_minimum_size = Vector2(110, 0)
		line.add_child(label)
		var slider := HSlider.new()
		slider.name = "Hud_" + row[0]
		slider.min_value = HudLayout.RANGES[row[0]][0]
		slider.max_value = HudLayout.RANGES[row[0]][1]
		slider.step = 0.05
		slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		slider.value_changed.connect(func(value: float) -> void: set_hud(row[0], value))
		line.add_child(slider)
		_hud_sliders[row[0]] = slider
	_hud_on = CheckButton.new()
	_hud_on.name = "Hud_on"
	_hud_on.text = "Show this part"
	_hud_on.custom_minimum_size = Vector2(0, 40)
	_hud_on.toggled.connect(func(on: bool) -> void: set_hud(HudLayout.ON, on))
	_rows.add_child(_hud_on)
	var presets := HBoxContainer.new()
	_rows.add_child(presets)
	for preset: Array in [["HudMinimal", "Minimal HUD", HudLayout.minimal()], ["HudStandard", "Usual HUD", HudLayout.standard()]]:
		var button := Button.new()
		button.name = preset[0]
		button.text = preset[1]
		button.custom_minimum_size = Vector2(0, 40)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.pressed.connect(func() -> void: Settings.set_value(HudLayout.SETTING, preset[2]))
		presets.add_child(button)
	var quality := Button.new()
	quality.name = "Setting_quality"
	quality.custom_minimum_size = Vector2(0, 40)
	quality.pressed.connect(func() -> void:
		Settings.set_value("quality", next_quality(GraphicsQuality.level(Settings.get_value("quality")))))
	_rows.add_child(quality)
	# Each part of the graphics on its own, under the level above.
	for part: String in GraphicsQuality.ORDER:
		var setting: String = GraphicsQuality.PARTS[part].setting
		var button := Button.new()
		button.name = "Setting_" + setting
		button.custom_minimum_size = Vector2(0, 40)
		button.pressed.connect(func() -> void:
			Settings.set_value(setting, GraphicsQuality.next_choice(part, Settings.get_value(setting))))
		_rows.add_child(button)

	# From here on is what "Customise" shows on its own.
	var first_of_customise := _rows.get_child_count()
	var loadout_title := Label.new()
	loadout_title.text = "Your weapons, from when you next come back"
	loadout_title.add_theme_font_size_override("font_size", 20)
	_rows.add_child(loadout_title)
	for key: String in LOADOUT:
		var slot := Button.new()
		slot.name = "Setting_" + key
		slot.custom_minimum_size = Vector2(0, 40)
		slot.pressed.connect(func() -> void:
			Settings.set_value(key, next_choice(LOADOUT[key][1], Settings.get_value(key))))
		_rows.add_child(slot)

	var look_title := Label.new()
	look_title.text = "Your character, as others see it"
	look_title.add_theme_font_size_override("font_size", 20)
	_rows.add_child(look_title)
	for key: String in Looks.SETTINGS:
		var look := Button.new()
		look.name = "Setting_" + key
		look.custom_minimum_size = Vector2(0, 40)
		look.pressed.connect(func() -> void:
			# Through what the player has earned, and no further.
			Settings.set_value(key, Unlocks.next(Looks.KIND_OF[key], Settings.get_value(key), Progress.level(), Progress.achievements, Progress.challenges_done())))
		_rows.add_child(look)

	for index in range(first_of_customise, _rows.get_child_count()):
		_customise_rows.append(_rows.get_child(index))
	var keys_title := Label.new()
	keys_title.text = "Keys"
	keys_title.add_theme_font_size_override("font_size", 20)
	_rows.add_child(keys_title)
	for action: String in Controls.REBINDABLE:
		var line := HBoxContainer.new()
		_rows.add_child(line)
		var label := Label.new()
		label.text = Controls.REBINDABLE[action]
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		line.add_child(label)
		var button := Button.new()
		button.name = "Key_" + action
		button.custom_minimum_size = Vector2(160, 36)
		button.pressed.connect(rebind.bind(action))
		line.add_child(button)
		_key_buttons[action] = button
	var reset := Button.new()
	reset.name = "SettingsResetKeys"
	reset.text = "Reset keys"
	reset.custom_minimum_size = Vector2(0, 36)
	reset.pressed.connect(_reset_keys)
	_rows.add_child(reset)

	_done = Button.new()
	_done.name = "SettingsDone"
	_done.text = "Done"
	_done.custom_minimum_size = Vector2(0, 44)
	_done.pressed.connect(close)
	outer.add_child(_done)


func _add_slider(key: String, text: String, low: float, high: float, step: float) -> void:
	var line := HBoxContainer.new()
	_rows.add_child(line)
	var label := Label.new()
	label.text = text
	label.custom_minimum_size = Vector2(110, 0)
	line.add_child(label)
	var slider := HSlider.new()
	slider.name = "Setting_" + key
	slider.min_value = low
	slider.max_value = high
	slider.step = step
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slider.custom_minimum_size = Vector2(0, 32)
	slider.value_changed.connect(func(value: float) -> void: Settings.set_value(key, value))
	line.add_child(slider)
	var shown := Label.new()
	shown.custom_minimum_size = Vector2(60, 0)
	shown.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	line.add_child(shown)
	_value_labels[key] = shown


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
