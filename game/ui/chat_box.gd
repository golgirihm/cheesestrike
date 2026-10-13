class_name ChatBox
extends VBoxContainer
## Text chat on the screen: the last few things said, a box to type in, and
## a wheel of quick messages for a gamepad or a thumb.
##
## Enter opens the box to everyone and the team key to the team alone; Enter
## again sends and Escape gives up. While it is open the keys type rather
## than play. What a muted player says is not shown.

const SETTING := "text_chat"
## How many lines are kept, and seconds each is shown for.
const LINES := 6
const LIFE := 10.0
const WIDE := 380.0

var world: Node3D
## The wheel of quick messages.
var wheel: Wheel
# Each line said: {"text", "left", "colour"}.
var _said: Array[Dictionary] = []
var _lines: Label
var _row: HBoxContainer
var _to: Label
var _box: LineEdit
var _to_team := false
# Which way the wheel has been pushed by the mouse since it opened.
var _push := Vector2.ZERO


func _init(of: Node3D = null) -> void:
	world = of
	name = "ChatBox"
	custom_minimum_size = Vector2(WIDE, 0)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	alignment = BoxContainer.ALIGNMENT_END
	_lines = Label.new()
	_lines.name = "Lines"
	_lines.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_lines.custom_minimum_size = Vector2(WIDE, 0)
	_lines.add_theme_constant_override("outline_size", 5)
	_lines.add_theme_color_override("font_outline_color", Color.BLACK)
	_lines.add_theme_font_size_override("font_size", 14)
	_lines.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_lines)
	_row = HBoxContainer.new()
	_row.name = "Typing"
	_row.visible = false
	# While it is open, the keys are for typing: the player stands still.
	_row.add_to_group("pause_menu")
	add_child(_row)
	_to = Label.new()
	_row.add_child(_to)
	_box = LineEdit.new()
	_box.name = "ChatEdit"
	_box.max_length = Chat.MAX_LENGTH
	_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_box.text_submitted.connect(func(_text: String) -> void: submit())
	_row.add_child(_box)


func _ready() -> void:
	if world != null:
		world.match_.chatted.connect(add)


## Opens the box to type in, to the team alone or to everyone.
func open(to_team: bool) -> void:
	if not Settings.get_value(SETTING) or world == null or not world.match_.has_state():
		return
	_to_team = to_team and world.match_.state.settings.is_teams()
	_to.text = "To your team:" if _to_team else "To all:"
	_box.text = ""
	_row.show()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_box.grab_focus()


## Sends what has been typed and closes the box.
func submit() -> void:
	var text := Chat.clean(_box.text)
	if not text.is_empty() and world != null:
		world.match_.chat(text, _to_team)
	close()


## Closes the box without sending.
func close() -> void:
	_row.hide()
	_box.release_focus()


func is_typing() -> bool:
	return _row.visible


## Player `by` said `text`, to their team alone or to everyone.
func add(by: int, text: String, to_team: bool) -> void:
	if not Settings.get_value(SETTING) or world == null or world.voice_chat.is_muted(by):
		return
	var record: PlayerRecord = world.match_.state.player(by) if world.match_.has_state() else null
	var team: int = record.team if record != null else MatchState.NO_TEAM
	_said.append({"text": Chat.line(world.match_.state.name_of(by) if record != null else "Someone", text, to_team), "left": LIFE, "colour": TeamColors.of(team)})
	while _said.size() > LINES:
		_said.pop_front()
	Sounds.play("ui_hover", -8.0)
	_show()


## The lines showing, oldest first.
func lines() -> Array[String]:
	var texts: Array[String] = []
	for line in _said:
		texts.append(line.text)
	return texts


## Forgets everything said, as when a session ends.
func clear() -> void:
	_said.clear()
	close()
	wheel_close(false)
	_show()


## Opens the wheel of quick messages.
func wheel_open() -> void:
	if wheel == null or wheel.visible or not Settings.get_value(SETTING) or is_typing():
		return
	_push = Vector2.ZERO
	wheel.open(Chat.QUICK)


## Closes the wheel, sending the message picked to the team if `send`.
func wheel_close(send := true) -> void:
	if wheel == null or not wheel.visible:
		return
	if not send:
		wheel.picked = -1
	wheel.close()


func _on_wheel_chosen(index: int) -> void:
	if world != null and index >= 0 and index < Chat.QUICK.size():
		world.match_.chat(Chat.QUICK[index], true)


func _show() -> void:
	_lines.text = "\n".join(lines())
	_lines.visible = not _said.is_empty()


func _process(delta: float) -> void:
	# Lines stay while the box is open, so that what is being answered can
	# be read.
	if not is_typing():
		var before := _said.size()
		for line in _said:
			line.left -= delta
		_said = _said.filter(func(line: Dictionary) -> bool: return line.left > 0.0)
		if _said.size() != before:
			_show()
	# A gamepad's looking stick points the wheel.
	if wheel != null and wheel.visible:
		var stick := Input.get_vector("look_left", "look_right", "look_up", "look_down")
		if stick.length() >= Wheel.DEAD:
			wheel.point(stick)


func _input(event: InputEvent) -> void:
	if world == null or not is_visible_in_tree():
		return
	if is_typing():
		if event.is_action_pressed("ui_cancel"):
			close()
			get_viewport().set_input_as_handled()
		return
	if wheel != null and wheel.visible:
		# The mouse pushes the wheel as a stick would.
		if event is InputEventMouseMotion:
			_push = (_push + event.relative / 60.0).limit_length(1.0)
			wheel.point(_push)
		if event.is_action_released("quick_chat"):
			wheel_close()
		return
	if event.is_action_pressed("quick_chat"):
		wheel_open()
	elif event.is_action_pressed("chat") or event.is_action_pressed("chat_team"):
		open(event.is_action_pressed("chat_team"))
		get_viewport().set_input_as_handled()
