extends Node3D
## Short-lived effects in the arena that only show what happened: a shot's
## muzzle flash, the tracer of each pellet, the mark and the puff of dust
## where it lands, the casing thrown out of the weapon and the smoke left
## hanging. They have no effect on play, so each machine makes its own, and
## makes fewer of them, or none, on lower graphics.

const TRACER_LIFE := 0.09
const TRACER_WIDTH := 0.03
const FLASH_LIFE := 0.06
const TRACER_COLOR := Color(1.0, 0.96, 0.9)
const FLASH_COLOR := Color(1.0, 0.55, 0.25)

var _tracer_material := _glow(TRACER_COLOR)
var _flash_material := _glow(FLASH_COLOR)
var _tracer_mesh := BoxMesh.new()
var _flash_mesh := SphereMesh.new()
var _wheel_mesh := Rounded.cylinder(0.16, 0.12, 0.04)
var _wheel_material := _glow(WeaponModels.WAX)
var _grenade_mesh := SphereMesh.new()
var _burst_material := _glow(Color(1.0, 0.5, 0.2, 0.7))
# Projectiles in flight, by id: {"node", "from", "velocity", "gravity", "age"}.
var _projectiles := {}

const BURST_LIFE := 0.35
## Physics layer of the arena, which shots leave marks on.
const WORLD_MASK := 1
## Seconds a mark stays, fading over the last of them.
const MARK_LIFE := 14.0
const MARK_FADE := 3.0
## Seconds a casing is seen for.
const CASING_LIFE := 1.6
const CASING_COLOR := Color(0.72, 0.42, 0.26)

## Marks left by shots, oldest first: {"node", "left", "material"}.
var _marks: Array[Dictionary] = []
## Casings in the air or come to rest: {"node", "velocity", "floor", "left"}.
var _casings: Array[Dictionary] = []
var _casing_mesh := Rounded.cylinder(0.018, 0.07, 0.006, 8)
var _casing_material := StandardMaterial3D.new()
var _puff_mesh := SphereMesh.new()
## How many puffs of dust, sparks and smoke have been made, for tests.
var puffs_made := 0


## The most marks kept at once for graphics quality `level`; beyond it the
## oldest goes.
static func mark_limit(level: String) -> int:
	return {GraphicsQuality.LOW: 16, GraphicsQuality.MEDIUM: 40}.get(level, 80)


## Whether graphics quality `level` shows the lesser effects: puffs of dust,
## sparks, casings and smoke.
static func extras(level: String) -> bool:
	return level != GraphicsQuality.LOW


func _quality() -> String:
	return GraphicsQuality.level(Settings.get_value("quality"))


func _init() -> void:
	_tracer_mesh.size = Vector3(TRACER_WIDTH, TRACER_WIDTH, 1.0)
	_tracer_mesh.material = _tracer_material
	_flash_mesh.radius = 0.09
	_flash_mesh.height = 0.18
	_flash_mesh.material = _flash_material
	_burst_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_casing_material.albedo_color = CASING_COLOR
	_casing_material.metallic = 0.6
	_casing_material.roughness = 0.4
	_puff_mesh.radius = 0.5
	_puff_mesh.height = 1.0
	_puff_mesh.radial_segments = 6
	_puff_mesh.rings = 3
	_grenade_mesh.radius = 0.14
	_grenade_mesh.height = 0.28
	_grenade_mesh.radial_segments = 12
	_grenade_mesh.rings = 6


## A shot fired from `muzzle` whose pellets ended at `ends`. A casing is
## thrown out to `aside`, the weapon's right, if that is given.
func show_shot(muzzle: Vector3, ends: Array, aside := Vector3.ZERO) -> void:
	_flash(muzzle)
	for end: Variant in ends:
		if end is Vector3:
			_tracer(muzzle, end)
			_impact(muzzle, end)
	if extras(_quality()):
		if aside != Vector3.ZERO and not ends.is_empty():
			_casing(muzzle, aside)
		_puff(muzzle, Vector3.UP, Color(0.75, 0.75, 0.78, 0.35), 3, 0.6, 0.5, 0.22)


## How many effects of `kind` ("tracer", "flash", "puff") are showing.
func showing(kind: String) -> int:
	return get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == kind and not child.is_queued_for_deletion()).size()


## How many marks shots have left that are still showing.
func marks_showing() -> int:
	return _marks.size()


func casings_showing() -> int:
	return _casings.size()


# What a shot from `from` does where it ended, at `to`: if that is on the
# arena, a mark, and a puff of what the surface is made of.
func _impact(from: Vector3, to: Vector3) -> void:
	if not is_inside_tree() or from.distance_to(to) < 0.01:
		return
	var along := (to - from).normalized()
	var query := PhysicsRayQueryParameters3D.create(to - along * 0.3, to + along * 0.3, WORLD_MASK)
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if hit.is_empty():
		return
	var body: Node = hit.collider
	# The arena's edge is solid and can't be seen: nothing shows on it.
	if String(body.name).begins_with("Wall"):
		return
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.08, 0.08, 0.09, 0.75)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var mark := Rounded.spot(0.09, material, hit.position, hit.normal, 0.012)
	add_child(mark)
	mark.global_position = hit.position + hit.normal * 0.012
	_marks.append({"node": mark, "left": MARK_LIFE, "material": material})
	while _marks.size() > mark_limit(_quality()):
		_marks.pop_front().node.queue_free()
	if extras(_quality()):
		_puff(hit.position, hit.normal, Surfaces.dust(body), 7, 3.5, 0.4, 0.1)
		if Surfaces.sparks(body):
			_puff(hit.position, hit.normal, Color(1.0, 0.8, 0.5), 8, 9.0, 0.22, 0.04, true)


# A few specks of `colour` thrown out from `at` along `normal`, at about
# `speed` metres a second, lasting `life` seconds, each `size` across.
func _puff(at: Vector3, normal: Vector3, colour: Color, count: int, speed: float, life: float, size: float, bright := false) -> void:
	if not is_inside_tree():
		return
	puffs_made += 1
	var specks := CPUParticles3D.new()
	specks.one_shot = true
	specks.explosiveness = 0.95
	specks.amount = count
	specks.lifetime = life
	specks.direction = normal
	specks.spread = 50.0
	specks.initial_velocity_min = speed * 0.4
	specks.initial_velocity_max = speed
	specks.gravity = Vector3.DOWN * (9.0 if speed > 1.0 else -0.6)
	specks.scale_amount_min = size * 0.6
	specks.scale_amount_max = size
	specks.mesh = _puff_mesh
	var material := _glow(colour) if bright else StandardMaterial3D.new()
	material.albedo_color = colour
	if colour.a < 1.0:
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	specks.material_override = material
	specks.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	specks.set_meta("kind", "puff")
	add_child(specks)
	specks.global_position = at
	specks.emitting = true
	_expire(specks, life + 0.3)


# A spent casing flicked out of a weapon at `muzzle`, to `aside`.
func _casing(muzzle: Vector3, aside: Vector3) -> void:
	if not is_inside_tree():
		return
	var casing := Rounded.instance(_casing_mesh, _casing_material)
	casing.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(casing)
	casing.global_position = muzzle + aside * 0.15
	# The ground under it, found once, for it to bounce on.
	var below := PhysicsRayQueryParameters3D.create(muzzle, muzzle + Vector3.DOWN * 12.0, WORLD_MASK)
	var ground := get_world_3d().direct_space_state.intersect_ray(below)
	var velocity := aside.normalized() * randf_range(2.0, 3.2) + Vector3.UP * randf_range(2.0, 3.0)
	_casings.append({"node": casing, "velocity": velocity, "floor": ground.position.y if not ground.is_empty() else -INF, "left": CASING_LIFE})
	while _casings.size() > 24:
		_casings.pop_front().node.queue_free()


## A projectile `id` launched from `from` at `velocity`, drawn along its path
## until it bursts.
func launch_projectile(id: int, weapon: String, from: Vector3, velocity: Vector3) -> void:
	var stats := Weapons.find(weapon)
	var wheel := MeshInstance3D.new()
	wheel.mesh = _grenade_mesh if stats != null and stats.bounces else _wheel_mesh
	wheel.material_override = _wheel_material
	wheel.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(wheel)
	wheel.global_position = from
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8) * (stats.projectile_gravity if stats != null else 0.0)
	_projectiles[id] = {"node": wheel, "from": from, "velocity": velocity, "gravity": gravity, "age": 0.0, "fuse": stats.fuse if stats != null else 3.0}
	if stats != null and stats.bounces:
		# One that bounces has no simple path: follow it through this
		# machine's own copy of the arena, which is the same as the host's.
		_projectiles[id].flight = Projectile.new(id, 0, stats, from, velocity)


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
	for mark in _marks:
		mark.left -= delta
		mark.material.albedo_color.a = 0.75 * clampf(mark.left / MARK_FADE, 0.0, 1.0)
	while not _marks.is_empty() and _marks[0].left <= 0.0:
		_marks.pop_front().node.queue_free()
	for casing in _casings:
		casing.left -= delta
		var node: Node3D = casing.node
		if casing.velocity != Vector3.ZERO:
			casing.velocity += Vector3.DOWN * 18.0 * delta
			node.global_position += casing.velocity * delta
			node.rotate_x(delta * 20.0)
			if node.global_position.y <= casing.floor + 0.02:
				node.global_position.y = casing.floor + 0.02
				# One bounce, then it lies where it fell.
				casing.velocity = casing.velocity.bounce(Vector3.UP) * 0.35 if casing.velocity.y < -2.0 else Vector3.ZERO
	while not _casings.is_empty() and _casings[0].left <= 0.0:
		_casings.pop_front().node.queue_free()
	for id: int in _projectiles.keys():
		var flying: Dictionary = _projectiles[id]
		flying.age += delta
		# A burst that never arrives, such as one lost with its host, ends it.
		if flying.age > flying.fuse + 2.0:
			flying.node.queue_free()
			_projectiles.erase(id)
			continue
		if flying.has("flight"):
			var flight: Projectile = flying.flight
			# Where it bursts is the host's to say: until then, it is only moved.
			flight.advance(delta, Match.wall_finder(get_world_3d().direct_space_state), {})
			flying.node.global_position = flight.position
			if not flight.resting:
				flying.node.rotate_x(delta * 9.0)
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
	tracer.set_meta("kind", "tracer")
	add_child(tracer)
	tracer.global_transform = tracer_transform(from, to)
	_expire(tracer, TRACER_LIFE)


func _flash(at: Vector3) -> void:
	var flash := MeshInstance3D.new()
	flash.mesh = _flash_mesh
	flash.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	flash.set_meta("kind", "flash")
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
