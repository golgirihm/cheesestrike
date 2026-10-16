class_name BigMap
extends RefCounted
## Builds a large map in code: one too big to lay out box by box in a
## scene, where getting from one side to the other takes a minute or two.
## Its buildings can be walked into, through doors, with ramps up between
## their floors and a ladder to the roof; and most of what stands in it is
## cheese, for the mice to eat.
##
## What it builds is laid out the way an arena's scene is (see Maps): plain
## boxes under Geometry, of which a Block is cheese; SpawnPoints, Pickups,
## Objectives, Places, Ladders, Doors and a MenuCamera; and roads and the
## like under Decor, only to be seen. Besides, its root has:
## - `large`: true, as only a map built here is.
## - `routes`: the street corners and doorways bots find their way by (see
##   Routes).
## - `footprints`: each building's ground, as a Rect2 across X and Z, for the
##   lighting to darken round as one (see the Lighting).
##
## Every machine builds the same map from the same plan, so whatever is
## chosen at random is chosen from a seed.

## Metres from one floor of a building to the next, and how thick its floors
## and walls are. A player is 5.4 m tall.
const STOREY := 7.0
const SLAB := 0.5
const WALL := 0.6
## A doorway's width and height, and a window's; and how high a window's
## sill is above the floor.
const DOOR := Vector2(4.4, 6.0)
const WINDOW := Vector2(4.0, 3.2)
const SILL := 1.8
## How far a ramp between floors runs across the ground, and how wide it is:
## a slope of about 30 degrees.
const RAMP_RUN := 12.0
const RAMP_WIDTH := 4.4
const RAMP_THICK := 0.4
## How tall the low wall round a roof is: enough to crouch behind.
const PARAPET := 2.0
## How far out from a wall the foot of a ladder stands, for a climber to fit.
const LADDER_OUT := 1.3
## The smallest building: room inside for a ramp, and to walk round it.
const LEAST := Vector2(24.0, 18.0)
## The yellow of cheese, and the colours of what isn't.
const CHEESE := Color(1.0, 0.82, 0.22)
const STONE := Color(0.7, 0.66, 0.6)
const BOARDS := Color(0.52, 0.36, 0.24)
const ROAD := Color(0.2, 0.21, 0.24)
const PAVING := Color(0.4, 0.39, 0.38)
const LINE := Color(0.93, 0.93, 0.9)

var map: Node3D
var geometry: Node3D
var decor: Node3D
## The map's size across X and Z.
var size: Vector2
var _counts := {}
var _points: Array[Vector3] = []
var _links: Array = []
var _footprints: Array[Rect2] = []
var _dashes: Array[Transform3D] = []
# What stands on the ground, across X and Z, for ways to keep clear of.
var _ground: Array[Rect2] = []
# The building whose pieces are being made, and how many have been.
var _group := -1
var _groups := 0
# Copies of what is drawn many times over, by what each is: its mesh and
# material, and where each copy is.
var _copies := {}


## Begins a map called `name`, with the surroundings `scenery` (see Scenery),
## `extent` across X and Z: its floor, and the four walls at its edge.
func _init(name: String, scenery: String, extent: Vector2, floor_colour := Color(0.46, 0.47, 0.5)) -> void:
	size = extent
	map = Node3D.new()
	map.name = name
	map.set_meta("scenery", scenery)
	map.set_meta("large", true)
	geometry = _holder("Geometry")
	for holder: String in ["SpawnPoints", "Objectives", "Pickups", "Places", "JumpPads", "Ladders", "Doors"]:
		_holder(holder)
	decor = _holder("Decor")
	var half := extent / 2.0
	box("Floor", Vector3(0, -0.5, 0), Vector3(extent.x, 1, extent.y), floor_colour)
	# The edge: tall, solid, and not drawn (see Scenery).
	box("WallNorth", Vector3(0, 999, -half.y - 2.0), Vector3(extent.x + 8.0, 2000, 4), Color.TRANSPARENT)
	box("WallSouth", Vector3(0, 999, half.y + 2.0), Vector3(extent.x + 8.0, 2000, 4), Color.TRANSPARENT)
	box("WallEast", Vector3(half.x + 2.0, 999, 0), Vector3(4, 2000, extent.y), Color.TRANSPARENT)
	box("WallWest", Vector3(-half.x - 2.0, 999, 0), Vector3(4, 2000, extent.y), Color.TRANSPARENT)


## The name for the next of what begins `prefix`: Block1, Block2 and so on.
func _next(prefix: String) -> String:
	_counts[prefix] = _counts.get(prefix, 0) + 1
	return "%s%d" % [prefix, _counts[prefix]]


func _holder(name: String) -> Node3D:
	var holder := Node3D.new()
	holder.name = name
	map.add_child(holder)
	return holder


## A solid box of `extent` centred at `at`, named `name` exactly if it is
## one of a kind (the floor, the walls) or else numbered after it; drawn in
## `colour` unless that is clear, and made of `surface` if one is given.
func box(name: String, at: Vector3, extent: Vector3, colour: Color, surface := "") -> StaticBody3D:
	var body := StaticBody3D.new()
	body.name = name if name == "Floor" or name.begins_with("Wall") else _next(name)
	body.position = at
	if surface != "":
		body.set_meta("surface", surface)
	if colour.a > 0.0:
		var drawn := MeshInstance3D.new()
		drawn.name = "MeshInstance3D"
		var mesh := BoxMesh.new()
		mesh.size = extent
		var material := StandardMaterial3D.new()
		material.albedo_color = colour
		mesh.material = material
		drawn.mesh = mesh
		body.add_child(drawn)
	var shape := CollisionShape3D.new()
	shape.name = "CollisionShape3D"
	var solid := BoxShape3D.new()
	solid.size = extent
	shape.shape = solid
	body.add_child(shape)
	geometry.add_child(body)
	# Drawn as one with the rest of the building it is part of (see Clusters).
	if _group >= 0:
		body.set_meta("group", _group)
	if body.name != "Floor" and not body.name.begins_with("Wall") and at.y - extent.y / 2.0 < 1.0:
		_ground.append(Rect2(at.x - extent.x / 2.0, at.z - extent.z / 2.0, extent.x, extent.z))
	return body


## A block of cheese, for the mice.
func cheese(at: Vector3, extent: Vector3) -> StaticBody3D:
	return box("Block", at, extent, CHEESE)


## A piece of a building: cheese, or else stone.
func _piece(at: Vector3, extent: Vector3, is_cheese: bool) -> StaticBody3D:
	return cheese(at, extent) if is_cheese else box("Masonry", at, extent, STONE)


## A tower too tall to climb, with windows all the way up (see Scenery):
## something to find one's way by.
func tower(at: Vector2, footprint: Vector2, height: float, colour: Color) -> void:
	box("Building", Vector3(at.x, height / 2.0, at.y), Vector3(footprint.x, height, footprint.y), colour)
	_footprints.append(Rect2(at - footprint / 2.0, footprint))


## A solid block of cheese standing on the ground, `extent` big, with a
## ladder up one side to its top if `ladder_side` is one of the four ways
## (Vector3.LEFT and so on) its sides face.
func warehouse(at: Vector2, extent: Vector3, ladder_side := Vector3.ZERO) -> void:
	cheese(Vector3(at.x, extent.y / 2.0, at.y), extent)
	_footprints.append(Rect2(at - Vector2(extent.x, extent.z) / 2.0, Vector2(extent.x, extent.z)))
	if ladder_side != Vector3.ZERO:
		var out := absf(ladder_side.dot(extent / 2.0)) + LADDER_OUT
		ladder(Vector3(at.x, 0, at.y) + ladder_side * out, -ladder_side, extent.y)


## A ladder whose foot is at `foot`, climbed facing `toward`, `height` tall.
func ladder(foot: Vector3, toward: Vector3, height: float) -> void:
	var marker := Marker3D.new()
	marker.name = _next("Ladder")
	marker.position = foot
	marker.basis = Basis.looking_at(toward.normalized())
	marker.set_meta("height", height)
	map.get_node("Ladders").add_child(marker)


## A jump pad at `at` that throws whoever steps on it at `launch`.
func jump_pad(at: Vector3, launch: Vector3) -> void:
	var marker := Marker3D.new()
	marker.name = _next("Pad")
	marker.position = at
	marker.set_meta("launch", launch)
	map.get_node("JumpPads").add_child(marker)


## A door of `extent` filling a doorway centred at `at`, which players open
## and close (see MovingParts).
func door(at: Vector3, extent: Vector3) -> void:
	var marker := Marker3D.new()
	marker.name = _next("Door")
	marker.position = at
	marker.set_meta("size", extent)
	map.get_node("Doors").add_child(marker)


## A building that can be walked into: `footprint` across (along its front,
## and from front to back), `floors` high, its front facing `turn` quarter
## turns round from the south (+Z). Each floor has a ramp up to the next, the
## ground floor a door at the front and another at the back, the floors above
## windows on every side, and the roof a low wall round it and a ladder up
## its left side from the street.
##
## Options: "cheese" (true, unless it is stone), "label" for the place it
## is, "inside": kinds of pickup to lie on the ground floor.
##
## Returns where things are: {"front": just outside the front door, "back":
## just outside the back one, "inside": just inside the front one, "roof":
## the middle of the roof, "floors": the middle of each floor}.
func house(at: Vector2, footprint: Vector2, floors: int, turn: int, options := {}) -> Dictionary:
	footprint = footprint.max(LEAST)
	floors = clampi(floors, 1, 3)
	var is_cheese: bool = options.get("cheese", true)
	var w := footprint.x
	var d := footprint.y
	var frame := {"at": at, "turn": posmod(turn, 4)}
	_group = _groups
	_groups += 1
	var inner := Rect2(-w / 2.0 + WALL, -d / 2.0 + WALL, w - WALL * 2.0, d - WALL * 2.0)
	for storey in floors:
		var base := storey * STOREY
		var top := base + STOREY
		var ground := storey == 0
		# The front and back run the whole width; the sides between them.
		var front := [_opening(0.0, DOOR, 0.0)] if ground else [_opening(0.0, WINDOW, SILL)]
		var back := [_opening(w / 2.0 - WALL - 4.0, DOOR, 0.0)] if ground else [_opening(0.0, WINDOW, SILL)]
		var side := [] if ground else [_opening(0.0, WINDOW, SILL)]
		_run(frame, true, d / 2.0 - WALL / 2.0, -w / 2.0, w / 2.0, base, top, front, is_cheese)
		_run(frame, true, -d / 2.0 + WALL / 2.0, -w / 2.0, w / 2.0, base, top, back, is_cheese)
		_run(frame, false, -w / 2.0 + WALL / 2.0, inner.position.y, inner.end.y, base, top, side, is_cheese)
		_run(frame, false, w / 2.0 - WALL / 2.0, inner.position.y, inner.end.y, base, top, side, is_cheese)
		if storey > 0:
			_floor(frame, inner, top - STOREY, storey - 1, is_cheese)
		if storey < floors - 1:
			_ramp(frame, inner, base, storey)
	# The roof, and the low wall round it, open where the ladder comes up.
	var roof := floors * STOREY
	_slab(frame, inner, roof, is_cheese)
	var ladder_along := d / 2.0 - 4.0
	_run(frame, true, d / 2.0 - WALL / 2.0, -w / 2.0, w / 2.0, roof, roof + PARAPET, [], is_cheese)
	_run(frame, true, -d / 2.0 + WALL / 2.0, -w / 2.0, w / 2.0, roof, roof + PARAPET, [], is_cheese)
	_run(frame, false, -w / 2.0 + WALL / 2.0, inner.position.y, inner.end.y, roof, roof + PARAPET, [_opening(ladder_along, Vector2(3.0, PARAPET), 0.0)], is_cheese)
	_run(frame, false, w / 2.0 - WALL / 2.0, inner.position.y, inner.end.y, roof, roof + PARAPET, [], is_cheese)
	_group = -1
	ladder(_world(frame, Vector3(-w / 2.0 - LADDER_OUT, 0, ladder_along)), _turned(frame, Vector3.RIGHT), roof)
	# The doors, and boards to walk on inside.
	var turned_door := _extent(frame, Vector3(DOOR.x, DOOR.y, WALL))
	door(_world(frame, Vector3(0, DOOR.y / 2.0, d / 2.0 - WALL / 2.0)), turned_door)
	door(_world(frame, Vector3(w / 2.0 - WALL - 4.0, DOOR.y / 2.0, -d / 2.0 + WALL / 2.0)), turned_door)
	_copy("Boards", Textures.material(BOARDS, Textures.WOOD), _world(frame, Vector3(0, 0.02, 0)), _extent(frame, Vector3(inner.size.x, 0.04, inner.size.y)))
	var world_size := Vector2(w, d) if frame.turn % 2 == 0 else Vector2(d, w)
	_footprints.append(Rect2(at - world_size / 2.0, world_size))
	# Where things are, and the ways in for bots.
	var found := {
		"front": _world(frame, Vector3(0, 0, d / 2.0 + 5.0)),
		"back": _world(frame, Vector3(w / 2.0 - WALL - 4.0, 0, -d / 2.0 - 5.0)),
		"inside": _world(frame, Vector3(0, 0, d / 2.0 - 4.0)),
		"roof": _world(frame, Vector3(0, roof, 0)),
		"floors": [],
	}
	for storey in floors:
		found.floors.append(_world(frame, Vector3(0, storey * STOREY, 0)))
	var inside := point(found.inside)
	var inside_back := point(_world(frame, Vector3(w / 2.0 - WALL - 4.0, 0, -d / 2.0 + 4.0)))
	found["front_point"] = point(found.front)
	found["back_point"] = point(found.back)
	link(inside, found.front_point)
	link(inside, inside_back)
	link(inside_back, found.back_point)
	var lying: Array = options.get("inside", [])
	for index in lying.size():
		# Along the front wall inside, clear of the doorway and the ramp.
		var along := (index + 1) * (inner.size.x / (lying.size() + 1)) - inner.size.x / 2.0
		if absf(along) < DOOR.x:
			along += DOOR.x * signf(along if along != 0.0 else 1.0)
		pickup(lying[index], _world(frame, Vector3(along, 0, d / 2.0 - WALL - 2.5)))
	if options.has("label"):
		place(options.label, found.floors[0])
		if floors > 1:
			place("%s, upstairs" % options.label, found.floors[1])
		place("%s roof" % options.label, found.roof)
	return found


func _opening(along: float, extent: Vector2, bottom: float) -> Dictionary:
	return {"along": along, "width": extent.x, "bottom": bottom, "top": bottom + extent.y}


# One wall of a building from `from` to `to` along X (or Z, if not
# `along_x`) of its own frame, at `fixed` across, from `base` up to `top`,
# with `openings` in it: piers between them, and over and under each what
# the opening leaves.
func _run(frame: Dictionary, along_x: bool, fixed: float, from: float, to: float, base: float, top: float, openings: Array, is_cheese: bool) -> void:
	var height := top - base
	var cursor := from
	var sorted := openings.duplicate()
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a.along < b.along)
	for opening: Dictionary in sorted:
		var left: float = opening.along - opening.width / 2.0
		var right: float = opening.along + opening.width / 2.0
		_wall_piece(frame, along_x, fixed, cursor, left, base, top, is_cheese)
		if opening.bottom > 0.05:
			_wall_piece(frame, along_x, fixed, left, right, base, base + opening.bottom, is_cheese)
		if opening.top < height - 0.05:
			_wall_piece(frame, along_x, fixed, left, right, base + opening.top, top, is_cheese)
		cursor = right
	_wall_piece(frame, along_x, fixed, cursor, to, base, top, is_cheese)


func _wall_piece(frame: Dictionary, along_x: bool, fixed: float, from: float, to: float, bottom: float, top: float, is_cheese: bool) -> void:
	if to - from < 0.05 or top - bottom < 0.05:
		return
	var middle := (from + to) / 2.0
	var local := Vector3(middle, (bottom + top) / 2.0, fixed) if along_x else Vector3(fixed, (bottom + top) / 2.0, middle)
	var extent := Vector3(to - from, top - bottom, WALL) if along_x else Vector3(WALL, top - bottom, to - from)
	_piece(_world(frame, local), _extent(frame, extent), is_cheese)


# The floor of storey `above` + 1, its top at `level`, with a hole where the
# ramp up to it from storey `above` comes through.
func _floor(frame: Dictionary, inner: Rect2, level: float, above: int, is_cheese: bool) -> void:
	var well := _well(inner, above)
	var y := level - SLAB / 2.0
	# All of it beside the band the ramp is in, and the rest of that band.
	var beside := Rect2(inner.position.x, inner.position.y + RAMP_WIDTH, inner.size.x, inner.size.y - RAMP_WIDTH) if above % 2 == 0 else Rect2(inner.position.x, inner.position.y, inner.size.x, inner.size.y - RAMP_WIDTH)
	var rest := Rect2(well.end.x, well.position.y, inner.end.x - well.end.x, well.size.y) if above % 2 == 0 else Rect2(inner.position.x, well.position.y, well.position.x - inner.position.x, well.size.y)
	for part: Rect2 in [beside, rest]:
		if part.size.x > 0.05 and part.size.y > 0.05:
			_piece(_world(frame, Vector3(part.get_center().x, y, part.get_center().y)), _extent(frame, Vector3(part.size.x, SLAB, part.size.y)), is_cheese)


func _slab(frame: Dictionary, inner: Rect2, level: float, is_cheese: bool) -> void:
	_piece(_world(frame, Vector3(inner.get_center().x, level - SLAB / 2.0, inner.get_center().y)), _extent(frame, Vector3(inner.size.x, SLAB, inner.size.y)), is_cheese)


# Where, across a building's frame, the ramp up from storey `storey` and the
# hole above it are: along the back wall from the left for the ground floor
# and every other one, along the front from the right for the rest.
static func _well(inner: Rect2, storey: int) -> Rect2:
	if storey % 2 == 0:
		return Rect2(inner.position.x, inner.position.y, 1.0 + RAMP_RUN, RAMP_WIDTH)
	return Rect2(inner.end.x - 1.0 - RAMP_RUN, inner.end.y - RAMP_WIDTH, 1.0 + RAMP_RUN, RAMP_WIDTH)


# A ramp from storey `storey`'s floor, at `base`, up to the one above.
func _ramp(frame: Dictionary, inner: Rect2, base: float, storey: int) -> void:
	var well := _well(inner, storey)
	var rising := 1.0 if storey % 2 == 0 else -1.0
	var bottom := well.position.x + 1.0 if rising > 0.0 else well.end.x - 1.0
	var slope := atan2(STOREY, RAMP_RUN)
	var length := Vector2(RAMP_RUN, STOREY).length()
	# Its top face runs from the floor here to the floor above.
	var normal := Vector3(-sin(slope) * rising, cos(slope), 0)
	var local := Vector3(bottom + rising * RAMP_RUN / 2.0, base + STOREY / 2.0, well.get_center().y) - normal * RAMP_THICK / 2.0
	var body := StaticBody3D.new()
	body.name = _next("Ramp")
	body.transform = Transform3D(Basis(Vector3.UP, frame.turn * PI / 2.0) * Basis(Vector3.BACK, slope * rising), _world(frame, local))
	var mesh := BoxMesh.new()
	mesh.size = Vector3(length, RAMP_THICK, RAMP_WIDTH)
	var material := StandardMaterial3D.new()
	material.albedo_color = BOARDS
	mesh.material = material
	var drawn := MeshInstance3D.new()
	drawn.name = "MeshInstance3D"
	drawn.mesh = mesh
	body.add_child(drawn)
	var shape := CollisionShape3D.new()
	shape.name = "CollisionShape3D"
	var solid := BoxShape3D.new()
	solid.size = mesh.size
	shape.shape = solid
	body.add_child(shape)
	geometry.add_child(body)


# Where a point of a building's own frame is in the map: turned the
# building's quarter turns round, and moved to where it stands.
static func _world(frame: Dictionary, local: Vector3) -> Vector3:
	var turned := _turned(frame, local)
	return Vector3(frame.at.x + turned.x, local.y, frame.at.y + turned.z)


static func _turned(frame: Dictionary, local: Vector3) -> Vector3:
	match int(frame.turn):
		1:
			return Vector3(local.z, local.y, -local.x)
		2:
			return Vector3(-local.x, local.y, -local.z)
		3:
			return Vector3(-local.z, local.y, local.x)
	return local


# The size of a box of a building's frame as it stands in the map.
static func _extent(frame: Dictionary, local: Vector3) -> Vector3:
	return Vector3(local.z, local.y, local.x) if int(frame.turn) % 2 == 1 else local


## A car at `at`, pointing `heading` radians round from north, drawn by
## Scenery over its two boxes.
func car(at: Vector2, heading: float) -> void:
	var body := StaticBody3D.new()
	body.name = _next("Car")
	body.transform = Transform3D(Basis(Vector3.UP, heading), Vector3(at.x, 0, at.y))
	_stands(at, Vector2(3.2, 7.0), heading)
	for part: Array in [[Vector3(3.2, 1.4, 7), Vector3(0, 0.7, 0)], [Vector3(2.8, 1, 3.4), Vector3(0, 1.9, 0.3)]]:
		var shape := CollisionShape3D.new()
		var solid := BoxShape3D.new()
		solid.size = part[0]
		shape.shape = solid
		shape.position = part[1]
		body.add_child(shape)
	geometry.add_child(body)


## A bus at `at`, pointing `heading` radians round from north.
func bus(at: Vector2, heading: float) -> void:
	var extent := Vector3(3.6, 4.2, 12)
	var body := StaticBody3D.new()
	body.name = _next("Bus")
	body.transform = Transform3D(Basis(Vector3.UP, heading), Vector3(at.x, 0, at.y))
	_stands(at, Vector2(extent.x, extent.z), heading)
	var shape := CollisionShape3D.new()
	shape.name = "CollisionShape3D"
	var solid := BoxShape3D.new()
	solid.size = extent
	shape.shape = solid
	shape.position = Vector3(0, extent.y / 2.0, 0)
	body.add_child(shape)
	geometry.add_child(body)


## A road from `from` to `to` (each across X and Z, along one or the other),
## `width` wide, with a dashed line down its middle: only to be seen.
func road(name: String, from: Vector2, to: Vector2, width: float) -> void:
	var along_x := absf(to.x - from.x) > absf(to.y - from.y)
	var length := from.distance_to(to)
	var strip := BoxMesh.new()
	strip.size = Vector3(length, 0.04, width) if along_x else Vector3(width, 0.04, length)
	var middle := (from + to) / 2.0
	var drawn := Rounded.instance(strip, Textures.material(ROAD, Textures.ASPHALT), Vector3(middle.x, 0.0, middle.y))
	drawn.name = _next(name)
	decor.add_child(drawn)
	var step := 7.0
	for index in int(length / step):
		var at := from.lerp(to, (index + 0.5) * step / length)
		var turn := Basis.IDENTITY if not along_x else Basis(Vector3.UP, PI / 2.0)
		_dashes.append(Transform3D(turn, Vector3(at.x, 0.04, at.y)))


## Paving under a block of the town, `rect` across X and Z: only to be seen.
func paving(rect: Rect2) -> void:
	_copy("Paving", Textures.material(PAVING, Textures.TILE), Vector3(rect.get_center().x, 0.0, rect.get_center().y), Vector3(rect.size.x, 0.04, rect.size.y))


# A flat box `extent` big at `at`, one of many called `name` drawn at once
# in `material`, as they would cost a great deal drawn one by one.
func _copy(name: String, material: Material, at: Vector3, extent: Vector3) -> void:
	if not _copies.has(name):
		_copies[name] = {"material": material, "places": []}
	_copies[name].places.append(Transform3D(Basis.from_scale(extent), at))


func _marker(holder: String, name: String, at: Vector3) -> Marker3D:
	var marker := Marker3D.new()
	marker.name = _next(name)
	marker.position = at
	map.get_node(holder).add_child(marker)
	return marker


## A place a player starts, `at` on the ground.
func spawn(at: Vector2) -> void:
	_marker("SpawnPoints", "Spawn", Vector3(at.x, 0.1, at.y))


## A pad holding `kind`, at `at`.
func pickup(kind: String, at: Vector3) -> void:
	_marker("Pickups", kind.capitalize(), at).set_meta("kind", kind)


## One of the places the modes need: BaseCheddar, Hill, PointA and so on.
func objective(name: String, at: Vector3) -> void:
	var marker := Marker3D.new()
	marker.name = name
	marker.position = at
	map.get_node("Objectives").add_child(marker)


## What the part of the map nearest `at` is called.
func place(label: String, at: Vector3) -> void:
	_marker("Places", "Place", at).set_meta("label", label)


## A point bots find their way by (see Routes). Returns its index.
func point(at: Vector3) -> int:
	_points.append(Vector3(at.x, at.y, at.z))
	return _points.size() - 1


## Bots can walk straight between points `a` and `b`.
func link(a: int, b: int) -> void:
	_links.append([a, b])


## Links point `from` to the nearest `count` of the points bots go between
## on the streets, the first `streets` of them, that can be walked to
## straight from it.
func link_nearest(from: int, streets: int, count := 2) -> void:
	var order := range(streets)
	order.sort_custom(func(a: int, b: int) -> bool: return _points[a].distance_to(_points[from]) < _points[b].distance_to(_points[from]))
	var linked := 0
	for index: int in order:
		if linked >= count:
			return
		if is_clear(_points[from], _points[index]):
			link(from, index)
			linked += 1


## Whether a player can walk straight from `a` to `b` past everything
## standing on the ground: doors aside, which open.
func is_clear(a: Vector3, b: Vector3) -> bool:
	var from := Vector2(a.x, a.z)
	var to := Vector2(b.x, b.z)
	for ground: Rect2 in _ground:
		if crosses(from, to, ground.grow(BodyShape.RADIUS)):
			return false
	return true


## Whether the line from `a` to `b` goes through `rect`.
static func crosses(a: Vector2, b: Vector2, rect: Rect2) -> bool:
	var way := b - a
	var enter := 0.0
	var leave := 1.0
	# Cut down to the part of it between each pair of the rect's sides.
	for side: Array in [[-way.x, a.x - rect.position.x], [way.x, rect.end.x - a.x], [-way.y, a.y - rect.position.y], [way.y, rect.end.y - a.y]]:
		var toward: float = side[0]
		var room: float = side[1]
		if is_zero_approx(toward):
			if room < 0.0:
				return false
			continue
		var at := room / toward
		if toward < 0.0:
			enter = maxf(enter, at)
		else:
			leave = minf(leave, at)
		if enter > leave:
			return false
	return true


# Notes something `extent` across (along X, and along Z before it is
# turned) at `at`, turned `heading` round, as standing on the ground.
func _stands(at: Vector2, extent: Vector2, heading: float) -> void:
	var across := absf(cos(heading)) * extent.x + absf(sin(heading)) * extent.y
	var along := absf(sin(heading)) * extent.x + absf(cos(heading)) * extent.y
	_ground.append(Rect2(at.x - across / 2.0, at.y - along / 2.0, across, along))


## The finished map, seen from `camera` behind the menu.
func finish(camera: Transform3D) -> Node3D:
	_many("RoadLines", Rounded.box(Vector3(0.35, 0.06, 3.0), 0.02), Textures.material(LINE, Textures.PLASTER), _dashes)
	var unit := BoxMesh.new()
	for name: String in _copies:
		_many(name, unit, _copies[name].material, _copies[name].places)
	var view := Camera3D.new()
	view.name = "MenuCamera"
	view.transform = camera
	map.add_child(view)
	map.set_meta("routes", {"points": _points, "links": _links})
	map.set_meta("footprints", _footprints)
	return map


# Copies of `mesh` in `material` at each of `places`, drawn at once.
func _many(name: String, mesh: Mesh, material: Material, places: Array) -> void:
	var copies := MultiMesh.new()
	copies.transform_format = MultiMesh.TRANSFORM_3D
	copies.mesh = mesh
	copies.instance_count = places.size()
	for index in places.size():
		copies.set_instance_transform(index, places[index])
	var drawn := MultiMeshInstance3D.new()
	drawn.name = name
	drawn.multimesh = copies
	drawn.material_override = material
	decor.add_child(drawn)
