extends CharacterBody3D
## One player's character. The node is named after the owning peer's id, and
## that peer has authority over it: it simulates movement locally and the
## MultiplayerSynchronizer replicates the result to everyone else.

const SPEED := 6.0
const JUMP_VELOCITY := 5.5
const MOUSE_SENSITIVITY := 0.0025
const STICK_LOOK_SPEED := 2.8
const TOUCH_LOOK_SENSITIVITY := 0.006
const TOUCH_STICK_RADIUS := 80.0
const TOUCH_TAP_MSEC := 200
const PITCH_LIMIT := deg_to_rad(85.0)

@onready var head: Node3D = $Head
@onready var camera: Camera3D = $Head/Camera3D
@onready var body: MeshInstance3D = $Body
@onready var visor: MeshInstance3D = $Head/Visor

# Touch controls: the left half of the screen is a movement stick, the right
# half drags to look and taps to jump.
var _move_touch := -1
var _move_touch_origin := Vector2.ZERO
var _move_touch_vector := Vector2.ZERO
var _look_touch := -1
var _look_touch_start_msec := 0
var _touch_jump := false
var _mine := false

## Where this player starts, chosen by the host and set before the node enters
## the tree.
var spawn_position := Vector3.ZERO


func _enter_tree() -> void:
	set_multiplayer_authority(name.to_int())


func _ready() -> void:
	var id := name.to_int()
	var material := StandardMaterial3D.new()
	material.albedo_color = Color.from_hsv(fmod(id * 0.618034, 1.0), 0.65, 0.95)
	body.material_override = material

	_mine = is_multiplayer_authority()
	set_physics_process(_mine)
	set_process_input(_mine)
	body.visible = not _mine
	visor.visible = not _mine
	if _mine:
		camera.make_current()
		global_position = spawn_position
		# Start facing the middle of the arena, not the wall behind the spot.
		rotation.y = atan2(spawn_position.x, spawn_position.z)


func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		_handle_touch(event)
	elif event is InputEventScreenDrag:
		_handle_drag(event)
	elif event.device == InputEvent.DEVICE_ID_EMULATION:
		return  # Mouse events synthesized from touches; handled above.
	elif event is InputEventMouseButton and event.pressed:
		# Not while waiting on the host: the pointer is needed for its notice.
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED and not Net.host_silent:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_look(event.relative * MOUSE_SENSITIVITY)
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _physics_process(delta: float) -> void:
	if Net.host_silent:
		return
	_look(Input.get_vector("look_left", "look_right", "look_up", "look_down") * STICK_LOOK_SPEED * delta)

	if not is_on_floor():
		velocity += get_gravity() * delta
	elif Input.is_action_just_pressed("jump") or _touch_jump:
		velocity.y = JUMP_VELOCITY
	_touch_jump = false

	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back") + _move_touch_vector
	var direction := transform.basis * Vector3(input.x, 0.0, input.y).limit_length(1.0)
	velocity.x = direction.x * SPEED
	velocity.z = direction.z * SPEED
	move_and_slide()


func _exit_tree() -> void:
	if _mine:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _look(delta: Vector2) -> void:
	if Net.host_silent:
		return
	rotate_y(-delta.x)
	head.rotation.x = clampf(head.rotation.x - delta.y, -PITCH_LIMIT, PITCH_LIMIT)


func _handle_touch(event: InputEventScreenTouch) -> void:
	if event.pressed:
		if event.position.x < get_viewport().get_visible_rect().size.x * 0.5:
			if _move_touch == -1:
				_move_touch = event.index
				_move_touch_origin = event.position
		elif _look_touch == -1:
			_look_touch = event.index
			_look_touch_start_msec = Time.get_ticks_msec()
	elif event.index == _move_touch:
		_move_touch = -1
		_move_touch_vector = Vector2.ZERO
	elif event.index == _look_touch:
		_look_touch = -1
		if Time.get_ticks_msec() - _look_touch_start_msec < TOUCH_TAP_MSEC:
			_touch_jump = true


func _handle_drag(event: InputEventScreenDrag) -> void:
	if event.index == _move_touch:
		_move_touch_vector = ((event.position - _move_touch_origin) / TOUCH_STICK_RADIUS).limit_length(1.0)
	elif event.index == _look_touch:
		_look(event.relative * TOUCH_LOOK_SENSITIVITY)
