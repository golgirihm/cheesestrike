extends Node3D
## Things that move in and around an arena for the look of it: flags on the
## fence, birds overhead, the shadows of clouds crossing the ground, and in
## the city traffic lights changing, steam rising from grates and a train
## going by beyond the fence.
##
## None of it is solid, none of it is part of the match, and no machine tells
## another about it: each draws its own, moved by the clock. The world adds
## this as a child of the arena.

const MEADOW := "meadow"
const CANYON := "canyon"
const CITY := "city"
const INK := Color(0.1, 0.1, 0.12)
const PENNANT := Color(0.86, 0.2, 0.18)
## Seconds a traffic light stays on each of its lamps: red, green, amber.
const LIGHT_TIMES := [6.0, 5.0, 1.5]
const LIGHT_COLOURS := [Color(0.95, 0.15, 0.12), Color(0.15, 0.85, 0.3), Color(1.0, 0.6, 0.1)]
## Seconds between one train and the next, and how fast it goes.
const TRAIN_EVERY := 40.0
const TRAIN_SPEED := 45.0

var _time := 0.0
var _edge := Rect2()
var _flags: Array[Node3D] = []
var _birds: Array[Node3D] = []
var _shadows: Array[Node3D] = []
var _lights: Array[Dictionary] = []
var _train: Node3D
var _train_from := 0.0


## Builds what moves for an arena of `kind` whose edge is `edge` (see
## Scenery.bounds). With `extras` off, as on low graphics, only what costs
## next to nothing is made.
func build(kind: String, edge: Rect2, extras := true) -> void:
	name = "AmbientLife"
	_edge = edge
	var corners := [edge.position, Vector2(edge.end.x, edge.position.y), edge.end, Vector2(edge.position.x, edge.end.y)]
	for corner: Vector2 in corners:
		_flags.append(_flag(Vector3(corner.x, 0, corner.y)))
	if kind == CITY:
		for x: float in [-9.5, 9.5]:
			for z: float in [-9.5, 9.5]:
				_traffic_light(Vector3(x, 0, z), 0 if x * z > 0.0 else 1)
		_train = _build_train()
		_train_from = edge.position.y - 16.0
	if not extras:
		return
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(kind) + 3
	for index in 5:
		_birds.append(_bird(rng))
	for index in 4:
		_shadows.append(_cloud_shadow(rng))
	if kind == CITY:
		for at: Vector3 in [Vector3(3.5, 0, 10), Vector3(-10, 0, -3.5), Vector3(10, 0, 25)]:
			_steam(at)


## How many of each thing there are, for tests.
func counts() -> Dictionary:
	return {"flags": _flags.size(), "birds": _birds.size(), "shadows": _shadows.size(), "lights": _lights.size(), "train": 1 if _train != null else 0}


## Whether the shadows of clouds show: only under a sky with sun and cloud.
func show_cloud_shadows(on: bool) -> void:
	for shadow in _shadows:
		shadow.visible = on


## Which lamp of a traffic light is lit `time` seconds in, for a light that
## began on lamp `phase` (0 red, 1 green): 0 red, 1 green or 2 amber.
static func lamp_at(time: float, phase: int) -> int:
	var round := 0.0
	for seconds: float in LIGHT_TIMES:
		round += seconds
	# One that began on green is as far round as red's time has gone.
	var into := fmod(time + (LIGHT_TIMES[0] if phase == 1 else 0.0), round)
	for lamp in LIGHT_TIMES.size():
		if into < LIGHT_TIMES[lamp]:
			return lamp
		into -= LIGHT_TIMES[lamp]
	return 0


## How far along its track a train is `time` seconds in, in metres from
## where it comes into sight; below zero while there is none.
static func train_distance(time: float) -> float:
	var into := fmod(time, TRAIN_EVERY)
	return into * TRAIN_SPEED


func _process(delta: float) -> void:
	_time += delta
	for index in _flags.size():
		var cloth: Node3D = _flags[index].get_node("Cloth")
		cloth.rotation.y = sin(_time * 2.3 + index) * 0.35
		cloth.rotation.z = sin(_time * 3.1 + index * 2.0) * 0.08
	for bird in _birds:
		var round: float = _time * bird.get_meta("speed") + bird.get_meta("start")
		var reach: float = bird.get_meta("reach")
		var middle: Vector3 = bird.get_meta("middle")
		bird.position = middle + Vector3(cos(round) * reach, sin(round * 0.7) * 2.0, sin(round) * reach)
		bird.rotation.y = -round
		var flap := sin(_time * 7.0 + bird.get_meta("start")) * 0.5
		bird.get_node("Left").rotation.z = flap
		bird.get_node("Right").rotation.z = -flap
	for shadow in _shadows:
		var way: Vector3 = shadow.get_meta("way")
		shadow.position += way * delta
		# Off one side of the arena, it comes back on the other.
		var span := _edge.size.x + 80.0
		if shadow.position.x > _edge.end.x + 40.0:
			shadow.position.x -= span
	for light in _lights:
		var lit := lamp_at(_time, light.phase)
		for lamp in 3:
			(light.lamps[lamp] as MeshInstance3D).visible = lamp == lit
	if _train != null:
		var along := train_distance(_time)
		_train.position = Vector3(_edge.position.x - 120.0 + along, 0.0, _train_from)
		_train.visible = along < _edge.size.x + 240.0


# A pole with a pennant on it.
func _flag(at: Vector3) -> Node3D:
	var flag := Node3D.new()
	flag.position = at
	add_child(flag)
	flag.add_child(_part(Rounded.cylinder(0.08, 6.0, 0.03, 8), Color(0.85, 0.85, 0.88), Vector3(0, 3.0, 0)))
	var cloth := Node3D.new()
	cloth.name = "Cloth"
	cloth.position = Vector3(0, 5.4, 0)
	flag.add_child(cloth)
	cloth.add_child(_part(Rounded.box(Vector3(1.8, 1.0, 0.04), 0.01), PENNANT, Vector3(0.95, 0, 0)))
	return flag


# A bird, far overhead: a body and two wings that beat.
func _bird(rng: RandomNumberGenerator) -> Node3D:
	var bird := Node3D.new()
	bird.set_meta("middle", Vector3(rng.randf_range(-20, 20), rng.randf_range(26, 40), rng.randf_range(-20, 20)))
	bird.set_meta("reach", rng.randf_range(14.0, 34.0))
	bird.set_meta("speed", rng.randf_range(0.18, 0.32) * (1.0 if rng.randf() < 0.5 else -1.0))
	bird.set_meta("start", rng.randf_range(0.0, TAU))
	add_child(bird)
	bird.add_child(_part(Rounded.box(Vector3(0.25, 0.2, 0.9), 0.05), INK, Vector3.ZERO))
	for side: float in [-1.0, 1.0]:
		var wing := Node3D.new()
		wing.name = "Left" if side < 0.0 else "Right"
		bird.add_child(wing)
		wing.add_child(_part(Rounded.box(Vector3(1.1, 0.06, 0.45), 0.02), INK, Vector3(0.6 * side, 0, 0)))
	return bird


# The shadow of a cloud: a faint dark round on the ground, drifting.
func _cloud_shadow(rng: RandomNumberGenerator) -> Node3D:
	var shadow := MeshInstance3D.new()
	var round := CylinderMesh.new()
	round.top_radius = rng.randf_range(10.0, 18.0)
	round.bottom_radius = round.top_radius
	round.height = 0.01
	round.radial_segments = 24
	var dark := StandardMaterial3D.new()
	dark.albedo_color = Color(0, 0, 0.05, 0.13)
	dark.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	dark.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	round.material = dark
	shadow.mesh = round
	shadow.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	shadow.position = Vector3(rng.randf_range(_edge.position.x - 30.0, _edge.end.x + 30.0), 0.07, rng.randf_range(_edge.position.y, _edge.end.y))
	shadow.scale = Vector3(1.0, 1.0, rng.randf_range(0.6, 0.9))
	shadow.set_meta("way", Vector3(rng.randf_range(1.2, 2.2), 0, 0))
	add_child(shadow)
	return shadow


# A traffic light on a post, with three lamps of which one shows.
func _traffic_light(at: Vector3, phase: int) -> void:
	var post := Node3D.new()
	post.position = at
	add_child(post)
	post.add_child(_part(Rounded.cylinder(0.12, 6.0, 0.04, 8), INK.lightened(0.15), Vector3(0, 3.0, 0)))
	post.add_child(_part(Rounded.box(Vector3(0.7, 2.0, 0.7), 0.1), INK, Vector3(0, 6.6, 0)))
	var lamps := []
	for lamp in 3:
		var glow := StandardMaterial3D.new()
		glow.albedo_color = LIGHT_COLOURS[lamp]
		glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		var bulb := SphereMesh.new()
		bulb.radius = 0.42
		bulb.height = 0.84
		bulb.radial_segments = 10
		bulb.rings = 5
		# Red at the top, amber in the middle, green at the bottom.
		var height: float = [7.25, 5.95, 6.6][lamp]
		var shown := Rounded.instance(bulb, glow, Vector3(0, height, 0))
		shown.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		post.add_child(shown)
		lamps.append(shown)
	_lights.append({"phase": phase, "lamps": lamps})


# A train of a few carriages.
func _build_train() -> Node3D:
	var train := Node3D.new()
	add_child(train)
	for carriage in 5:
		var colour := Color(0.75, 0.2, 0.16) if carriage == 0 else Color(0.3, 0.42, 0.55)
		train.add_child(_part(Rounded.box(Vector3(16.0, 5.0, 4.0), 0.4), colour, Vector3(-carriage * 17.5, 3.4, 0)))
		train.add_child(_part(Rounded.box(Vector3(14.0, 1.4, 4.1), 0.1), INK.lightened(0.5), Vector3(-carriage * 17.5, 4.3, 0)))
	return train


# Steam rising from a grate in the road.
func _steam(at: Vector3) -> void:
	add_child(_part(Rounded.box(Vector3(1.6, 0.05, 1.0), 0.02), INK, at + Vector3.UP * 0.03))
	var steam := CPUParticles3D.new()
	steam.position = at + Vector3.UP * 0.1
	steam.amount = 14
	steam.lifetime = 3.0
	steam.direction = Vector3.UP
	steam.spread = 14.0
	steam.initial_velocity_min = 1.2
	steam.initial_velocity_max = 2.0
	steam.gravity = Vector3(0.3, 0.4, 0)
	steam.scale_amount_min = 0.6
	steam.scale_amount_max = 1.6
	steam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var puff := SphereMesh.new()
	puff.radius = 0.6
	puff.height = 1.2
	puff.radial_segments = 8
	puff.rings = 4
	var white := StandardMaterial3D.new()
	white.albedo_color = Color(0.92, 0.93, 0.95, 0.22)
	white.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	white.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	puff.material = white
	steam.mesh = puff
	add_child(steam)


func _part(mesh: Mesh, colour: Color, at: Vector3) -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = colour
	material.roughness = 0.8
	return Rounded.instance(mesh, material, at)
