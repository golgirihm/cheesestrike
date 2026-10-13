extends Node3D
## Short-lived effects in the arena that only show what happened: a shot's
## muzzle flash, the tracer of each pellet, the mark and the puff of dust
## where it lands, the casing thrown out of the weapon and the smoke left
## hanging. They have no effect on play, so each machine makes its own, and
## makes fewer of them, or none, on lower graphics.

## A tracer is a short bright streak that flies from the muzzle to where the
## shot ended: how fast, in metres a second, and how long the streak is.
const TRACER_SPEED := 260.0
const TRACER_LENGTH := 7.0
const TRACER_WIDTH := 0.03
## The weapons whose shots leave a faint line hanging in the air, seconds it
## takes to fade, and how wide it is.
const TRAILED := ["rifle"]
const TRAIL_LIFE := 0.7
const TRAIL_WIDTH := 0.02
## Seconds between the wisps of smoke a launcher's wheel leaves behind it.
const WISP_EVERY := 0.05
const FLASH_LIFE := 0.06
const TRACER_COLOR := Color(1.0, 0.96, 0.9)
const FLASH_COLOR := Color(1.0, 0.55, 0.25)

var _tracer_material := _glow(TRACER_COLOR)
var _flash_material := _glow(FLASH_COLOR)
var _tracer_mesh := BoxMesh.new()
var _trail_mesh := BoxMesh.new()
var _flash_mesh := SphereMesh.new()
var _wheel_mesh := Rounded.cylinder(0.16, 0.12, 0.04)
var _wheel_material := _glow(WeaponModels.WAX)
var _grenade_mesh := SphereMesh.new()
var _ball_mesh := SphereMesh.new()
var _ring_mesh := TorusMesh.new()
var _lump_mesh := BoxMesh.new()
## Lumps thrown up by bursts: as casings are.
var _debris: Array[Dictionary] = []
# Projectiles in flight, by id: {"node", "from", "velocity", "gravity", "age"}.
var _projectiles := {}
# Tracers in flight: {"node", "from", "to", "age"}.
var _tracers: Array[Dictionary] = []

## Seconds a burst's shockwave takes to reach its edge.
const BURST_LIFE := 0.35
## Seconds its flash lasts, and how big it gets, as a share of the burst.
const BURST_FLASH_LIFE := 0.16
const BURST_FLASH_SIZE := 0.8
## How wide the ground is burnt under a burst, as a share of its radius, and
## how dark.
const SCORCH_SIZE := 0.45
const SCORCH_ALPHA := 0.55
## Seconds a lump thrown up by a burst is seen for.
const DEBRIS_LIFE := 3.0
## The colour of the sparkle each pickup gives; a weapon's is wax.
const SPARKLES := {Pickups.HEALTH: Color(1.0, 0.45, 0.4), Pickups.AMMO: Color(1.0, 0.85, 0.4), Pickups.ARMOUR: Color(0.5, 0.75, 1.0), Pickups.POWER: Color(0.8, 0.5, 1.0)}
## Seconds the smoke of a burst hangs about.
const SMOKE_LIFE := 2.2
## Physics layer of the arena, which shots leave marks on.
const WORLD_MASK := 1
## Seconds a mark stays, fading over the last of them.
const MARK_LIFE := 14.0
const MARK_FADE := 3.0
## Seconds a casing is seen for.
const CASING_LIFE := 1.6
const CASING_COLOR := Color(0.72, 0.42, 0.26)

## Seconds a footprint stays, fading all the while.
const PRINT_LIFE := 5.0
## Footprints, oldest first: {"node", "left", "material"}.
var _prints: Array[Dictionary] = []
# Which foot the next print is of, for each walker by where they last trod.
var _left_foot := false

## Marks left by shots, oldest first: {"node", "left", "material"}.
var _marks: Array[Dictionary] = []
## Casings in the air or come to rest: {"node", "velocity", "floor", "left"}.
var _casings: Array[Dictionary] = []
var _casing_mesh := Rounded.cylinder(0.018, 0.07, 0.006, 8)
var _casing_material := StandardMaterial3D.new()
var _puff_mesh := SphereMesh.new()
## How many puffs of dust, sparks and smoke have been made, for tests.
var puffs_made := 0
# The puffs in the air: {"amount": how many specks, "left": seconds}.
var _live: Array[Dictionary] = []


## The most marks kept at once for graphics quality `level`; beyond it the
## oldest goes.
static func mark_limit(level: String) -> int:
	return {GraphicsQuality.LOW: 16, GraphicsQuality.MEDIUM: 40}.get(level, 80)


## Whether graphics quality `level` shows the lesser effects: puffs of dust,
## sparks, casings and smoke.
static func extras(level: String) -> bool:
	return level != GraphicsQuality.LOW


func _quality() -> String:
	return GraphicsQuality.choice(GraphicsQuality.EFFECTS)


func _init() -> void:
	_tracer_mesh.size = Vector3(TRACER_WIDTH, TRACER_WIDTH, 1.0)
	_tracer_mesh.material = _tracer_material
	_trail_mesh.size = Vector3(TRAIL_WIDTH, TRAIL_WIDTH, 1.0)
	_flash_mesh.radius = 0.09
	_flash_mesh.height = 0.18
	_flash_mesh.material = _flash_material
	_ball_mesh.radius = 1.0
	_ball_mesh.height = 2.0
	_ball_mesh.radial_segments = 16
	_ball_mesh.rings = 8
	# A ring a metre across, thin and flat, to be stretched to a burst's size.
	_ring_mesh.inner_radius = 0.9
	_ring_mesh.outer_radius = 1.0
	_ring_mesh.rings = 32
	_ring_mesh.ring_segments = 4
	_lump_mesh.size = Vector3(0.16, 0.12, 0.2)
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
## thrown out to `aside`, the weapon's right, if that is given. `weapon` is
## what fired it, if that is known.
func show_shot(muzzle: Vector3, ends: Array, aside := Vector3.ZERO, weapon := "") -> void:
	_flash(muzzle)
	for end: Variant in ends:
		if end is Vector3:
			_tracer(muzzle, end)
			_impact(muzzle, end)
			if TRAILED.has(weapon):
				_trail(muzzle, end)
	if extras(_quality()):
		if aside != Vector3.ZERO and not ends.is_empty():
			_casing(muzzle, aside)
		_puff(muzzle, Vector3.UP, Color(0.75, 0.75, 0.78, 0.35), 3, 0.6, 0.5, 0.22)


## How many effects of `kind` ("tracer", "flash", "puff") are showing.
func showing(kind: String) -> int:
	return get_children().filter(func(child: Node) -> bool: return child.get_meta("kind", "") == kind and not child.is_queued_for_deletion()).size()


## A footprint where a player at `feet`, turned by `yaw`, has just trodden on
## the floor of `map`: left and right in turn, fading away. Not on paving,
## nor on graphics without the lesser effects.
func footprint(feet: Vector3, yaw: float, map: Node) -> void:
	if not is_inside_tree() or not extras(_quality()) or not Surfaces.takes_prints(map):
		return
	var below := PhysicsRayQueryParameters3D.create(feet + Vector3.UP * 0.3, feet + Vector3.DOWN * 0.5, WORLD_MASK)
	var ground := get_world_3d().direct_space_state.intersect_ray(below)
	if ground.is_empty():
		return
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.1, 0.07, 0.05, 0.35)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var mark := Rounded.spot(0.16, material, Vector3.ZERO, Vector3.UP, 0.0)
	add_child(mark)
	_left_foot = not _left_foot
	var aside := Basis(Vector3.UP, yaw) * Vector3(-0.45 if _left_foot else 0.45, 0, 0)
	# A shoe's shape: longer than it is wide, pointing the way they face.
	mark.global_transform = Transform3D(Basis(Vector3.UP, yaw) * Basis.from_scale(Vector3(0.8, 1.0, 1.7)), ground.position + aside + Vector3.UP * 0.015)
	_prints.append({"node": mark, "left": PRINT_LIFE, "material": material})
	while _prints.size() > 48:
		_prints.pop_front().node.queue_free()


func prints_showing() -> int:
	return _prints.size()


## Crumbs of `colour` thrown out from `at`: `count` of them, as when a
## character is knocked out. More, and further, for one blown apart.
func crumbs(at: Vector3, colour: Color, count: int, speed := 5.0) -> void:
	_puff(at, Vector3.UP, colour, count, speed, 0.7, 0.22, false, {"matters": true})


## Dust kicked up where a player comes down at `feet`, more of it the
## harder they land (`hardness` from 0 to 1).
func landing(feet: Vector3, hardness: float, colour := Color(0.62, 0.55, 0.45, 0.6)) -> void:
	if hardness > 0.2:
		_puff(feet + Vector3.UP * 0.1, Vector3.UP, colour, roundi(lerpf(6.0, 18.0, hardness)), lerpf(2.0, 4.5, hardness), 0.6, 0.3, false, {"spread": 88.0, "gravity": 3.0, "grow": 1.8})


## A sparkle rising from `at`, where a pickup of `kind` was taken, in its
## colour.
func sparkle(at: Vector3, kind: String) -> void:
	var colour: Color = SPARKLES.get(kind, WeaponModels.WAX)
	_puff(at + Vector3.UP * 0.6, Vector3.UP, colour, 12, 3.0, 0.6, 0.09, true, {"spread": 40.0, "gravity": -1.5, "grow": 0.2})


## Fire and sparks thrown up round a player at `at` who sets off an Overload.
func overload_flare(at: Vector3, colour: Color) -> void:
	_puff(at, Vector3.UP, Color(colour, 0.85), 34, 6.0, 0.8, 0.34, true, {"spread": 75.0, "gravity": -4.0, "grow": 0.3, "matters": true})
	_puff(at, Vector3.UP, Color(1.0, 0.9, 0.6), 22, 11.0, 0.5, 0.06, true, {"spread": 90.0, "matters": true})


## What a burst of `radius` metres at `at` throws out: fire that is gone in a
## moment, sparks, and smoke that hangs and drifts up.
func burst_specks(at: Vector3, radius: float) -> void:
	_puff(at, Vector3.UP, Color(1.0, 0.6, 0.2, 0.9), 18, radius * 2.2, 0.35, radius * 0.22, true, {"spread": 180.0, "gravity": -2.0, "grow": 0.3, "matters": true})
	_puff(at, Vector3.UP, Color(1.0, 0.85, 0.5), 26, radius * 4.5, 0.5, 0.06, true, {"spread": 120.0, "matters": true})
	_puff(at, Vector3.UP, Color(0.25, 0.24, 0.24, 0.42), 14, radius * 0.6, SMOKE_LIFE, radius * 0.3, false, {"spread": 180.0, "gravity": -0.8, "grow": 2.6, "matters": true})


## Clears every effect away at once.
func clear() -> void:
	for child in get_children():
		child.queue_free()
	for kept: Array in [_marks, _prints, _casings, _debris, _tracers, _live]:
		kept.clear()
	_projectiles.clear()


## How many specks are in the air.
func specks_showing() -> int:
	var total := 0
	for puff in _live:
		total += puff.amount
	return total


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
# `speed` metres a second, lasting `life` seconds, each `size` across. As
# many of `count` as there is room for (see ParticleBudget). `look` can
# give: "spread" in degrees, "gravity" downward (negative to rise), "grow",
# how many times its size a speck ends at, and "matters", for an effect that
# shows something happening.
func _puff(at: Vector3, normal: Vector3, colour: Color, count: int, speed: float, life: float, size: float, bright := false, look := {}) -> void:
	if not is_inside_tree():
		return
	count = ParticleBudget.grant(count, specks_showing(), GraphicsQuality.choice(GraphicsQuality.PARTICLES), look.get("matters", false))
	if count == 0:
		return
	puffs_made += 1
	_live.append({"amount": count, "left": life})
	var specks := CPUParticles3D.new()
	specks.one_shot = true
	specks.explosiveness = 0.95
	specks.amount = count
	specks.lifetime = life
	specks.direction = normal
	specks.spread = look.get("spread", 50.0)
	specks.initial_velocity_min = speed * 0.4
	specks.initial_velocity_max = speed
	specks.gravity = Vector3.DOWN * look.get("gravity", 9.0 if speed > 1.0 else -0.6)
	specks.scale_amount_min = size * 0.6
	specks.scale_amount_max = size
	if look.has("grow"):
		# From its size to `grow` times it, or the other way for one that shrinks.
		var most: float = maxf(look.grow, 1.0)
		var curve := Curve.new()
		curve.add_point(Vector2(0.0, 1.0 / most))
		curve.add_point(Vector2(1.0, look.grow / most))
		specks.scale_amount_curve = curve
		specks.scale_amount_min *= most
		specks.scale_amount_max *= most
	specks.mesh = _puff_mesh
	var material := _glow(colour) if bright else StandardMaterial3D.new()
	material.albedo_color = colour
	if colour.a < 1.0:
		# Something see-through thins away rather than vanishing.
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.vertex_color_use_as_albedo = true
		var fade := Gradient.new()
		fade.set_color(0, Color.WHITE)
		fade.set_color(1, Color(1, 1, 1, 0))
		specks.color_ramp = fade
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
	var stick := stats != null and stats.splash_radius <= 0.0
	if stick:
		wheel.mesh = _stick_mesh()
	else:
		wheel.mesh = _grenade_mesh if stats != null and stats.bounces else _wheel_mesh
		wheel.material_override = _wheel_material
	wheel.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(wheel)
	wheel.global_position = from
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity", 9.8) * (stats.projectile_gravity if stats != null else 0.0) * Projectile.gravity_scale
	_projectiles[id] = {"node": wheel, "from": from, "velocity": velocity, "gravity": gravity, "age": 0.0, "fuse": stats.fuse if stats != null else 3.0, "stick": stick}
	# A launcher's wheel smokes as it goes.
	_projectiles[id].wisp = 0.0 if stats != null and not stick and not stats.bounces else INF
	if stick:
		wheel.global_transform = stick_transform(from, velocity)
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
	if radius <= 0.0:
		# It only struck: nothing bursts.
		return
	_burst_flash(at, radius)
	_shockwave(at, radius)
	burst_specks(at, radius)
	# What is under it, if it is near enough to be marked and torn up.
	if not is_inside_tree():
		return
	var below := PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.2, at + Vector3.DOWN * radius * 0.6, WORLD_MASK)
	var ground := get_world_3d().direct_space_state.intersect_ray(below)
	if ground.is_empty() or String(ground.collider.name).begins_with("Wall"):
		return
	_scorch(ground.position, ground.normal, radius)
	_debris_from(ground.position, Surfaces.dust(ground.collider), radius)


# The flash of a burst: a white-hot ball that swells to a part of the
# burst's size and is gone almost at once, lighting what is round it.
func _burst_flash(at: Vector3, radius: float) -> void:
	var flash := MeshInstance3D.new()
	var material := _glow(Color(1.0, 0.9, 0.65, 0.95))
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	flash.mesh = _ball_mesh
	flash.material_override = material
	flash.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	flash.set_meta("kind", "burst_flash")
	add_child(flash)
	flash.global_position = at
	flash.scale = Vector3.ONE * radius * 0.15
	var light := OmniLight3D.new()
	light.light_color = FLASH_COLOR
	light.light_energy = 6.0
	light.omni_range = radius * 2.5
	flash.add_child(light)
	var going := create_tween().set_parallel()
	going.tween_property(flash, "scale", Vector3.ONE * radius * BURST_FLASH_SIZE, BURST_FLASH_LIFE).set_ease(Tween.EASE_OUT)
	going.tween_property(material, "albedo_color:a", 0.0, BURST_FLASH_LIFE).set_ease(Tween.EASE_IN)
	going.tween_property(light, "light_energy", 0.0, BURST_FLASH_LIFE)
	_expire(flash, BURST_FLASH_LIFE)


# The shockwave of a burst: a thin ring that races out along the ground to
# the burst's edge, showing how far it reached.
func _shockwave(at: Vector3, radius: float) -> void:
	var ring := MeshInstance3D.new()
	var material := _glow(Color(1.0, 0.85, 0.7, 0.6))
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	ring.mesh = _ring_mesh
	ring.material_override = material
	ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	ring.set_meta("kind", "shockwave")
	add_child(ring)
	ring.global_position = at
	ring.scale = Vector3(0.1, 1.0, 0.1)
	var going := create_tween().set_parallel()
	going.tween_property(ring, "scale", Vector3(radius, 1.0, radius), BURST_LIFE).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_QUAD)
	going.tween_property(material, "albedo_color:a", 0.0, BURST_LIFE).set_ease(Tween.EASE_IN)
	_expire(ring, BURST_LIFE)


# The ground burnt black under a burst, which stays as a shot's mark does.
func _scorch(at: Vector3, normal: Vector3, radius: float) -> void:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.06, 0.05, 0.05, SCORCH_ALPHA)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var mark := Rounded.spot(1.0, material, Vector3.ZERO, normal, 0.0)
	mark.set_meta("kind", "scorch")
	add_child(mark)
	mark.global_position = at + normal * 0.02
	mark.scale = Vector3(radius * SCORCH_SIZE, 1.0, radius * SCORCH_SIZE)
	_marks.append({"node": mark, "left": MARK_LIFE, "material": material, "alpha": SCORCH_ALPHA})
	while _marks.size() > mark_limit(_quality()):
		_marks.pop_front().node.queue_free()


## How many lumps a burst throws up from the ground on graphics quality
## `level`.
static func debris_count(level: String) -> int:
	return {GraphicsQuality.LOW: 0, GraphicsQuality.MEDIUM: 5}.get(level, 9)


# Lumps of the ground, in `colour`, flung up and out from under a burst, to
# fall, bounce once and lie a while.
func _debris_from(at: Vector3, colour: Color, radius: float) -> void:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(colour.darkened(0.25), 1.0)
	material.roughness = 1.0
	for index in debris_count(_quality()):
		var lump := Rounded.instance(_lump_mesh, material)
		lump.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		lump.set_meta("kind", "debris")
		add_child(lump)
		lump.global_position = at + Vector3.UP * 0.1
		lump.scale = Vector3.ONE * randf_range(0.6, 1.4)
		var out := Vector3.RIGHT.rotated(Vector3.UP, randf() * TAU)
		var velocity := out * randf_range(0.4, 1.0) * radius * 1.4 + Vector3.UP * randf_range(5.0, 9.0)
		_debris.append({"node": lump, "velocity": velocity, "floor": at.y, "left": DEBRIS_LIFE})
	while _debris.size() > 36:
		_debris.pop_front().node.queue_free()


## How many lumps thrown up by bursts are in the air or lying about.
func debris_showing() -> int:
	return _debris.size()


## How many projectiles are being drawn, for tests.
func projectiles_in_flight() -> int:
	return _projectiles.size()


func _process(delta: float) -> void:
	for index in range(_tracers.size() - 1, -1, -1):
		if not _place_tracer(_tracers[index], delta):
			_tracers[index].node.queue_free()
			_tracers.remove_at(index)
	for puff in _live:
		puff.left -= delta
	_live = _live.filter(func(puff: Dictionary) -> bool: return puff.left > 0.0)
	for mark in _marks:
		mark.left -= delta
		mark.material.albedo_color.a = mark.get("alpha", 0.75) * clampf(mark.left / MARK_FADE, 0.0, 1.0)
	while not _marks.is_empty() and _marks[0].left <= 0.0:
		_marks.pop_front().node.queue_free()
	for trodden in _prints:
		trodden.left -= delta
		trodden.material.albedo_color.a = 0.35 * clampf(trodden.left / PRINT_LIFE, 0.0, 1.0)
	while not _prints.is_empty() and _prints[0].left <= 0.0:
		_prints.pop_front().node.queue_free()
	for thrown: Array[Dictionary] in [_casings, _debris]:
		for casing in thrown:
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
		while not thrown.is_empty() and thrown[0].left <= 0.0:
			thrown.pop_front().node.queue_free()
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
		var at := Projectile.position_at(flying.from, flying.velocity, flying.gravity, flying.age)
		if flying.stick:
			# A stick flies point first, tipping over as it falls.
			flying.node.global_transform = stick_transform(at, flying.velocity + Vector3.DOWN * flying.gravity * flying.age)
			continue
		flying.node.global_position = at
		flying.node.rotate_x(delta * 12.0)
		flying.wisp -= delta
		if flying.wisp <= 0.0:
			flying.wisp = WISP_EVERY
			_puff(at, Vector3.UP, Color(0.85, 0.82, 0.78, 0.4), 1, 0.3, 0.55, 0.12, false, {"spread": 180.0, "gravity": -0.4, "grow": 2.4})


## Where a stick at `at` flying along `velocity` sits: its length (the
## mesh's Y) along the way it goes.
static func stick_transform(at: Vector3, velocity: Vector3) -> Transform3D:
	var way := velocity.normalized() if velocity.length() > 0.001 else Vector3.FORWARD
	return Transform3D(Basis(Quaternion(Vector3.UP, way)), at)


static var _stick: Mesh


# A breadstick: long, thin and baked brown.
static func _stick_mesh() -> Mesh:
	if _stick == null:
		var stick := CylinderMesh.new()
		stick.top_radius = 0.018
		stick.bottom_radius = 0.022
		stick.height = 0.55
		stick.radial_segments = 8
		stick.rings = 1
		var crust := StandardMaterial3D.new()
		crust.albedo_color = WeaponModels.CRUST
		crust.roughness = 0.9
		stick.material = crust
		_stick = stick
	return _stick


## Where the tracer from `from` to `to` sits: a unit-long box stretched along
## the line between them and centred on it.
static func tracer_transform(from: Vector3, to: Vector3) -> Transform3D:
	var length := from.distance_to(to)
	if length < 0.001:
		return Transform3D(Basis.from_scale(Vector3(1, 1, 0.001)), from)
	var up := Vector3.UP if absf((to - from).normalized().y) < 0.99 else Vector3.RIGHT
	var basis := Basis.looking_at(to - from, up).scaled_local(Vector3(1, 1, length))
	return Transform3D(basis, (from + to) / 2.0)


## How far along its path of `length` metres a tracer's tail (x) and head
## (y) are, `age` seconds after the shot: the streak leaves the muzzle,
## flies, and runs into where the shot ended.
static func tracer_span(length: float, age: float) -> Vector2:
	var head := minf(TRACER_SPEED * maxf(age, 0.0), length)
	var tail := clampf(TRACER_SPEED * age - TRACER_LENGTH, 0.0, length)
	return Vector2(tail, head)


## Where each tracer in flight is going, for tests.
func tracer_ends() -> Array:
	return _tracers.map(func(tracer: Dictionary) -> Vector3: return tracer.to)


## Seconds a tracer along `length` metres is seen for.
static func tracer_life(length: float) -> float:
	return (length + TRACER_LENGTH) / TRACER_SPEED


func _tracer(from: Vector3, to: Vector3) -> void:
	var tracer := MeshInstance3D.new()
	tracer.mesh = _tracer_mesh
	tracer.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	tracer.set_meta("kind", "tracer")
	add_child(tracer)
	_tracers.append({"node": tracer, "from": from, "to": to, "age": 0.0})
	# As it is on the frame of the shot: just leaving the muzzle.
	_place_tracer(_tracers[-1], 1.0 / 120.0)


# Moves a tracer on by `delta` seconds. Returns whether it is still flying.
func _place_tracer(tracer: Dictionary, delta: float) -> bool:
	tracer.age += delta
	var length: float = tracer.from.distance_to(tracer.to)
	if tracer.age >= tracer_life(length) or length < 0.001:
		return false
	var span := tracer_span(length, tracer.age)
	var along: Vector3 = (tracer.to - tracer.from) / length
	tracer.node.global_transform = tracer_transform(tracer.from + along * span.x, tracer.from + along * span.y)
	return true


# The faint line a heavy shot leaves hanging in the air from `from` to `to`.
func _trail(from: Vector3, to: Vector3) -> void:
	var trail := MeshInstance3D.new()
	var material := _glow(Color(TRACER_COLOR, 0.35))
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	trail.mesh = _trail_mesh
	trail.material_override = material
	trail.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	trail.set_meta("kind", "trail")
	add_child(trail)
	trail.global_transform = tracer_transform(from, to)
	create_tween().tween_property(material, "albedo_color:a", 0.0, TRAIL_LIFE)
	_expire(trail, TRAIL_LIFE)


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
