extends Node3D
## How a player looks to everyone else: an upright wedge of cheese with a
## face and holes drawn flat on its surface, standing on two stick legs with
## two stick arms, one of them holding the weapon. It is built from simple
## shapes and animated in code. Its measurements are BodyShape's, which shots
## are tested against too, so the wedge is hit where it is seen. It stands
## on the player's feet and fits inside their collision capsule.
##
## The player sets its colour and tells it how it is moving, where it is
## looking and where its weapon is; it walks, jumps, winces when hurt, melts
## when knocked out and pops back on returning.

const INK := Color(0.1, 0.09, 0.09)
const BLUSH := Color(0.96, 0.5, 0.5)
const TEETH := Color(0.97, 0.97, 0.95)
## How far the legs swing, in radians, at full running speed.
const STRIDE := 0.7
## Steps per metre walked.
const STEPS_PER_METRE := 0.9
const MELT_TIME := 0.45
const POP_TIME := 0.2
## Seconds the face stays screwed up after being hurt.
const GRIMACE_TIME := 0.5
## Seconds the whole body flashes pale when hit.
const FLASH_TIME := 0.09
## How far above the surface each layer of what is drawn on it sits, so that
## none flickers through another.
const LAYER := 0.003
## How far the eyes shift up or down as the player looks up or down.
const GAZE_SHIFT := 0.012

## Whether to draw it at all: not for the player's own machine, which sees
## from inside it.
var shown := true

var _material := StandardMaterial3D.new()
var _hole_material := StandardMaterial3D.new()
var _ink := _flat(INK)
var _wedge: MeshInstance3D
var _legs: Array[Node3D] = []
var _arms: Array[Node3D] = []
var _eyes: Array[Node3D] = []
var _calm: Node3D
var _wince: Node3D
var _phase := 0.0
var _speed := 0.0
var _airborne := false
var _gaze := 0.0
var _grimace_left := 0.0
var _flash_left := 0.0
var _glowing := false
# 0 standing, 1 fully melted.
var _melt := 0.0
var _melting := false
# 0 just appeared, 1 full size.
var _pop := 1.0


func _init() -> void:
	_material.roughness = 0.7
	_hole_material.roughness = 0.9
	_build()
	scale = Vector3.ONE * BodyShape.SIZE
	# Until told otherwise, the weapon is where the player's scene holds it.
	reach_for(Vector3(0.36, BodyShape.HEAD_HEIGHT - 0.29, -0.34))


## The body's colour. Its holes are a darker shade of the same.
func set_colour(colour: Color) -> void:
	_material.albedo_color = colour
	_hole_material.albedo_color = colour.darkened(0.3)


func colour() -> Color:
	return _material.albedo_color


## Whether the body glows, as in an Overload.
func set_glow(on: bool) -> void:
	_glowing = on
	_light()


# The body lights up orange in an Overload, and white for an instant when hit.
func _light() -> void:
	_material.emission_enabled = _glowing or is_flashing()
	_material.emission = Color.WHITE if is_flashing() else Color(1.0, 0.45, 0.1)
	_material.emission_energy_multiplier = 0.9 if is_flashing() else 1.2


## How the player is moving, for the walk: speed across the ground in metres
## per second, and whether they are off the ground.
func set_motion(speed: float, airborne: bool) -> void:
	_speed = speed
	_airborne = airborne


## How far up (or, below zero, down) the player is looking, in radians: the
## eyes follow.
func set_gaze(pitch: float) -> void:
	_gaze = pitch


## The player was just hurt: the body flashes for an instant, and the face
## screws up for a moment.
func grimace() -> void:
	_grimace_left = GRIMACE_TIME
	_flash_left = FLASH_TIME
	_light()


func is_flashing() -> bool:
	return _flash_left > 0.0


func is_grimacing() -> bool:
	return _grimace_left > 0.0


## Stretches the weapon arm to the hand holding the weapon, at `hand` in this
## node's own measurements.
func reach_for(hand: Vector3) -> void:
	var arm := _arms[1]
	var reach := hand - arm.position
	var length := reach.length()
	if length < 0.001:
		return
	# The stick hangs down its own Y axis, so point that at the hand.
	arm.quaternion = Quaternion(Vector3.DOWN, reach / length)
	arm.get_child(0).scale.y = length / BodyShape.ARMS[1][1]


## Knocked out (melting away) or back in play (popping up).
func set_in_play(in_play: bool) -> void:
	if in_play and _melting:
		_melting = false
		_melt = 0.0
		_pop = 0.0
	elif not in_play:
		_melting = true


## How far a leg swings forward at walk phase `phase` (radians) when moving at
## `speed`: a sine wave, larger at speed, nothing standing still.
static func leg_swing(phase: float, speed: float) -> float:
	return sin(phase) * STRIDE * clampf(speed / 6.0, 0.0, 1.0)


func is_melted() -> bool:
	return _melting and _melt >= 1.0


func _process(delta: float) -> void:
	_phase = fmod(_phase + _speed * STEPS_PER_METRE * TAU * delta, TAU)
	var swing := leg_swing(_phase, _speed)
	for index in _legs.size():
		var side := 1.0 if index == 0 else -1.0
		# Legs tuck forward in the air.
		_legs[index].rotation.x = -0.6 if _airborne else swing * side
	# The free arm swings; the other holds the weapon.
	_arms[0].rotation.x = -swing * 0.6
	_grimace_left = maxf(_grimace_left - delta, 0.0)
	if is_flashing():
		_flash_left = maxf(_flash_left - delta, 0.0)
		_light()
	_calm.visible = not is_grimacing()
	_wince.visible = is_grimacing()
	for eye in _eyes:
		eye.position.y = BodyShape.HEAD_HEIGHT + clampf(_gaze, -1.0, 1.0) * GAZE_SHIFT
		# Screwed up tight when wincing.
		eye.scale.z = 0.35 if is_grimacing() else 1.0
	if _melting:
		_melt = minf(_melt + delta / MELT_TIME, 1.0)
	_pop = minf(_pop + delta / POP_TIME, 1.0)
	# Melting sags and spreads the body; popping back grows it from the floor.
	var sag := 1.0 - 0.85 * _melt
	var spread := 1.0 + 0.6 * _melt
	scale = Vector3(spread, sag, spread) * _ease_out(_pop) * BodyShape.SIZE
	visible = shown and not is_melted()


static func _ease_out(t: float) -> float:
	return 1.0 - (1.0 - t) * (1.0 - t)


func _build() -> void:
	_wedge = Rounded.instance(Rounded.wedge(BodyShape.WEDGE_SIZE, BodyShape.WEDGE_ROUNDING), _material, BodyShape.WEDGE_CENTRE)
	_wedge.name = "Wedge"
	add_child(_wedge)

	for hole: Array in BodyShape.HOLES:
		var place := BodyShape.hole_place(hole)
		var spot := Rounded.spot(place.radius, _hole_material, place.centre, place.normal, LAYER)
		spot.name = "Hole%d" % get_child_count()
		add_child(spot)

	_build_face()

	for leg: Array in BodyShape.LEGS:
		var hip := Node3D.new()
		hip.name = "Leg%d" % (_legs.size() + 1)
		hip.position = leg[0]
		add_child(hip)
		hip.add_child(_stick(leg[1]))
		# A shoe, toe forward, resting on the ground.
		var shoe := _ball(0.05, Vector3(0, -leg[1] + 0.03, -0.03))
		shoe.scale = Vector3(1.0, 0.6, 1.6)
		hip.add_child(shoe)
		_legs.append(hip)
	for arm: Array in BodyShape.ARMS:
		var shoulder := Node3D.new()
		shoulder.name = "Arm%d" % (_arms.size() + 1)
		shoulder.position = arm[0]
		add_child(shoulder)
		shoulder.add_child(_stick(arm[1]))
		_arms.append(shoulder)
	# The free arm hangs out from the side, with a mitten on the end. The
	# weapon arm's hand is part of the weapon it holds.
	_arms[0].rotation.z = -0.35
	_arms[0].add_child(_ball(0.042, Vector3(0, -BodyShape.ARMS[0][1], 0)))


# The face, drawn on the front of the wedge: two dot eyes, and either a
# smile between rosy cheeks or, when wincing, gritted teeth under a frown.
func _build_face() -> void:
	var front := -BodyShape.WEDGE_SIZE.z / 2.0
	var eye_level := BodyShape.HEAD_HEIGHT
	for centre in BodyShape.eye_centres():
		var eye := Rounded.spot(BodyShape.EYE_RADIUS, _ink, centre, Vector3.FORWARD, LAYER * 2.0)
		eye.name = "Eye%d" % (_eyes.size() + 1)
		# A glint, so the eyes look alive.
		eye.add_child(Rounded.spot(BodyShape.EYE_RADIUS * 0.32, _flat(Color.WHITE), Vector3(0.008, 0.0, 0.009), Vector3.UP, LAYER))
		add_child(eye)
		_eyes.append(eye)

	_calm = Node3D.new()
	_calm.name = "Calm"
	add_child(_calm)
	var blush := _flat(BLUSH)
	for side in [-1.0, 1.0]:
		_calm.add_child(Rounded.spot(0.034, blush, Vector3(0.15 * side, eye_level - 0.055, front), Vector3.FORWARD, LAYER))
	var smile := MeshInstance3D.new()
	smile.mesh = _arc(0.05, 0.011, deg_to_rad(205.0), deg_to_rad(335.0))
	smile.material_override = _ink
	smile.position = Vector3(0, eye_level - 0.03, front - LAYER * 2.0)
	_calm.add_child(smile)

	_wince = Node3D.new()
	_wince.name = "Wince"
	_wince.visible = false
	add_child(_wince)
	var mouth := Vector3(0, eye_level - 0.085, front)
	_wince.add_child(_patch(Vector2(0.15, 0.062), _ink, mouth, 1))
	_wince.add_child(_patch(Vector2(0.132, 0.044), _flat(TEETH), mouth, 2))
	_wince.add_child(_patch(Vector2(0.132, 0.007), _ink, mouth, 3))
	for gap in [-0.033, 0.0, 0.033]:
		_wince.add_child(_patch(Vector2(0.007, 0.044), _ink, mouth + Vector3(gap, 0, 0), 3))
	# Brows drawn down toward the middle.
	for side in [-1.0, 1.0]:
		var brow := _patch(Vector2(0.07, 0.014), _ink, Vector3(BodyShape.EYE_APART * side, eye_level + 0.045, front), 2)
		brow.rotation.z = 0.4 * side
		_wince.add_child(brow)


static func _flat(colour: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	material.roughness = 1.0
	# Drawn-on marks are flat, and seen from either side.
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material


# A flat rectangle of `size` on the front of the wedge, centred on `at`.
static func _patch(size: Vector2, material: Material, at: Vector3, layer: int) -> MeshInstance3D:
	var box := BoxMesh.new()
	box.size = Vector3(size.x, size.y, 0.001)
	var patch := MeshInstance3D.new()
	patch.mesh = box
	patch.material_override = material
	patch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	patch.position = at + Vector3.FORWARD * LAYER * layer
	return patch


# A flat curved line `width` thick: part of a circle of `radius`, from angle
# `from` round to `to`, lying in the XY plane.
static func _arc(radius: float, width: float, from: float, to: float) -> ArrayMesh:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	var steps := 12
	for step in steps + 1:
		var angle := lerpf(from, to, float(step) / steps)
		var out := Vector3(cos(angle), sin(angle), 0)
		vertices.append(out * (radius - width / 2.0))
		vertices.append(out * (radius + width / 2.0))
		normals.append(Vector3.FORWARD)
		normals.append(Vector3.FORWARD)
		if step > 0:
			var last := step * 2
			indices.append_array([last - 2, last - 1, last, last - 1, last + 1, last])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


# A stick `length` long hanging down from its joint.
func _stick(length: float) -> Node3D:
	var capsule := CapsuleMesh.new()
	capsule.radius = BodyShape.LIMB_RADIUS
	capsule.height = length
	capsule.radial_segments = 8
	capsule.rings = 2
	var mesh := MeshInstance3D.new()
	mesh.mesh = capsule
	mesh.material_override = _ink
	mesh.position = Vector3(0, -length / 2.0, 0)
	# Held by a node of its own, so that stretching it stretches it from the
	# joint.
	var holder := Node3D.new()
	holder.add_child(mesh)
	return holder


func _ball(radius: float, at: Vector3) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 12
	sphere.rings = 6
	var mesh := MeshInstance3D.new()
	mesh.mesh = sphere
	mesh.material_override = _ink
	mesh.position = at
	return mesh
