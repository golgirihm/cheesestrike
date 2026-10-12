class_name Rounded
## Shapes with their edges and corners rounded off, which the engine's own
## boxes and prisms don't have. Nearly everything in the game is built from
## these, so that nothing has a hard edge.
##
## Each shape is made once for each size asked for and shared after that, so
## a mesh from here carries no material: set one on the MeshInstance3D.

## Steps round each quarter turn of an edge.
const STEPS := 3

static var _made := {}


## A box of `size` whose edges are rounded to `radius`, or as near to it as
## the box's smallest side allows.
static func box(size: Vector3, radius := 0.12) -> ArrayMesh:
	radius = minf(radius, minf(size.x, minf(size.y, size.z)) * 0.5)
	var key := ["box", size, radius]
	if not _made.has(key):
		_made[key] = _build_box(size, radius)
	return _made[key]


## An upright wedge like the engine's PrismMesh, a triangle across with its
## point up in the middle, `size.z` deep, with its edges rounded to `radius`.
static func wedge(size: Vector3, radius := 0.05) -> ArrayMesh:
	var half := size / 2.0
	return prism(PackedVector2Array([Vector2(-half.x, -half.y), Vector2(half.x, -half.y), Vector2(0, half.y)]), size.z, radius)


## A slab `depth` deep (along Z) whose outline is `outline`, the corners of a
## convex shape in the XY plane given anticlockwise, with its edges rounded
## to `radius`.
static func prism(outline: PackedVector2Array, depth: float, radius := 0.05) -> ArrayMesh:
	radius = minf(radius, depth * 0.5)
	var key := ["prism", outline, depth, radius]
	if not _made.has(key):
		_made[key] = _build_prism(outline, depth, radius)
	return _made[key]


## An upright cylinder whose rims are rounded to `edge`.
static func cylinder(radius: float, height: float, edge := 0.05, sides := 20) -> ArrayMesh:
	edge = minf(edge, minf(radius, height * 0.5))
	var key := ["cylinder", radius, height, edge, sides]
	if not _made.has(key):
		_made[key] = _build_cylinder(radius, height, edge, sides)
	return _made[key]


## A MeshInstance3D showing `mesh` in `material`.
static func instance(mesh: Mesh, material: Material, at := Vector3.ZERO) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.mesh = mesh
	node.material_override = material
	node.position = at
	return node


# Where the lines of a face's grid fall along one side of a box: close
# together across the rounded edge at each end, and one long step between.
static func _stops(half: float, radius: float) -> PackedFloat32Array:
	var stops := PackedFloat32Array()
	var flat := half - radius
	for step in range(STEPS, -1, -1):
		stops.append(-flat - radius * tan(PI / 4.0 * step / STEPS))
	for step in STEPS + 1:
		stops.append(flat + radius * tan(PI / 4.0 * step / STEPS))
	return stops


static func _build_box(size: Vector3, radius: float) -> ArrayMesh:
	var half := size / 2.0
	var core := half - Vector3.ONE * radius
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	for axis in 3:
		var across := (axis + 1) % 3
		var up := (axis + 2) % 3
		var columns := _stops(half[across], radius)
		var rows := _stops(half[up], radius)
		for side: float in [-1.0, 1.0]:
			var first := vertices.size()
			for row in rows:
				for column in columns:
					var on_box := Vector3.ZERO
					on_box[axis] = side * half[axis]
					on_box[across] = column
					on_box[up] = row
					# Every point of the rounded box is `radius` from the box
					# that many smaller, straight out from the nearest of it.
					var inside := on_box.clamp(-core, core)
					var out := (on_box - inside).normalized()
					vertices.append(inside + out * radius)
					normals.append(out)
			_grid(indices, first, columns.size(), rows.size(), false)
	return _mesh(vertices, normals, indices)


static func _build_prism(outline: PackedVector2Array, depth: float, radius: float) -> ArrayMesh:
	var count := outline.size()
	# Each side's outward direction, and each corner moved in by `radius`.
	var out: Array[Vector2] = []
	for i in count:
		var along := (outline[(i + 1) % count] - outline[i]).normalized()
		out.append(Vector2(along.y, -along.x))
	var core := PackedVector2Array()
	for i in count:
		var before := out[(i - 1 + count) % count]
		var after := out[i]
		core.append(outline[i] - (before + after) * radius / (1.0 + before.dot(after)))
	# Round the outline: at each corner, an arc from the side before to the
	# side after. Each entry is [the corner it turns about, which way is out].
	var ring := []
	for i in count:
		var before := out[(i - 1 + count) % count]
		var turn := before.angle_to(out[i])
		for step in STEPS + 1:
			ring.append([core[i], before.rotated(turn * step / STEPS)])

	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	# From the back face round the edge to the side, along it, and round the
	# edge to the front face: [how far along Z the edge turns about, how far
	# round it].
	var rows := []
	for step in STEPS + 1:
		rows.append([-(depth / 2.0 - radius), -PI / 2.0 + PI / 2.0 * step / STEPS])
	for step in STEPS + 1:
		rows.append([depth / 2.0 - radius, PI / 2.0 * step / STEPS])
	for row: Array in rows:
		for point: Array in ring:
			var centre: Vector2 = point[0]
			var sideways: Vector2 = point[1]
			var normal := Vector3(sideways.x * cos(row[1]), sideways.y * cos(row[1]), sin(row[1]))
			vertices.append(Vector3(centre.x, centre.y, row[0]) + normal * radius)
			normals.append(normal)
	_grid(indices, 0, ring.size(), rows.size(), true)
	# The flat faces, front and back.
	for side: float in [-1.0, 1.0]:
		var first := vertices.size()
		for corner in core:
			vertices.append(Vector3(corner.x, corner.y, side * depth / 2.0))
			normals.append(Vector3(0, 0, side))
		for i in range(1, count - 1):
			indices.append_array([first, first + i, first + i + 1])
	return _mesh(vertices, normals, indices)


static func _build_cylinder(radius: float, height: float, edge: float, sides: int) -> ArrayMesh:
	# The outline from the middle of the bottom, out and up the side, to the
	# middle of the top, as [distance from the axis, height, normal].
	var profile := [[0.0, -height / 2.0, Vector2.DOWN]]
	for step in STEPS + 1:
		var angle := -PI / 2.0 + PI / 2.0 * step / STEPS
		profile.append([radius - edge + cos(angle) * edge, -height / 2.0 + edge + sin(angle) * edge, Vector2(cos(angle), sin(angle))])
	for step in STEPS + 1:
		var angle := PI / 2.0 * step / STEPS
		profile.append([radius - edge + cos(angle) * edge, height / 2.0 - edge + sin(angle) * edge, Vector2(cos(angle), sin(angle))])
	profile.append([0.0, height / 2.0, Vector2.UP])
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	for point: Array in profile:
		for side in sides:
			var around := TAU * side / sides
			var normal: Vector2 = point[2]
			vertices.append(Vector3(cos(around) * point[0], point[1], sin(around) * point[0]))
			normals.append(Vector3(cos(around) * normal.x, normal.y, sin(around) * normal.x))
	_grid(indices, 0, sides, profile.size(), true)
	return _mesh(vertices, normals, indices)


# Two triangles for each square of a grid of points `columns` wide and `rows`
# high starting at `first`; joined round from the last column to the first
# if it `wraps`.
static func _grid(indices: PackedInt32Array, first: int, columns: int, rows: int, wraps: bool) -> void:
	for row in rows - 1:
		for column in columns if wraps else columns - 1:
			var a := first + row * columns + column
			var b := first + row * columns + (column + 1) % columns
			var c := a + columns
			var d := b + columns
			indices.append_array([a, b, d, a, d, c])


# The mesh of these triangles, each turned to face the way its corners'
# normals point, and without those that have shrunk to nothing.
static func _mesh(vertices: PackedVector3Array, normals: PackedVector3Array, indices: PackedInt32Array) -> ArrayMesh:
	var kept := PackedInt32Array()
	for i in range(0, indices.size(), 3):
		var a := indices[i]
		var b := indices[i + 1]
		var c := indices[i + 2]
		var facing := (vertices[b] - vertices[a]).cross(vertices[c] - vertices[a])
		if facing.length() < 1e-9:
			continue
		# The engine draws the side of a triangle its corners run clockwise on.
		if facing.dot(normals[a] + normals[b] + normals[c]) > 0.0:
			kept.append_array([a, c, b])
		else:
			kept.append_array([a, b, c])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = kept
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh
