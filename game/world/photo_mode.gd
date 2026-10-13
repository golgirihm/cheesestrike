class_name PhotoMode
extends Node
## A photo mode: the camera comes free of the player, the HUD goes, and a
## picture of what is seen can be saved.
##
## Playing alone the camera can go anywhere. In a session with others it is
## kept within a few metres of the player and on their side of any wall, so
## that it can't be used to look round corners at them.

signal changed
signal saved(bytes: int)

const ACTION := "photo"
## Metres a second the camera flies, and radians it turns for each pixel
## the mouse moves.
const SPEED := 9.0
const LOOK := 0.0025
## How far from the player the camera can go in a session with others.
const REACH := 6.0
const WORLD_MASK := 1

var world: Node3D
var on := false
## How many pictures have been saved, and how big the last was, for tests.
var saved_count := 0
var _camera: Camera3D
var _back: Camera3D


func _init(of: Node3D = null) -> void:
	world = of
	name = "PhotoMode"


## Where a camera asked to be at `wanted` may be, when the player's eyes
## are at `eyes` and the camera is `tethered` to them or free: within reach
## of them when tethered. `blocked_at`, if given, is how far from the eyes
## a wall is on the way to it, which it stops short of.
static func allowed(wanted: Vector3, eyes: Vector3, tethered: bool, blocked_at := INF) -> Vector3:
	if not tethered:
		return wanted
	var away := wanted - eyes
	var far := minf(minf(away.length(), REACH), maxf(blocked_at - 0.3, 0.0))
	return eyes + away.normalized() * far if away.length() > 0.001 else eyes


## The name a picture is saved under, taken at `unix` seconds.
static func file_name(unix: int) -> String:
	return "cheesestrike-%d.png" % unix


## Whether the camera is kept near the player: in a session with anyone
## else in it.
func tethered() -> bool:
	return not Net.alone and world != null and world.players.get_child_count() > 1


## Whether there is a player of this machine's to free the camera from.
func can_begin() -> bool:
	return world != null and multiplayer.multiplayer_peer != null and world.player_node(multiplayer.get_unique_id()) != null


## Turns the photo mode on or off. Not for someone who isn't in a session.
func toggle() -> void:
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if world != null and multiplayer.multiplayer_peer != null else null
	if not on and own == null:
		return
	on = not on
	if on:
		_back = world.get_viewport().get_camera_3d()
		_camera = Camera3D.new()
		_camera.name = "PhotoCamera"
		_camera.fov = Settings.get_value("fov")
		world.add_child(_camera)
		_camera.global_transform = _back.global_transform if _back != null else own.head.global_transform
		_camera.make_current()
	else:
		if is_instance_valid(_camera):
			_camera.queue_free()
		if is_instance_valid(_back):
			_back.make_current()
		_camera = null
	changed.emit()


## Saves a picture of what is seen. Returns its size in bytes, or 0 if
## there was nothing to take.
func take() -> int:
	if not on:
		return 0
	var image: Image = world.get_viewport().get_texture().get_image() if world.get_viewport().get_texture() != null else null
	if image == null or image.is_empty():
		return 0
	var png := image.save_png_to_buffer()
	var called := file_name(int(Time.get_unix_time_from_system()))
	if OS.has_feature("web"):
		# Handed to the browser, which saves it where it saves anything.
		JavaScriptBridge.download_buffer(png, called, "image/png")
	else:
		DirAccess.make_dir_recursive_absolute("user://photos")
		var file := FileAccess.open("user://photos/" + called, FileAccess.WRITE)
		if file != null:
			file.store_buffer(png)
	saved_count += 1
	Sounds.play("ui_press", -4.0)
	saved.emit(png.size())
	return png.size()


func _process(delta: float) -> void:
	if not on:
		return
	var own: Node3D = world.player_node(multiplayer.get_unique_id()) if multiplayer.multiplayer_peer != null else null
	if own == null or not is_instance_valid(_camera):
		# The session ended under it.
		on = false
		_camera = null
		changed.emit()
		return
	var push := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var rise := Input.get_axis("crouch", "jump")
	var wanted: Vector3 = _camera.global_position + (_camera.global_basis * Vector3(push.x, 0, push.y) + Vector3.UP * rise) * SPEED * delta
	var eyes: Vector3 = own.head.global_position
	var blocked := INF
	if tethered():
		var hit := world.get_world_3d().direct_space_state.intersect_ray(PhysicsRayQueryParameters3D.create(eyes, wanted, WORLD_MASK))
		if not hit.is_empty():
			blocked = eyes.distance_to(hit.position)
	_camera.global_position = allowed(wanted, eyes, tethered(), blocked)
	if OS.has_feature("web") and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_look(WebPage.take_mouse_movement())


func _look(moved: Vector2) -> void:
	_camera.rotation.y -= moved.x * LOOK
	_camera.rotation.x = clampf(_camera.rotation.x - moved.y * LOOK, -1.5, 1.5)


func _input(event: InputEvent) -> void:
	# Not while something is being typed, where its key is a letter, and
	# not out of a session, where there is nothing to photograph.
	if world == null or get_viewport().gui_get_focus_owner() is LineEdit:
		return
	if event.is_action_pressed(ACTION):
		if on or can_begin():
			toggle()
			get_viewport().set_input_as_handled()
	elif on:
		if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED and not OS.has_feature("web"):
			_look(event.relative)
		elif event.is_action_pressed("fire") or event.is_action_pressed("ui_accept"):
			take()
			get_viewport().set_input_as_handled()
		elif event.is_action_pressed("ui_cancel"):
			toggle()
			get_viewport().set_input_as_handled()
