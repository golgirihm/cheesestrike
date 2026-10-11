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
var _wheel_mesh := CylinderMesh.new()
var _burst_material := _glow(Color(1.0, 0.85, 0.35, 0.7))
# Projectiles in flight, by id: {"node", "from", "velocity", "gravity", "age"}.
var _projectiles := {}

const BURST_LIFE := 0.35


func _init() -> void:
	_tracer_mesh.size = Vector3(TRACER_WIDTH, TRACER_WIDTH, 1.0)
	_tracer_mesh.material = _tracer_material
	_flash_mesh.radius = 0.09
	_flash_mesh.height = 0.18
	_flash_mesh.material = _flash_material
	_wheel_mesh.top_radius = 0.16
	_wheel_mesh.bottom_radius = 0.16
	_wheel_mesh.height = 0.12
	_wheel_mesh.material = _glow(WeaponModels.GOUDA)
	_burst_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA


## A shot fired from `muzzle` whose pellets ended at `ends`.
func show_shot(muzzle: Vector3, ends: Array) -> void:
	_flash(muzzle)
	for end: Variant in ends:
		if end is Vector3:
			_tracer(muzzle, end)


## A projectile `id` launched from `from` at `velocity`, drawn along its path
## until it bursts.
func launch_projectile(id: int, weapon: String, from: Vector3, velocity: Vector3) -> void:
	var stats := Weapons.find(weapon)
	var wheel := MeshInstance3D.new()
	wheel.mesh = _wheel_mesh
	wheel.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(wheel)
	wheel.global_position = from
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8) * (stats.projectile_gravity if stats != null else 0.0)
	_projectiles[id] = {"node": wheel, "from": from, "velocity": velocity, "gravity": gravity, "age": 0.0, "fuse": stats.fuse if stats != null else 3.0}


## Projectile `id` burst at `at`.
func burst_projectile(id: int, weapon: String, at: Vector3) -> void:
	if _projectiles.has(id):
		_projectiles[id].node.queue_free()
		_projectiles.erase(id)
	var stats := Weapons.find(weapon)
	var radius := stats.splash_radius if stats != null else 3.0
	var burst := MeshInstance3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.material = _burst_material
	burst.mesh = sphere
	burst.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(burst)
	burst.global_position = at
	burst.scale = Vector3.ONE * 0.2
	var light := OmniLight3D.new()
	light.light_color = FLASH_COLOR
	light.light_energy = 4.0
	light.omni_range = radius * 2.0
	burst.add_child(light)
	var grow := create_tween()
	grow.tween_property(burst, "scale", Vector3.ONE * radius, BURST_LIFE)
	_expire(burst, BURST_LIFE)


## How many projectiles are being drawn, for tests.
func projectiles_in_flight() -> int:
	return _projectiles.size()


func _process(delta: float) -> void:
	for id: int in _projectiles.keys():
		var flying: Dictionary = _projectiles[id]
		flying.age += delta
		# A burst that never arrives, such as one lost with its host, ends it.
		if flying.age > flying.fuse + 2.0:
			flying.node.queue_free()
			_projectiles.erase(id)
			continue
		flying.node.global_position = Projectile.position_at(flying.from, flying.velocity, flying.gravity, flying.age)
		flying.node.rotate_x(delta * 12.0)


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
