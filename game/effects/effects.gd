extends Node3D
## Short-lived effects in the arena that only show what happened: a shot's
## muzzle flash and the tracer of each pellet. They have no effect on play, so
## each machine makes its own.

const TRACER_LIFE := 0.09
const TRACER_WIDTH := 0.03
const FLASH_LIFE := 0.06
const TRACER_COLOR := Color(1.0, 0.9, 0.45)
const FLASH_COLOR := Color(1.0, 0.75, 0.3)

var _tracer_material := _glow(TRACER_COLOR)
var _flash_material := _glow(FLASH_COLOR)
var _tracer_mesh := BoxMesh.new()
var _flash_mesh := SphereMesh.new()


func _init() -> void:
	_tracer_mesh.size = Vector3(TRACER_WIDTH, TRACER_WIDTH, 1.0)
	_tracer_mesh.material = _tracer_material
	_flash_mesh.radius = 0.09
	_flash_mesh.height = 0.18
	_flash_mesh.material = _flash_material


## A shot fired from `muzzle` whose pellets ended at `ends`.
func show_shot(muzzle: Vector3, ends: Array) -> void:
	_flash(muzzle)
	for end: Variant in ends:
		if end is Vector3:
			_tracer(muzzle, end)


## Where the tracer from `from` to `to` sits: a unit-long box stretched along
## the line between them and centred on it.
static func tracer_transform(from: Vector3, to: Vector3) -> Transform3D:
	var length := from.distance_to(to)
	if length < 0.001:
		return Transform3D(Basis.from_scale(Vector3(1, 1, 0.001)), from)
	var up := Vector3.UP if absf((to - from).normalized().y) < 0.99 else Vector3.RIGHT
	var basis := Basis.looking_at(to - from, up).scaled_local(Vector3(1, 1, length))
	return Transform3D(basis, (from + to) / 2.0)


func _tracer(from: Vector3, to: Vector3) -> void:
	var tracer := MeshInstance3D.new()
	tracer.mesh = _tracer_mesh
	tracer.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(tracer)
	tracer.global_transform = tracer_transform(from, to)
	_expire(tracer, TRACER_LIFE)


func _flash(at: Vector3) -> void:
	var flash := MeshInstance3D.new()
	flash.mesh = _flash_mesh
	flash.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(flash)
	flash.global_position = at
	var light := OmniLight3D.new()
	light.light_color = FLASH_COLOR
	light.light_energy = 2.0
	light.omni_range = 3.0
	flash.add_child(light)
	_expire(flash, FLASH_LIFE)


func _expire(node: Node, after: float) -> void:
	get_tree().create_timer(after, false).timeout.connect(node.queue_free)


static func _glow(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	return material
