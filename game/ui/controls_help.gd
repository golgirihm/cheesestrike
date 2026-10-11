extends CenterContainer
## A short list of the controls for the device being played with: shown when a
## player first enters a session, and from the in-game menu. The player holds
## still while it shows.

signal closed

const KEYBOARD := "keyboard"
const GAMEPAD := "gamepad"
const TOUCH := "touch"
const TITLES := {KEYBOARD: "Keyboard and mouse", GAMEPAD: "Gamepad", TOUCH: "Touch screen"}

var device := KEYBOARD
var _title: Label
var _grid: GridContainer
var _done: Button


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	visible = false
	# Players stand still under the in-game menu, and here too.
	add_to_group("pause_menu")
	add_to_group("touch_blockers")
	_build()


## The device a player is most likely using: a touch screen, else a gamepad
## if one is connected, else the keyboard and mouse.
static func device_for(touchscreen: bool, gamepads: int) -> String:
	if touchscreen:
		return TOUCH
	return GAMEPAD if gamepads > 0 else KEYBOARD


## What to do and how, on `device`: [what, how] pairs. Keys are the ones the
## player has chosen; touch follows which side the buttons are on.
static func lines(on_device: String) -> Array:
	match on_device:
		GAMEPAD:
			return [
				["Move", "Left stick"],
				["Look", "Right stick"],
				["Fire", "Right trigger"],
				["Jump", "A / Cross"],
				["Reload", "X / Square"],
				["Change weapon", "Shoulder buttons"],
				["Gouda Overload", "Y / Triangle, when the meter is full"],
				["Scoreboard", "Hold Back / Select"],
				["Menu", "Start / Options"],
			]
		TOUCH:
			var left_handed: bool = Settings.get_value("touch_left_handed")
			var move_side := "right" if left_handed else "left"
			var look_side := "left" if left_handed else "right"
			return [
				["Move", "Drag on the %s half of the screen" % move_side],
				["Look", "Drag on the %s half" % look_side],
				["Jump", "Tap quickly on the %s half" % look_side],
				["Fire", "Hold FIRE"],
				["Reload", "R"],
				["Change weapon", "Swap"],
				["Gouda Overload", "GO, when the meter is full"],
				["Scoreboard", "Hold Scores"],
				["Menu", "Menu, at the top left"],
			]
	return [
		["Move", "%s %s %s %s" % [Controls.keys_text("move_forward"), Controls.keys_text("move_left"), Controls.keys_text("move_back"), Controls.keys_text("move_right")]],
		["Look", "Mouse (click the game first)"],
		["Fire", "Left mouse button"],
		["Jump", Controls.keys_text("jump")],
		["Reload", Controls.keys_text("reload")],
		["Change weapon", "1 to 4, or the mouse wheel"],
		["Gouda Overload", "%s, when the meter is full" % Controls.keys_text("overload")],
		["Scoreboard", "Hold %s" % Controls.keys_text("scoreboard")],
		["Menu", "Esc"],
	]


## Shows the controls for `on_device`.
func open(on_device: String) -> void:
	device = on_device
	_title.text = TITLES[device]
	for child in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	for line: Array in lines(device):
		for index in 2:
			var label := Label.new()
			label.text = line[index]
			if index == 0:
				label.add_theme_color_override("font_color", Color(1.0, 0.82, 0.25))
			_grid.add_child(label)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	show()
	_done.grab_focus()


func close() -> void:
	hide()
	closed.emit()


## What the list shows, as "what: how" lines, for the browser tests.
func shown() -> Array[String]:
	var text: Array[String] = []
	var labels := _grid.get_children()
	for index in range(0, labels.size() - 1, 2):
		text.append("%s: %s" % [labels[index].text, labels[index + 1].text])
	return text


func _input(event: InputEvent) -> void:
	if visible and (event.is_action_pressed("ui_cancel") or event.is_action_pressed("pause_menu")):
		get_viewport().set_input_as_handled()
		close()


func _build() -> void:
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(380, 0)
	add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 20)
	panel.add_child(margin)
	var rows := VBoxContainer.new()
	rows.add_theme_constant_override("separation", 10)
	margin.add_child(rows)
	_title = Label.new()
	_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_title.add_theme_font_size_override("font_size", 24)
	rows.add_child(_title)
	_grid = GridContainer.new()
	_grid.columns = 2
	_grid.add_theme_constant_override("h_separation", 18)
	_grid.add_theme_constant_override("v_separation", 4)
	rows.add_child(_grid)
	_done = Button.new()
	_done.name = "HelpDone"
	_done.text = "Got it"
	_done.custom_minimum_size = Vector2(0, 44)
	_done.pressed.connect(close)
	rows.add_child(_done)
