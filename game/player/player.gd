extends CharacterBody3D
## One player's character. The node is named after the owning peer's id, and
## that peer has authority over it: it simulates movement locally and the
## MultiplayerSynchronizer replicates the result to everyone else.
##
## The owner also fires its weapon here, showing the shot at once and telling
## the host (see Match), which decides what it hit. Health and whether the
## player is in play are the host's to decide, and arrive with the match state.

## A shot this player fired, for effects on this machine.
signal fired(origin: Vector3, direction: Vector3, seed: int)

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
@onready var gun: Node3D = $Head/Gun
## Where this player's shots leave from: the muzzle of the weapon in hand.
var muzzle: Node3D
@onready var name_tag: Label3D = $NameTag
@onready var collider: CollisionShape3D = $CollisionShape3D

# Touch controls: the left half of the screen is a movement stick, the right
# half drags to look and taps to jump, and on-screen buttons fire and reload.
var _move_touch := -1
var _move_touch_origin := Vector2.ZERO
var _move_touch_vector := Vector2.ZERO
var _look_touch := -1
var _look_touch_start_msec := 0
var _touch_jump := false
# Touch index -> name of the on-screen button it is holding.
var _button_touches := {}
var _mine := false

## Where this player starts, chosen by the host and set before the node enters
## the tree.
var spawn_position := Vector3.ZERO
## The world's Match node, set with spawn_position.
var match_: Node

## Seconds a weapon takes to come up after switching to it.
const SWITCH_TIME := 0.35
## How quickly a shove from a burst dies away, on the ground and in the air.
const KNOCKBACK_GROUND_DECAY := 6.0
const KNOCKBACK_AIR_DECAY := 0.8

## The weapon in this player's hands. Only the owner fires it.
var weapon := WeaponState.new(Weapons.find(Weapons.STARTING))
## Every weapon this player carries, by id. Kept by the owner, who learns from
## the host which they have.
var arsenal := {Weapons.STARTING: weapon}
## Whether the host has this player in play, as opposed to knocked out.
var alive := true

var _mouse_fire_held := false
# The sideways part of a shove from a burst, which dies away; the owner's own
# movement is added to it.
var _knockback := Vector3.ZERO
# A weapon change asked for since the last physics frame: a step through the
# carried weapons, or a weapon by id.
var _switch_step := 0
var _switch_to := ""
var _shown_weapon := ""
var _body_material: StandardMaterial3D
var _base_colour := Color.WHITE
var _team := -2
var _badge: MeshInstance3D
# Presses noted as their events arrive, so that a click or tap that begins and
# ends between two physics frames isn't missed.
var _fire_pressed := false
var _reload_pressed := false
var _jump_pressed := false
var _overload_pressed := false


func _enter_tree() -> void:
	set_multiplayer_authority(name.to_int())


func _ready() -> void:
	_body_material = StandardMaterial3D.new()
	body.material_override = _body_material
	_show_team(MatchState.NO_TEAM)

	_mine = is_multiplayer_authority()
	set_physics_process(_mine)
	set_process_input(_mine)
	set_process_unhandled_input(_mine)
	body.visible = not _mine
	visor.visible = not _mine
	name_tag.visible = not _mine
	# Everyone places a new character at its spot: until its owner's first
	# update arrives, it would otherwise stand in the middle of the arena, and
	# could collect what lies there.
	global_position = spawn_position
	_face_middle()
	if _mine:
		camera.make_current()
	_show_weapon(Weapons.STARTING)
	if match_ != null:
		match_.state_changed.connect(_on_match_state_changed)
		_on_match_state_changed()


## The host's record of this player, or null before it has arrived.
func record() -> PlayerRecord:
	return match_.state.player(name.to_int()) if match_ != null else null


## Puts this player, who has just come back into play, at `at`.
func respawn_at(at: Vector3) -> void:
	global_position = at
	velocity = Vector3.ZERO
	head.rotation.x = 0.0
	_knockback = Vector3.ZERO
	_face_middle()
	for carried: WeaponState in arsenal.values():
		carried.refill()


## A burst shoved this player.
func push(impulse: Vector3) -> void:
	velocity.y += impulse.y
	_knockback += Vector3(impulse.x, 0.0, impulse.z)


## Takes the weapon `id` in hand, if carried. The owner tells the host, so that
## others see it.
func equip(id: String) -> void:
	if not arsenal.has(id) or weapon.stats.id == id:
		return
	weapon.reload_left = 0.0
	weapon = arsenal[id]
	weapon.cooldown = maxf(weapon.cooldown, SWITCH_TIME)
	_show_weapon(id)
	if _mine and match_ != null:
		match_.select_weapon(id)


func _show_weapon(id: String) -> void:
	if id == _shown_weapon:
		return
	_shown_weapon = id
	for child in gun.get_children():
		child.queue_free()
	var model := WeaponModels.build(id)
	gun.add_child(model)
	muzzle = model.get_node("Muzzle")


func _face_middle() -> void:
	# Start facing the middle of the arena, not the wall behind the spot.
	rotation.y = atan2(global_position.x, global_position.z)


func _on_match_state_changed() -> void:
	var own := record()
	name_tag.text = match_.state.name_of(name.to_int())
	_set_alive(own == null or own.alive)
	_show_team(own.team if own != null else MatchState.NO_TEAM)
	_show_overload(own)
	if own == null:
		return
	if _mine:
		_carry(own.weapons)
	else:
		_show_weapon(own.weapon)


# Brings the owner's weapons in line with what the host says they carry: a new
# one is taken in hand at once, and a lost one is put away.
func _carry(carried: Array) -> void:
	var picked_up := ""
	for id: String in carried:
		if not arsenal.has(id) and Weapons.find(id) != null:
			arsenal[id] = WeaponState.new(Weapons.find(id))
			picked_up = id
	for id: String in arsenal.keys():
		if not carried.has(id):
			arsenal.erase(id)
	if not picked_up.is_empty():
		equip(picked_up)
	elif not arsenal.has(weapon.stats.id):
		weapon = arsenal.get(Weapons.STARTING, WeaponState.new(Weapons.find(Weapons.STARTING)))
		arsenal[Weapons.STARTING] = weapon
		_show_weapon(Weapons.STARTING)


# An Overloaded player glows; a melted one sags and darkens.
func _show_overload(own: PlayerRecord) -> void:
	var glowing := own != null and own.overload_left > 0.0
	var melted := own != null and own.melted_for > 0.0
	_body_material.emission_enabled = glowing
	_body_material.emission = Color(1.0, 0.55, 0.1)
	_body_material.emission_energy_multiplier = 1.2
	body.scale = Vector3(1.1, 0.85, 1.1) if melted else Vector3.ONE
	_body_material.albedo_color = _base_colour.darkened(0.35) if melted else _base_colour


## A player's own colour when not on a team: spread round the colour wheel by id.
static func colour_for(id: int) -> Color:
	return Color.from_hsv(fmod(id * 0.618034, 1.0), 0.65, 0.95)


# Team colours on the body and name, and a badge over the head whose shape
# also tells the teams apart: a wedge for Cheddar, a round wheel for Brie.
func _show_team(team: int) -> void:
	if team == _team:
		return
	_team = team
	_base_colour = TeamColors.of(team) if team != MatchState.NO_TEAM else colour_for(name.to_int())
	_body_material.albedo_color = _base_colour
	name_tag.modulate = TeamColors.of(team)
	if _badge != null:
		_badge.queue_free()
		_badge = null
	if team == MatchState.NO_TEAM:
		return
	_badge = MeshInstance3D.new()
	_badge.name = "TeamBadge"
	var material := StandardMaterial3D.new()
	material.albedo_color = TeamColors.of(team)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	if team == MatchState.CHEDDAR:
		var wedge := PrismMesh.new()
		wedge.size = Vector3(0.3, 0.26, 0.08)
		wedge.material = material
		_badge.mesh = wedge
	else:
		var wheel := CylinderMesh.new()
		wheel.top_radius = 0.15
		wheel.bottom_radius = 0.15
		wheel.height = 0.08
		wheel.material = material
		_badge.mesh = wheel
		_badge.rotation.x = PI / 2.0
	_badge.position = Vector3(0, 2.5, 0)
	_badge.visible = not _mine and alive
	add_child(_badge)


func _set_alive(now_alive: bool) -> void:
	if now_alive == alive:
		return
	alive = now_alive
	collider.set_deferred("disabled", not alive)
	if not _mine:
		body.visible = alive
		visor.visible = alive
		name_tag.visible = alive
		if _badge != null:
			_badge.visible = alive
	gun.visible = alive
	if not alive:
		_release_controls()


func _input(event: InputEvent) -> void:
	# Before the rest, as the mouse wheel's events are mouse buttons.
	if event.is_action_pressed("weapon_next"):
		_switch_step += 1
		return
	if event.is_action_pressed("weapon_previous"):
		_switch_step -= 1
		return
	if event is InputEventScreenTouch:
		_handle_touch(event)
	elif event is InputEventScreenDrag:
		_handle_drag(event)
	elif event.device == InputEvent.DEVICE_ID_EMULATION:
		return  # Mouse events synthesized from touches; handled above.
	elif event is InputEventMouseButton:
		var captured := Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
		if event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
			_mouse_fire_held = false
		elif event.pressed and captured and event.button_index == MOUSE_BUTTON_LEFT:
			_mouse_fire_held = true
			_fire_pressed = true
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		# A web build takes the movement from the browser instead; see WebPage.
		if not OS.has_feature("web"):
			_look(event.relative * MOUSE_SENSITIVITY)
	elif event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		_mouse_fire_held = false
	elif event.is_action_pressed("reload"):
		_reload_pressed = true
	elif event.is_action_pressed("overload"):
		_overload_pressed = true
	elif _weapon_key(event) != "":
		_switch_to = _weapon_key(event)
	elif event.is_action_pressed("jump"):
		_jump_pressed = true
	elif event.is_action_pressed("fire"):
		_fire_pressed = true


## A mouse movement measured in the page's pixels, as the browser reports it,
## in the game's own interface pixels, which is what the engine reports
## natively, so that the mouse feels the same either way. `view` is the size of
## the game's view in interface pixels, `window` the canvas in the screen's
## pixels and `pixel_ratio` the screen's pixels per page pixel.
static func page_to_view(movement: Vector2, view: Vector2, window: Vector2, pixel_ratio: float) -> Vector2:
	if window.x <= 0.0:
		return movement
	return movement * pixel_ratio * view.x / window.x


func _process(_delta: float) -> void:
	if OS.has_feature("web") and _mine:
		var moved := WebPage.take_mouse_movement()
		if moved != Vector2.ZERO and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			var view := get_viewport().get_visible_rect().size
			var window := Vector2(DisplayServer.window_get_size())
			_look(page_to_view(moved, view, window, DisplayServer.screen_get_scale()) * MOUSE_SENSITIVITY)


## Whether the in-game menu is open, so this player's controls are set aside.
func _menu_open() -> bool:
	var menu := get_tree().get_first_node_in_group("pause_menu")
	return menu != null and menu.visible


# Clicking the game takes the mouse. Handled after the interface has had the
# click, so that clicking a button, such as the menu's, doesn't.
func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.device != InputEvent.DEVICE_ID_EMULATION:
		# Not while waiting on the host or in the menu: the pointer is needed there.
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED and not Net.host_silent and not _menu_open():
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


# The weapon a number key asks for, or "".
static func _weapon_key(event: InputEvent) -> String:
	for index in Weapons.ORDER.size():
		if event.is_action_pressed("weapon_%d" % (index + 1)):
			return Weapons.ORDER[index]
	return ""


func _physics_process(delta: float) -> void:
	if Net.host_silent:
		return
	weapon.tick(delta)
	if _menu_open():
		_release_controls()
		_jump_pressed = false
		_touch_jump = false
		_reload_pressed = false
		velocity.x = 0.0
		velocity.z = 0.0
		if not is_on_floor():
			velocity += get_gravity() * delta
		move_and_slide()
		return
	if not alive:
		_jump_pressed = false
		_touch_jump = false
		_reload_pressed = false
		_fire_pressed = false
		return
	_look(Input.get_vector("look_left", "look_right", "look_up", "look_down") * STICK_LOOK_SPEED * delta)

	if not is_on_floor():
		velocity += get_gravity() * delta
	elif _jump_pressed or _touch_jump:
		velocity.y = JUMP_VELOCITY
	_jump_pressed = false
	_touch_jump = false

	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back") + _move_touch_vector
	var direction := transform.basis * Vector3(input.x, 0.0, input.y).limit_length(1.0)
	_knockback *= exp(-(KNOCKBACK_GROUND_DECAY if is_on_floor() else KNOCKBACK_AIR_DECAY) * delta)
	var speed := SPEED * Overload.speed_factor(record())
	velocity.x = direction.x * speed + _knockback.x
	velocity.z = direction.z * speed + _knockback.z
	move_and_slide()

	_use_weapon()


func _use_weapon() -> void:
	if _overload_pressed:
		_overload_pressed = false
		match_.set_off_overload()
	if _switch_step != 0:
		equip(Weapons.cycle(arsenal.keys(), weapon.stats.id, signi(_switch_step)))
	elif not _switch_to.is_empty():
		equip(_switch_to)
	_switch_step = 0
	_switch_to = ""
	if _reload_pressed:
		_reload_pressed = false
		if weapon.start_reload():
			match_.reload(weapon.stats.id)
	var held := Input.is_action_pressed("fire") or _mouse_fire_held or _button_touches.values().has("fire")
	var wants := _fire_pressed or (held and weapon.stats.automatic)
	# A press waits for the weapon to be ready, unless it is empty.
	if _fire_pressed and (weapon.can_fire() or weapon.is_reloading()):
		_fire_pressed = false
	if wants and weapon.fire():
		var origin := camera.global_position
		var direction := -camera.global_basis.z
		var seed := randi()
		match_.fire(origin, direction, seed, weapon.stats.id)
		fired.emit(origin, direction, seed)
		# Firing the last round starts the reload by itself.
		if weapon.is_reloading():
			match_.reload(weapon.stats.id)


func _exit_tree() -> void:
	if _mine:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _release_controls() -> void:
	_mouse_fire_held = false
	_fire_pressed = false
	_button_touches.clear()


func _look(delta: Vector2) -> void:
	if Net.host_silent:
		return
	rotate_y(-delta.x)
	head.rotation.x = clampf(head.rotation.x - delta.y, -PITCH_LIMIT, PITCH_LIMIT)


func _touch_controls() -> Node:
	return get_tree().get_first_node_in_group("touch_controls")


func _handle_touch(event: InputEventScreenTouch) -> void:
	var controls := _touch_controls()
	if controls != null:
		controls.notice_touch()
	# A touch on the menu button or in the menu is the interface's, not a move.
	if event.pressed and controls != null and controls.blocked_at(event.position):
		return
	if event.pressed:
		var button: String = controls.button_at(event.position) if controls != null else ""
		if button == "reload":
			_reload_pressed = true
		elif button == "swap":
			_switch_step += 1
		elif button == "overload":
			_overload_pressed = true
		elif not button.is_empty():
			_button_touches[event.index] = button
			_fire_pressed = _fire_pressed or button == "fire"
		elif event.position.x < get_viewport().get_visible_rect().size.x * 0.5:
			if _move_touch == -1:
				_move_touch = event.index
				_move_touch_origin = event.position
		elif _look_touch == -1:
			_look_touch = event.index
			_look_touch_start_msec = Time.get_ticks_msec()
	elif _button_touches.has(event.index):
		_button_touches.erase(event.index)
	elif event.index == _move_touch:
		_move_touch = -1
		_move_touch_vector = Vector2.ZERO
	elif event.index == _look_touch:
		_look_touch = -1
		if Time.get_ticks_msec() - _look_touch_start_msec < TOUCH_TAP_MSEC:
			_touch_jump = true
	if controls != null:
		controls.held = _button_touches.values()


func _handle_drag(event: InputEventScreenDrag) -> void:
	if event.index == _move_touch:
		_move_touch_vector = ((event.position - _move_touch_origin) / TOUCH_STICK_RADIUS).limit_length(1.0)
	elif event.index == _look_touch:
		_look(event.relative * TOUCH_LOOK_SENSITIVITY)
