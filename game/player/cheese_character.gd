extends Node3D
## How a player looks to everyone else: an upright wedge of cheese with holes,
## two legs and two arms, built from simple shapes and animated in code. Its
## measurements are BodyShape's, which shots are tested against too, so it is
## hit exactly where it is seen. It stands on the player's feet and fits
## inside their collision capsule. The eyes are on the player's head, so they
## look where the player looks.
##
## The player sets its colour and tells it how it is moving; it walks, jumps,
## melts when knocked out and pops back on returning.

const HOLE_COLOR := Color(0.25, 0.17, 0.05)
const LEG_COLOR := Color(0.95, 0.85, 0.6)
## How far the legs swing, in radians, at full running speed.
const STRIDE := 0.7
## Steps per metre walked.
const STEPS_PER_METRE := 0.9
const MELT_TIME := 0.45
const POP_TIME := 0.2

## Whether to draw it at all: not for the player's own machine, which sees
## from inside it.
var shown := true

var _material := StandardMaterial3D.new()
var _wedge: MeshInstance3D
var _legs: Array[Node3D] = []
var _arms: Array[Node3D] = []
var _phase := 0.0
var _speed := 0.0
var _airborne := false
# 0 standing, 1 fully melted.
var _melt := 0.0
var _melting := false
# 0 just appeared, 1 full size.
var _pop := 1.0


func _init() -> void:
	_material.roughness = 0.7
	_build()
	scale = Vector3.ONE * BodyShape.SIZE


## The body's colour.
func set_colour(colour: Color) -> void:
	_material.albedo_color = colour


func colour() -> Color:
	return _material.albedo_color


## Whether the body glows, as in an Overload.
func set_glow(on: bool) -> void:
	_material.emission_enabled = on
	_material.emission = Color(1.0, 0.55, 0.1)
	_material.emission_energy_multiplier = 1.2


## How the player is moving, for the walk: speed across the ground in metres
## per second, and whether they are off the ground.
func set_motion(speed: float, airborne: bool) -> void:
	_speed = speed
	_airborne = airborne


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
	for index in _arms.size():
		var side := -1.0 if index == 0 else 1.0
		_arms[index].rotation.x = swing * side * 0.6
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
	var wedge_mesh := PrismMesh.new()
	wedge_mesh.size = BodyShape.WEDGE_SIZE
	wedge_mesh.material = _material
	_wedge = MeshInstance3D.new()
	_wedge.name = "Wedge"
	_wedge.mesh = wedge_mesh
	_wedge.position = BodyShape.WEDGE_CENTRE
	add_child(_wedge)

	# Holes on the front and sides.
	var hole_material := StandardMaterial3D.new()
	hole_material.albedo_color = HOLE_COLOR
	for hole: Array in BodyShape.HOLES:
		var sphere := SphereMesh.new()
		sphere.radius = hole[1]
		sphere.height = hole[1] * 2.0
		sphere.material = hole_material
		var mesh := MeshInstance3D.new()
		mesh.mesh = sphere
		mesh.position = hole[0]
		mesh.name = "Hole%d" % get_child_count()
		add_child(mesh)

	var limb_material := StandardMaterial3D.new()
	limb_material.albedo_color = LEG_COLOR
	for leg: Array in BodyShape.LEGS:
		var hip := Node3D.new()
		hip.name = "Leg%d" % (_legs.size() + 1)
		hip.position = leg[0]
		add_child(hip)
		hip.add_child(_limb(leg[1], leg[2], limb_material))
		_legs.append(hip)
	for arm: Array in BodyShape.ARMS:
		var shoulder := Node3D.new()
		shoulder.name = "Arm%d" % (_arms.size() + 1)
		shoulder.position = arm[0]
		add_child(shoulder)
		shoulder.add_child(_limb(arm[1], arm[2], limb_material))
		_arms.append(shoulder)


static func _limb(length: float, radius: float, material: Material) -> MeshInstance3D:
	var capsule := CapsuleMesh.new()
	capsule.radius = radius
	capsule.height = length
	capsule.material = material
	var mesh := MeshInstance3D.new()
	mesh.mesh = capsule
	# Hangs down from its joint.
	mesh.position = Vector3(0, -length / 2.0, 0)
	return mesh
