extends Control
## The arena editor: fly over a floor and lay pieces out on it (see
## ArenaCode). What is made is kept in the settings as a code, can be hosted
## from the menu as the "Custom arena", and can be copied as a link that
## gives the same arena to whoever opens it.
##
## Made for a keyboard and mouse: move with the usual keys, look with the
## mouse, click to place the piece shown where the marker is, right-click to
## take one away, the wheel to change the piece, the reload key to turn it,
## and 1 to 3 for the size of the floor.

signal closed

const FLY_SPEED := 18.0
const LOOK := 0.0025

var world: Node3D
## The arena being laid out, the piece that would be placed next, and how
## it is turned.
var arena := ArenaCode.blank()
var kind := ArenaCode.BLOCK
var turn := 0
## Where a piece would go, or null: set by where the view points, or for
## tests.
var aim: Variant = null
var _camera: Camera3D
var _ghost: MeshInstance3D
var _label: Label
var _copied := ""


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false
	_label = Label.new()
	_label.set_anchors_and_offsets_preset(Control.PRESET_TOP_LEFT, Control.PRESET_MODE_MINSIZE, 16)
	_label.add_theme_constant_override("outline_size", 6)
	_label.add_theme_color_override("font_outline_color", Color.BLACK)
	add_child(_label)
	var buttons := HBoxContainer.new()
	buttons.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT, Control.PRESET_MODE_MINSIZE, 16)
	buttons.grow_vertical = Control.GROW_DIRECTION_BEGIN
	add_child(buttons)
	for made: Array in [["EditorDone", "Done", close], ["EditorCopy", "Copy link", copy_link], ["EditorClear", "Clear", clear]]:
		var button := Button.new()
		button.name = made[0]
		button.text = made[1]
		button.custom_minimum_size = Vector2(120, 44)
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(made[2])
		buttons.add_child(button)
	var cross := Label.new()
	cross.text = "+"
	cross.set_anchors_and_offsets_preset(Control.PRESET_CENTER, Control.PRESET_MODE_MINSIZE)
	add_child(cross)


## Opens the editor on the arena that was last made, or a blank one.
func open(of: Node3D) -> void:
	world = of
	var kept := ArenaCode.decode(Settings.get_value("custom_arena"))
	arena = kept if not kept.is_empty() else ArenaCode.blank()
	_camera = Camera3D.new()
	_camera.name = "EditorCamera"
	_camera.fov = 75.0
	world.add_child(_camera)
	_camera.global_position = Vector3(0, 12, arena.size * 0.5)
	_camera.rotation = Vector3(-0.5, 0, 0)
	_ghost = MeshInstance3D.new()
	var shown := StandardMaterial3D.new()
	shown.albedo_color = Color(0.3, 0.9, 1.0, 0.45)
	shown.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	shown.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_ghost.material_override = shown
	world.add_child(_ghost)
	_rebuild()
	_camera.make_current()
	visible = true
	set_process(true)


## Keeps what was made and goes back to the menu.
func close() -> void:
	if not visible:
		return
	Settings.set_value("custom_arena", code())
	visible = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	if _camera != null:
		_camera.queue_free()
		_ghost.queue_free()
		_camera = null
	if world != null:
		world.load_map(Maps.DEFAULT)
		world.menu_camera.make_current()
	closed.emit()


## The arena as a code: "" for one with nothing on it.
func code() -> String:
	return ArenaCode.encode(arena) if not arena.pieces.is_empty() else ""


## Puts the piece shown where the marker is, on top of a block if there is
## one there. Returns whether it went.
func place_here() -> bool:
	if aim == null:
		return false
	var piece := {"kind": kind, "x": aim.x, "z": aim.z, "turn": turn, "level": 0}
	var placed := ArenaCode.place(arena, piece)
	if not placed and kind == ArenaCode.BLOCK:
		piece.level = 1
		placed = ArenaCode.place(arena, piece)
	if placed:
		_rebuild()
	return placed


## Takes away whatever is where the marker is.
func remove_here() -> bool:
	if aim == null or not ArenaCode.remove_at(arena, aim.x, aim.z):
		return false
	_rebuild()
	return true


## Changes the piece that would be placed, to the next or the one before.
func step_kind(by: int) -> void:
	kind = ArenaCode.KINDS[posmod(ArenaCode.KINDS.find(kind) + by, ArenaCode.KINDS.size())]


func turn_piece() -> void:
	turn = (turn + 1) % 4


## Changes the size of the floor, leaving out any piece that no longer fits.
func set_floor(metres: int) -> void:
	if not ArenaCode.SIZES.has(metres) or metres == arena.size:
		return
	var old: Array = arena.pieces
	arena = ArenaCode.blank(metres)
	for piece: Dictionary in old:
		ArenaCode.place(arena, piece)
	_rebuild()


func clear() -> void:
	arena = ArenaCode.blank(arena.size)
	_rebuild()


## Puts a link to the arena on the clipboard, and says so.
func copy_link() -> void:
	_copied = link()
	if DisplayServer.has_feature(DisplayServer.FEATURE_CLIPBOARD):
		DisplayServer.clipboard_set(_copied)


## The link that gives the arena to whoever opens it.
func link() -> String:
	return ArenaCode.link(WebPage.address(), code())


## What the editor tells its user, a line at a time.
func text() -> Array[String]:
	var lines: Array[String] = [
		"Arena editor   ·   %d of %d pieces   ·   floor %d m" % [arena.pieces.size(), ArenaCode.MAX_PIECES, arena.size],
		"Piece: %s   (wheel to change, %s to turn)" % [ArenaCode.NAMES[kind], Controls.keys_text("reload")],
		"Click to place, right-click to remove   ·   1, 2, 3 for the floor's size   ·   Esc for the buttons",
	]
	if ArenaCode.spawns(arena).size() < MatchSettings.MAX_PLAYERS:
		lines.append("Too crowded: there isn't room for sixteen players to start")
	if not _copied.is_empty():
		lines.append("Link copied")
	return lines


# Shows the arena as it now is.
func _rebuild() -> void:
	_copied = ""
	if world != null:
		world.show_custom(arena)


func _process(delta: float) -> void:
	if not visible or _camera == null:
		return
	_label.text = "\n".join(text())
	var captured := Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	if OS.has_feature("web") and captured:
		_look(WebPage.take_mouse_movement())
	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var rise := Input.get_axis("crouch", "jump")
	_camera.global_position += (_camera.global_basis * Vector3(input.x, 0, input.y) + Vector3.UP * rise) * FLY_SPEED * delta
	_camera.global_position.y = clampf(_camera.global_position.y, 2.0, 60.0)
	# Where the middle of the view meets the floor.
	var from := _camera.global_position
	var way := -_camera.global_basis.z
	aim = null
	if way.y < -0.05:
		var at := from + way * (from.y / -way.y)
		if absf(at.x) < arena.size / 2.0 and absf(at.z) < arena.size / 2.0:
			aim = Vector3(snappedf(at.x, ArenaCode.GRID), 0, snappedf(at.z, ArenaCode.GRID))
	_ghost.visible = aim != null
	if aim != null:
		var size: Vector3 = ArenaCode.SOLID.get(kind, Vector3(1.6, 0.3, 1.6))
		var box := BoxMesh.new()
		box.size = size if turn % 2 == 0 else Vector3(size.z, size.y, size.x)
		_ghost.mesh = box
		_ghost.global_position = aim + Vector3.UP * size.y / 2.0


func _look(moved: Vector2) -> void:
	_camera.rotation.y -= moved.x * LOOK
	_camera.rotation.x = clampf(_camera.rotation.x - moved.y * LOOK, -1.5, 1.2)


func _unhandled_input(event: InputEvent) -> void:
	if not visible or _camera == null:
		return
	var captured := Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
	if event is InputEventMouseMotion and captured and not OS.has_feature("web"):
		_look(event.relative)
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			step_kind(-1)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			step_kind(1)
		elif not captured:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		elif event.button_index == MOUSE_BUTTON_LEFT:
			place_here()
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			remove_here()
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event.is_action_pressed("reload"):
		turn_piece()
	else:
		for index in ArenaCode.SIZES.size():
			if event.is_action_pressed("weapon_%d" % (index + 1)):
				set_floor(ArenaCode.SIZES[index])
