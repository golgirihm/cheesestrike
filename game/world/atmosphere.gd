class_name Atmosphere
extends Node3D
## The time of day and the weather over an arena: the colours of the sky,
## where the sun (or the moon) stands and how it lights the arena, haze, and
## rain or dust in the air. The host chooses one for a session and everyone
## sees the same. None of it changes how the game plays.
##
## The world adds this as a child and tells it which to show.

const NOON := "noon"
const DAWN := "dawn"
const DUSK := "dusk"
const NIGHT := "night"
const RAIN := "rain"
const DUST := "dust"
const ORDER := [NOON, DAWN, DUSK, NIGHT, RAIN, DUST]
## What the host's menu offers besides: one of the above, drawn by lot.
const RANDOM := "random"
const NAMES := {NOON: "Clear noon", DAWN: "Dawn", DUSK: "Dusk", NIGHT: "Night", RAIN: "Rain", DUST: "Dust storm", RANDOM: "Surprise me"}
## What falls or drifts, if anything.
const NOTHING := ""
const DROPS := "drops"
const MOTES := "motes"

## How each looks. `sun` is the light's colour and strength, `height` how
## far above the horizon it stands (degrees) and `turn` where round the
## compass; `clear` how much of the sky is free of cloud; `fog` how thick
## the haze is, in its own colour; `stars` how bright the stars are.
const LOOKS := {
	NOON: {"top": Color(0.2, 0.46, 0.9), "horizon": Color(0.72, 0.87, 0.98), "cloud": Color(1, 1, 1), "shade": Color(0.76, 0.81, 0.9), "clear": 0.5,
		"sun": Color(1, 0.98, 0.94), "energy": 1.0, "height": 55.0, "turn": 35.0, "ambient": Color(0.82, 0.88, 1), "ambient_energy": 0.5,
		"fog": 0.0, "fog_colour": Color(0.8, 0.88, 0.98), "stars": 0.0, "air": NOTHING},
	DAWN: {"top": Color(0.32, 0.45, 0.78), "horizon": Color(1.0, 0.72, 0.58), "cloud": Color(1.0, 0.86, 0.8), "shade": Color(0.72, 0.6, 0.7), "clear": 0.45,
		"sun": Color(1.0, 0.78, 0.62), "energy": 0.8, "height": 14.0, "turn": 95.0, "ambient": Color(0.92, 0.8, 0.82), "ambient_energy": 0.45,
		"fog": 0.006, "fog_colour": Color(1.0, 0.8, 0.72), "stars": 0.0, "air": NOTHING},
	DUSK: {"top": Color(0.2, 0.22, 0.5), "horizon": Color(0.98, 0.5, 0.3), "cloud": Color(1.0, 0.66, 0.5), "shade": Color(0.48, 0.36, 0.52), "clear": 0.5,
		"sun": Color(1.0, 0.6, 0.38), "energy": 0.75, "height": 10.0, "turn": 265.0, "ambient": Color(0.78, 0.66, 0.8), "ambient_energy": 0.42,
		"fog": 0.004, "fog_colour": Color(0.9, 0.55, 0.42), "stars": 0.15, "air": NOTHING},
	NIGHT: {"top": Color(0.02, 0.03, 0.09), "horizon": Color(0.09, 0.12, 0.24), "cloud": Color(0.3, 0.34, 0.46), "shade": Color(0.12, 0.14, 0.22), "clear": 0.62,
		"sun": Color(0.62, 0.72, 1.0), "energy": 0.42, "height": 48.0, "turn": 200.0, "ambient": Color(0.42, 0.5, 0.78), "ambient_energy": 0.5,
		"fog": 0.003, "fog_colour": Color(0.06, 0.08, 0.16), "stars": 1.0, "air": NOTHING},
	RAIN: {"top": Color(0.4, 0.45, 0.52), "horizon": Color(0.62, 0.66, 0.7), "cloud": Color(0.72, 0.74, 0.78), "shade": Color(0.45, 0.48, 0.54), "clear": 0.12,
		"sun": Color(0.82, 0.86, 0.92), "energy": 0.55, "height": 50.0, "turn": 35.0, "ambient": Color(0.72, 0.78, 0.86), "ambient_energy": 0.6,
		"fog": 0.012, "fog_colour": Color(0.6, 0.64, 0.7), "stars": 0.0, "air": DROPS},
	DUST: {"top": Color(0.62, 0.5, 0.36), "horizon": Color(0.8, 0.64, 0.46), "cloud": Color(0.86, 0.74, 0.58), "shade": Color(0.62, 0.5, 0.38), "clear": 0.3,
		"sun": Color(1.0, 0.84, 0.62), "energy": 0.7, "height": 40.0, "turn": 60.0, "ambient": Color(0.9, 0.78, 0.62), "ambient_energy": 0.55,
		"fog": 0.022, "fog_colour": Color(0.78, 0.62, 0.44), "stars": 0.0, "air": MOTES},
}

## Which is showing.
var showing := ""
var _environment: Environment
var _sun: DirectionalLight3D
var _sky: ShaderMaterial
var _air: CPUParticles3D


static func is_kind(id: Variant) -> bool:
	return id is String and LOOKS.has(id)


## One of the looks: `chosen`, or for RANDOM (or anything that isn't one)
## one drawn by lot.
static func settle(chosen: Variant, rng := RandomNumberGenerator.new()) -> String:
	if is_kind(chosen):
		return chosen
	return ORDER[rng.randi_range(0, ORDER.size() - 1)]


func _init(environment: Environment, sun: DirectionalLight3D) -> void:
	name = "Atmosphere"
	_environment = environment
	_sun = sun
	_sky = environment.sky.sky_material if environment.sky != null else null


## Shows `id`, with what is in the air only if `particles` are wanted.
func show_look(id: String, particles := true) -> void:
	if not is_kind(id):
		id = NOON
	var look: Dictionary = LOOKS[id]
	var wanted_air: String = look.air if particles else NOTHING
	if id == showing and (_air != null) == (wanted_air != NOTHING):
		return
	showing = id
	if _sky != null:
		_sky.set_shader_parameter("top_color", look.top)
		_sky.set_shader_parameter("horizon_color", look.horizon)
		_sky.set_shader_parameter("cloud_color", look.cloud)
		_sky.set_shader_parameter("cloud_shade", look.shade)
		_sky.set_shader_parameter("clear", look.clear)
		_sky.set_shader_parameter("stars", look.stars)
	_sun.light_color = look.sun
	_sun.light_energy = look.energy
	_sun.rotation_degrees = Vector3(-look.height, look.turn, 0.0)
	_environment.ambient_light_color = look.ambient
	_environment.ambient_light_energy = look.ambient_energy
	_environment.fog_enabled = look.fog > 0.0
	_environment.fog_density = look.fog
	_environment.fog_light_color = look.fog_colour
	_environment.fog_sky_affect = 0.0
	if _air != null:
		_air.queue_free()
		_air = null
	if wanted_air != NOTHING:
		_air = _fall(wanted_air)
		add_child(_air)


## What is falling or drifting just now: DROPS, MOTES or NOTHING.
func air() -> String:
	return String(_air.get_meta("kind")) if _air != null else NOTHING


func _process(_delta: float) -> void:
	# What is in the air is only made round whoever is looking.
	var camera := get_viewport().get_camera_3d() if _air != null else null
	if camera != null:
		_air.global_position = camera.global_position + Vector3.UP * (14.0 if air() == DROPS else 3.0)


# Rain coming down, or dust going by, in a box round the viewer.
func _fall(kind: String) -> CPUParticles3D:
	var air := CPUParticles3D.new()
	air.set_meta("kind", kind)
	air.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	air.local_coords = false
	air.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var drawn := StandardMaterial3D.new()
	drawn.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	drawn.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	var shape := BoxMesh.new()
	if kind == DROPS:
		air.amount = 500
		air.lifetime = 0.8
		air.emission_box_extents = Vector3(26, 1, 26)
		air.direction = Vector3(0.12, -1, 0.05)
		air.spread = 2.0
		air.initial_velocity_min = 34.0
		air.initial_velocity_max = 40.0
		air.gravity = Vector3.ZERO
		shape.size = Vector3(0.03, 0.9, 0.03)
		drawn.albedo_color = Color(0.8, 0.86, 0.95, 0.4)
	else:
		air.amount = 220
		air.lifetime = 4.0
		air.emission_box_extents = Vector3(24, 6, 24)
		air.direction = Vector3(1, 0.05, 0.3)
		air.spread = 12.0
		air.initial_velocity_min = 5.0
		air.initial_velocity_max = 9.0
		air.gravity = Vector3.ZERO
		shape.size = Vector3(0.12, 0.12, 0.12)
		drawn.albedo_color = Color(0.74, 0.6, 0.42, 0.35)
	shape.material = drawn
	air.mesh = shape
	return air
