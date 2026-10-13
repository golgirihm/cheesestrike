class_name Scenery
## Finishes an arena once its scene is loaded: what the scene lays out as
## plain boxes is drawn here with rounded edges, cheese gets its holes,
## buildings their windows and cars their bodies, a low fence goes up along
## the arena's edge, and the land beyond it is filled in.
##
## None of it changes play. What players and shots collide with is the
## scene's own collision shapes; everything added here is only drawn. The
## arena's edge is its four Wall bodies, which are tall, solid and not drawn:
## the fence only shows where they are.
##
## An arena's root says which surroundings it has in its `scenery` metadata.

const MEADOW := "meadow"
const CANYON := "canyon"
const CITY := "city"

## Cheese to eat. With the health pickups, the only yellow in the arena.
const CHEESE := Color(1.0, 0.82, 0.22)
const FENCE_HEIGHT := 1.5
## How far apart fence posts stand, about.
const POST_SPACING := 3.0
## How far out the land beyond the arena is filled in.
const REACH := 190.0

const INK := Color(0.1, 0.1, 0.12)
const CAR_COLOURS := [Color(0.8, 0.16, 0.14), Color(0.16, 0.4, 0.78), Color(0.9, 0.9, 0.88), Color(0.14, 0.55, 0.42), Color(0.22, 0.22, 0.26), Color(0.55, 0.3, 0.65)]
const BUILDING_COLOURS := [Color(0.5, 0.6, 0.72), Color(0.66, 0.4, 0.33), Color(0.72, 0.72, 0.7), Color(0.36, 0.42, 0.55), Color(0.58, 0.5, 0.46), Color(0.42, 0.52, 0.5)]

## How each kind of surroundings is coloured: the ground beyond the arena,
## and the fence's posts and rails.
const LOOKS := {
	MEADOW: {"ground": Color(0.3, 0.52, 0.26), "post": Color(0.95, 0.95, 0.93), "rail": Color(0.95, 0.95, 0.93)},
	CANYON: {"ground": Color(0.56, 0.34, 0.28), "post": Color(0.42, 0.27, 0.17), "rail": Color(0.5, 0.33, 0.2)},
	CITY: {"ground": Color(0.46, 0.47, 0.5), "post": Color(0.3, 0.31, 0.35), "rail": Color(0.78, 0.2, 0.16)},
}

static var _materials := {}
static var _window_texture: ImageTexture


## Finishes the arena `map`.
static func dress(map: Node3D) -> void:
	var geometry := map.get_node("Geometry")
	for body: Node3D in geometry.get_children():
		_round(body)
		if body.name.begins_with("Block"):
			_cheese(body)
		elif body.name.begins_with("Building"):
			_building(body)
		elif body.name.begins_with("Car"):
			_car(body)
		elif body.name.begins_with("Bus"):
			_bus(body)
	_fixtures(map)
	var kind := String(map.get_meta("scenery", MEADOW))
	if not LOOKS.has(kind):
		kind = MEADOW
	var around := Node3D.new()
	around.name = "Scenery"
	map.add_child(around)
	var edge := bounds(geometry)
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(kind)
	_fence(around, edge, LOOKS[kind])
	_ground(around, LOOKS[kind].ground)
	match kind:
		CANYON:
			_canyon(around, edge, rng)
		CITY:
			_city(around, edge, rng)
		_:
			_meadow(around, edge, rng)


## The arena's edge, seen from above: the rectangle its four walls' inner
## faces enclose, with X across and Z down.
static func bounds(geometry: Node) -> Rect2:
	var low := Vector2(-INF, -INF)
	var high := Vector2(INF, INF)
	for wall: Node3D in geometry.get_children():
		if not wall.name.begins_with("Wall"):
			continue
		var size: Vector3 = (wall.get_node("CollisionShape3D").shape as BoxShape3D).size
		var at := wall.position
		# A wall runs along its longer side, and faces the middle.
		if size.x > size.z:
			if at.z < 0.0:
				low.y = maxf(low.y, at.z + size.z / 2.0)
			else:
				high.y = minf(high.y, at.z - size.z / 2.0)
		elif at.x < 0.0:
			low.x = maxf(low.x, at.x + size.x / 2.0)
		else:
			high.x = minf(high.x, at.x - size.x / 2.0)
	return Rect2(low, high - low)


## Whether `colour` is the yellow kept for cheese that can be eaten.
static func is_yellow(colour: Color) -> bool:
	return colour.s > 0.35 and colour.v > 0.5 and colour.h > 0.105 and colour.h < 0.2


static func _material(colour: Color) -> StandardMaterial3D:
	if not _materials.has(colour):
		var material := StandardMaterial3D.new()
		material.albedo_color = colour
		material.roughness = 0.8
		_materials[colour] = material
	return _materials[colour]


# A material that takes each copy's own colour, for many things drawn at once.
static func _tinted() -> StandardMaterial3D:
	if not _materials.has("tinted"):
		var material := StandardMaterial3D.new()
		material.vertex_color_use_as_albedo = true
		material.roughness = 0.9
		_materials["tinted"] = material
	return _materials["tinted"]


# Draws the boxes of `body` with their edges rounded off.
static func _round(body: Node3D) -> void:
	for child in body.get_children():
		var drawn := child as MeshInstance3D
		if drawn == null or not drawn.mesh is BoxMesh:
			continue
		var box: BoxMesh = drawn.mesh
		var least := minf(box.size.x, minf(box.size.y, box.size.z))
		drawn.material_override = box.material
		drawn.mesh = Rounded.box(box.size, clampf(least * 0.2, 0.04, 0.3))


# The size of the box `body` is, from its collision shape.
static func _size(body: Node3D) -> Vector3:
	return (body.get_node("CollisionShape3D").shape as BoxShape3D).size


# A block of cheese: yellow, with holes on every side that shows.
static func _cheese(body: Node3D) -> void:
	var drawn: MeshInstance3D = body.get_node("MeshInstance3D")
	drawn.material_override = _material(CHEESE)
	var half := _size(body) / 2.0
	var hole := _material(CHEESE.darkened(0.28))
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(body.position)
	for normal: Vector3 in [Vector3.UP, Vector3.LEFT, Vector3.RIGHT, Vector3.FORWARD, Vector3.BACK]:
		var across := Vector3.UP.cross(normal) if normal != Vector3.UP else Vector3.RIGHT
		var up := normal.cross(across)
		for count in 3:
			var radius := rng.randf_range(0.14, 0.3)
			# Kept clear of the block's rounded edges.
			var room := half.x - 0.3 - radius
			var at := normal * half + across * rng.randf_range(-room, room) + up * rng.randf_range(-room, room)
			body.add_child(Rounded.spot(radius, hole, at, normal, 0.008))


# A tower: rows of windows all the way up, and a flat dark roof.
static func _building(body: Node3D) -> void:
	var drawn: MeshInstance3D = body.get_node("MeshInstance3D")
	var colour: Color = (drawn.material_override as StandardMaterial3D).albedo_color
	drawn.material_override = _windows(colour)
	var size := _size(body)
	body.add_child(Rounded.instance(Rounded.box(Vector3(size.x + 0.8, 1.0, size.z + 0.8), 0.25), _material(INK.lightened(0.15)), Vector3(0, size.y / 2.0 + 0.3, 0)))
	# A doorway on each side, so it reads as a building from the street.
	for normal: Vector3 in [Vector3.LEFT, Vector3.RIGHT, Vector3.FORWARD, Vector3.BACK]:
		var out := absf(normal.dot(size / 2.0))
		var door := Rounded.instance(Rounded.box(Vector3(3.0, 4.6, 0.3), 0.1), _material(INK.lightened(0.1)), normal * out + Vector3(0, -size.y / 2.0 + 2.3, 0))
		door.rotation.y = atan2(normal.x, normal.z)
		body.add_child(door)


# A wall of `colour` with a window in every few metres of it, whatever the
# size of the box it is on.
static func _windows(colour: Color) -> StandardMaterial3D:
	var key := ["windows", colour]
	if _materials.has(key):
		return _materials[key]
	if _window_texture == null:
		var image := Image.create(64, 64, true, Image.FORMAT_RGB8)
		image.fill(Color(0.92, 0.92, 0.9))
		image.fill_rect(Rect2i(12, 10, 40, 40), Color(0.3, 0.33, 0.38))
		image.fill_rect(Rect2i(15, 13, 34, 34), Color(0.2, 0.36, 0.55))
		image.fill_rect(Rect2i(15, 13, 34, 9), Color(0.45, 0.62, 0.8))
		image.fill_rect(Rect2i(31, 13, 2, 34), Color(0.3, 0.33, 0.38))
		image.generate_mipmaps()
		_window_texture = ImageTexture.create_from_image(image)
	var material := StandardMaterial3D.new()
	material.albedo_color = colour.lightened(0.25)
	material.albedo_texture = _window_texture
	material.uv1_triplanar = true
	material.uv1_world_triplanar = true
	material.uv1_scale = Vector3.ONE / 5.0
	material.roughness = 0.6
	_materials[key] = material
	return material


# Draws the arena's jump pads and ladders where its scene marks them.
static func _fixtures(map: Node3D) -> void:
	var glow := StandardMaterial3D.new()
	glow.albedo_color = Color(0.2, 0.85, 0.9)
	glow.emission_enabled = true
	glow.emission = Color(0.1, 0.7, 0.8)
	glow.emission_energy_multiplier = 0.8
	var pads := map.get_node_or_null("JumpPads")
	if pads != null:
		for pad: Node3D in pads.get_children():
			pad.add_child(Rounded.instance(Rounded.cylinder(Fixtures.PAD_RADIUS, 0.12, 0.05, 28), _material(INK.lightened(0.1)), Vector3(0, 0.06, 0)))
			pad.add_child(Rounded.instance(Rounded.cylinder(Fixtures.PAD_RADIUS * 0.72, 0.14, 0.05, 28), glow, Vector3(0, 0.07, 0)))
			# Chevrons pointing the way it throws.
			var launch: Vector3 = pad.get_meta("launch", Vector3.UP)
			var way := Vector3(launch.x, 0, launch.z)
			for step in 3:
				var chevron := Rounded.instance(Rounded.box(Vector3(0.9 - step * 0.22, 0.05, 0.16), 0.02), _material(Color(0.95, 0.95, 0.95)), Vector3.ZERO)
				chevron.position = Vector3(0, 0.16, 0) + (way.normalized() * (step * 0.4 - 0.3) if way.length() > 0.1 else Vector3.ZERO)
				if way.length() > 0.1:
					chevron.rotation.y = atan2(way.x, way.z)
				pad.add_child(chevron)
	var ladders := map.get_node_or_null("Ladders")
	if ladders != null:
		var steel := _material(Color(0.62, 0.65, 0.7))
		for ladder: Node3D in ladders.get_children():
			var height := float(ladder.get_meta("height", 4.0))
			# Against what it climbs, which is just ahead of where the climber stands.
			var back := -0.95
			for side in [-1.0, 1.0]:
				ladder.add_child(Rounded.instance(Rounded.box(Vector3(0.12, height + 0.9, 0.12), 0.04), steel, Vector3(0.6 * side, (height + 0.9) / 2.0, back)))
			for rung in int(height / 0.6):
				ladder.add_child(Rounded.instance(Rounded.box(Vector3(1.2, 0.09, 0.09), 0.03), steel, Vector3(0, 0.5 + rung * 0.6, back)))


# A wheel at `at`, its axle across the vehicle.
static func _wheel(body: Node3D, radius: float, at: Vector3) -> void:
	var wheel := Rounded.instance(Rounded.cylinder(radius, 0.5, 0.12, 16), _material(INK), at)
	wheel.rotation.z = PI / 2.0
	body.add_child(wheel)


# A car, pointing along -Z, drawn over its two collision boxes.
static func _car(body: Node3D) -> void:
	var colour: Color = CAR_COLOURS[body.name.trim_prefix("Car").to_int() % CAR_COLOURS.size()]
	var paint := _material(colour)
	var glass := _material(Color(0.16, 0.22, 0.3))
	body.add_child(Rounded.instance(Rounded.box(Vector3(3.2, 1.05, 7.0), 0.4), paint, Vector3(0, 0.875, 0)))
	body.add_child(Rounded.instance(Rounded.box(Vector3(2.8, 1.05, 3.4), 0.4), paint, Vector3(0, 1.875, 0.3)))
	# Windows all round the cabin.
	body.add_child(Rounded.instance(Rounded.box(Vector3(2.86, 0.6, 2.6), 0.1), glass, Vector3(0, 1.95, 0.3)))
	body.add_child(Rounded.instance(Rounded.box(Vector3(2.3, 0.6, 3.46), 0.1), glass, Vector3(0, 1.95, 0.3)))
	for side in [-1.0, 1.0]:
		for end in [-1.0, 1.0]:
			_wheel(body, 0.6, Vector3(1.4 * side, 0.6, 2.2 * end))
		body.add_child(Rounded.instance(Rounded.box(Vector3(0.6, 0.3, 0.12), 0.05), _material(Color(0.97, 0.97, 0.92)), Vector3(1.05 * side, 1.0, -3.48)))
		body.add_child(Rounded.instance(Rounded.box(Vector3(0.6, 0.25, 0.12), 0.05), _material(Color(0.7, 0.08, 0.08)), Vector3(1.05 * side, 1.05, 3.48)))


# A bus, drawn over its collision box.
static func _bus(body: Node3D) -> void:
	var size := _size(body)
	var glass := _material(Color(0.16, 0.22, 0.3))
	body.add_child(Rounded.instance(Rounded.box(Vector3(size.x, size.y - 0.5, size.z), 0.5), _material(Color(0.78, 0.14, 0.12)), Vector3(0, size.y / 2.0 + 0.25, 0)))
	body.add_child(Rounded.instance(Rounded.box(Vector3(size.x + 0.06, 1.2, size.z - 1.6), 0.15), glass, Vector3(0, size.y - 1.3, 0)))
	body.add_child(Rounded.instance(Rounded.box(Vector3(size.x - 0.6, 1.6, size.z + 0.06), 0.15), glass, Vector3(0, size.y - 1.5, 0)))
	for side in [-1.0, 1.0]:
		for end in [-1.0, 1.0]:
			_wheel(body, 0.75, Vector3((size.x / 2.0 - 0.3) * side, 0.75, (size.z / 2.0 - 2.2) * end))


# Many copies of `mesh`, one at each of `places`, drawn at once; each in its
# own one of `tints` if any are given, or else all in `material`.
static func _many(parent: Node3D, mesh: Mesh, places: Array[Transform3D], tints := PackedColorArray(), material: Material = null) -> MultiMeshInstance3D:
	var copies := MultiMesh.new()
	copies.transform_format = MultiMesh.TRANSFORM_3D
	copies.use_colors = not tints.is_empty()
	copies.mesh = mesh
	copies.instance_count = places.size()
	for index in places.size():
		copies.set_instance_transform(index, places[index])
		if copies.use_colors:
			copies.set_instance_color(index, tints[index])
	var node := MultiMeshInstance3D.new()
	node.multimesh = copies
	node.material_override = material if material != null else _tinted()
	# Kept where they can be read back: a machine that draws nothing, as when
	# running the tests, doesn't keep what it is given to draw.
	node.set_meta("places", places)
	node.set_meta("tints", tints)
	parent.add_child(node)
	return node


# A low fence along the inside of the arena's edge: posts, and two rails.
static func _fence(parent: Node3D, edge: Rect2, look: Dictionary) -> void:
	var fence := Node3D.new()
	fence.name = "Fence"
	parent.add_child(fence)
	var posts: Array[Transform3D] = []
	var corners := [edge.position, Vector2(edge.end.x, edge.position.y), edge.end, Vector2(edge.position.x, edge.end.y)]
	for side in 4:
		var from: Vector2 = corners[side]
		var to: Vector2 = corners[(side + 1) % 4]
		var length := from.distance_to(to)
		var count := maxi(roundi(length / POST_SPACING), 1)
		for index in count:
			var at := from.lerp(to, float(index) / count)
			posts.append(Transform3D(Basis.IDENTITY, Vector3(at.x, FENCE_HEIGHT / 2.0, at.y)))
		var middle := (from + to) / 2.0
		for height: float in [FENCE_HEIGHT * 0.45, FENCE_HEIGHT * 0.85]:
			var rail := Rounded.instance(Rounded.box(Vector3(length, 0.16, 0.1), 0.04), _material(look.rail), Vector3(middle.x, height, middle.y))
			rail.rotation.y = -(to - from).angle()
			rail.name = "Rail%d" % (fence.get_child_count() + 1)
			fence.add_child(rail)
	var drawn := _many(fence, Rounded.box(Vector3(0.28, FENCE_HEIGHT, 0.28), 0.06), posts, PackedColorArray(), _material(look.post))
	drawn.name = "Posts"


# The land beyond the arena, as far as can be seen.
static func _ground(parent: Node3D, colour: Color) -> void:
	var plane := PlaneMesh.new()
	plane.size = Vector2.ONE * 3000.0
	var ground := Rounded.instance(plane, _material(colour), Vector3(0, -0.4, 0))
	ground.name = "Ground"
	parent.add_child(ground)


# A place beyond the arena's edge: at least `near` and at most `far` past it.
static func _beyond(edge: Rect2, rng: RandomNumberGenerator, near: float, far: float) -> Vector3:
	var inner := edge.grow(near)
	var outer := edge.grow(far)
	for attempt in 50:
		var at := Vector2(rng.randf_range(outer.position.x, outer.end.x), rng.randf_range(outer.position.y, outer.end.y))
		if not inner.has_point(at):
			return Vector3(at.x, 0, at.y)
	return Vector3(outer.end.x, 0, outer.end.y)


static func _sphere() -> SphereMesh:
	var sphere := SphereMesh.new()
	sphere.radius = 1.0
	sphere.height = 2.0
	sphere.radial_segments = 16
	sphere.rings = 8
	return sphere


static func _shade(colour: Color, rng: RandomNumberGenerator, by := 0.12) -> Color:
	var amount := rng.randf_range(-by, by)
	return colour.lightened(amount) if amount > 0.0 else colour.darkened(-amount)


# Trees, each a trunk under a round crown, at `spots`.
static func _trees(parent: Node3D, spots: Array[Vector3], rng: RandomNumberGenerator) -> void:
	var trunks: Array[Transform3D] = []
	var crowns: Array[Transform3D] = []
	var greens := PackedColorArray()
	for spot in spots:
		var tall := rng.randf_range(3.0, 5.5)
		trunks.append(Transform3D(Basis.from_scale(Vector3(1, tall / 4.0, 1)), spot + Vector3.UP * tall / 2.0))
		var wide := rng.randf_range(2.2, 3.6)
		crowns.append(Transform3D(Basis.from_scale(Vector3(wide, wide * rng.randf_range(0.9, 1.25), wide)), spot + Vector3.UP * (tall + wide * 0.6)))
		greens.append(_shade(Color(0.24, 0.52, 0.26), rng, 0.2))
	_many(parent, Rounded.cylinder(0.45, 4.0, 0.1, 10), trunks, PackedColorArray(), _material(Color(0.42, 0.28, 0.18))).name = "Trunks"
	_many(parent, _sphere(), crowns, greens).name = "Crowns"


# Rolling green hills, trees, and a red barn.
static func _meadow(parent: Node3D, edge: Rect2, rng: RandomNumberGenerator) -> void:
	var hills: Array[Transform3D] = []
	var greens := PackedColorArray()
	for count in 26:
		var wide := rng.randf_range(22.0, 55.0)
		var at := _beyond(edge, rng, 45.0 + wide * 0.5, REACH)
		hills.append(Transform3D(Basis.from_scale(Vector3(wide, wide * rng.randf_range(0.25, 0.45), wide)), at + Vector3.DOWN * 2.0))
		greens.append(_shade(Color(0.27, 0.48, 0.24), rng))
	_many(parent, _sphere(), hills, greens).name = "Hills"
	var spots: Array[Vector3] = []
	for count in 70:
		spots.append(_beyond(edge, rng, 6.0, 90.0))
	_trees(parent, spots, rng)

	var barn := Node3D.new()
	barn.name = "Barn"
	barn.position = Vector3(edge.end.x + 26.0, 0, edge.position.y - 8.0)
	barn.rotation.y = 0.5
	parent.add_child(barn)
	barn.add_child(Rounded.instance(Rounded.box(Vector3(12, 7, 16), 0.3), _material(Color(0.68, 0.16, 0.13)), Vector3(0, 3.5, 0)))
	var roof := PackedVector2Array([Vector2(-7, 0), Vector2(7, 0), Vector2(0, 4.5)])
	barn.add_child(Rounded.instance(Rounded.prism(roof, 17.0, 0.3), _material(Color(0.3, 0.3, 0.34)), Vector3(0, 7, 0)))
	barn.add_child(Rounded.instance(Rounded.box(Vector3(4, 5, 0.3), 0.1), _material(Color(0.93, 0.93, 0.9)), Vector3(0, 2.5, 8.05)))
	barn.add_child(Rounded.instance(Rounded.cylinder(2.6, 13.0, 1.2, 16), _material(Color(0.8, 0.82, 0.85)), Vector3(10, 6.5, -4)))


# Red rock: flat-topped buttes near and far, and cactuses between them.
static func _canyon(parent: Node3D, edge: Rect2, rng: RandomNumberGenerator) -> void:
	var rocks: Array[Transform3D] = []
	var reds := PackedColorArray()
	for count in 34:
		var size := Vector3(rng.randf_range(10.0, 34.0), rng.randf_range(8.0, 34.0), rng.randf_range(10.0, 34.0))
		var at := _beyond(edge, rng, 12.0 + maxf(size.x, size.z) * 0.5, REACH)
		var turn := Basis(Vector3.UP, rng.randf_range(0.0, TAU))
		var colour := _shade(Color(0.5, 0.27, 0.21), rng)
		rocks.append(Transform3D(turn * Basis.from_scale(size), at + Vector3.UP * (size.y / 2.0 - 1.0)))
		reds.append(colour)
		# A narrower layer on top, as the rock has worn back.
		var cap := size * Vector3(0.7, 0.35, 0.7)
		rocks.append(Transform3D(turn * Basis.from_scale(cap), at + Vector3.UP * (size.y - 1.0 + cap.y / 2.0)))
		reds.append(colour.lightened(0.08))
	_many(parent, Rounded.box(Vector3.ONE, 0.06), rocks, reds).name = "Buttes"
	var stems: Array[Transform3D] = []
	for count in 40:
		var at := _beyond(edge, rng, 4.0, 80.0)
		var tall := rng.randf_range(2.5, 5.0)
		stems.append(Transform3D(Basis.from_scale(Vector3(1, tall / 4.0, 1)), at + Vector3.UP * tall / 2.0))
		# An arm, out to one side and up.
		var out := Vector3(rng.randf_range(0.9, 1.2) * (1.0 if rng.randf() < 0.5 else -1.0), 0, 0)
		stems.append(Transform3D(Basis.from_scale(Vector3(0.7, tall / 9.0, 0.7)), at + out + Vector3.UP * tall * 0.62))
		stems.append(Transform3D(Basis(Vector3.BACK, PI / 2.0) * Basis.from_scale(Vector3(0.7, 0.3, 0.7)), at + out / 2.0 + Vector3.UP * tall * 0.45))
	_many(parent, Rounded.cylinder(0.5, 4.0, 0.45, 10), stems, PackedColorArray(), _material(Color(0.25, 0.5, 0.3))).name = "Cactuses"


# More of the city: towers on every block, and the roads running on between
# them.
static func _city(parent: Node3D, edge: Rect2, rng: RandomNumberGenerator) -> void:
	var asphalt := _material(Color(0.2, 0.21, 0.24))
	var road := 14.0
	for along_x in [true, false]:
		var size := Vector3(REACH * 2.0, 0.02, road) if along_x else Vector3(road, 0.02, REACH * 2.0)
		var strip := BoxMesh.new()
		strip.size = size
		var drawn := Rounded.instance(strip, asphalt, Vector3(0, -0.1, 0))
		drawn.name = "RoadAcross" if along_x else "RoadDown"
		parent.add_child(drawn)
	var towers: Array[Transform3D] = []
	var roofs: Array[Transform3D] = []
	var tints := PackedColorArray()
	# A tower in the middle of every block: the blocks beside the roads that
	# run out of the arena, then block after block beyond its edge.
	var block := 30.0
	var across: Array[float] = []
	var down: Array[float] = []
	for side: float in [-1.0, 1.0]:
		across.append(side * (road / 2.0 + 15.0))
		down.append(side * (road / 2.0 + 15.0))
		for count in int(REACH / block):
			across.append(side * (edge.end.x + 13.0 + count * block))
			down.append(side * (edge.end.y + 13.0 + count * block))
	for x in across:
		for z in down:
			var at := Vector3(x, 0, z)
			if edge.has_point(Vector2(x, z)):
				continue
			var size := Vector3(rng.randf_range(14.0, 21.0), rng.randf_range(24.0, 110.0), rng.randf_range(14.0, 21.0))
			towers.append(Transform3D(Basis.from_scale(size), at + Vector3.UP * size.y / 2.0))
			roofs.append(Transform3D(Basis.from_scale(Vector3(size.x + 0.8, 1.0, size.z + 0.8)), at + Vector3.UP * (size.y + 0.3)))
			tints.append(BUILDING_COLOURS[rng.randi() % BUILDING_COLOURS.size()].lightened(0.25))
	var material := _windows(Color.WHITE).duplicate() as StandardMaterial3D
	material.albedo_color = Color.WHITE
	material.vertex_color_use_as_albedo = true
	_many(parent, Rounded.box(Vector3.ONE, 0.02), towers, tints, material).name = "Towers"
	_many(parent, Rounded.box(Vector3.ONE, 0.02), roofs, PackedColorArray(), _material(INK.lightened(0.15))).name = "Roofs"
