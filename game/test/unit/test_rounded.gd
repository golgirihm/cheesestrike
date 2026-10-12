extends GutTest
## The rounded shapes: that each fills the space it was asked to, has no hard
## edges, and is closed and the right way out.


func _shapes() -> Dictionary:
	return {
		"box": [Rounded.box(Vector3(2, 1, 3), 0.2), Vector3(2, 1, 3)],
		"thin box": [Rounded.box(Vector3(4, 0.1, 4), 0.5), Vector3(4, 0.1, 4)],
		"wedge": [Rounded.wedge(Vector3(0.78, 1.0, 0.56), 0.04), Vector3(0.78, 1.0, 0.56)],
		"cylinder": [Rounded.cylinder(0.5, 0.4, 0.1), Vector3(1.0, 0.4, 1.0)],
	}


func test_each_shape_fills_the_size_asked_for() -> void:
	var shapes := _shapes()
	for name: String in shapes:
		var bounds: AABB = shapes[name][0].get_aabb()
		var size: Vector3 = shapes[name][1]
		# A rounded point stops a little short of where a sharp one would reach.
		var slack := 0.09 if name == "wedge" else 0.001
		assert_almost_eq(bounds.size.x, size.x, slack, name)
		assert_almost_eq(bounds.size.y, size.y, slack, name)
		assert_almost_eq(bounds.size.z, size.z, 0.001, name)
		assert_almost_eq(bounds.get_center().x, 0.0, 0.001, name)
		assert_almost_eq(bounds.get_center().z, 0.0, 0.001, name)


func test_every_triangle_faces_outward() -> void:
	var shapes := _shapes()
	for name: String in shapes:
		var arrays: Array = shapes[name][0].surface_get_arrays(0)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		assert_gt(indices.size(), 0, name)
		var wrong := 0
		for i in range(0, indices.size(), 3):
			# A plane through three points faces the side they run clockwise on,
			# which is the side the engine draws.
			var drawn := Plane(vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]]).normal
			var out := normals[indices[i]] + normals[indices[i + 1]] + normals[indices[i + 2]]
			if drawn.dot(out) <= 0.0:
				wrong += 1
		assert_eq(wrong, 0, name)


func test_each_shape_is_closed() -> void:
	# Every edge of a closed surface is shared by exactly two triangles.
	var shapes := _shapes()
	for name: String in shapes:
		var faces: PackedVector3Array = shapes[name][0].get_faces()
		var edges := {}
		for i in range(0, faces.size(), 3):
			for side in 3:
				var a := faces[i + side].snappedf(0.0001)
				var b := faces[i + (side + 1) % 3].snappedf(0.0001)
				var key := [a, b] if a < b else [b, a]
				edges[key] = edges.get(key, 0) + 1
		var open := edges.values().filter(func(count: int) -> bool: return count != 2).size()
		assert_eq(open, 0, "%s has %d open edges" % [name, open])


func test_no_edge_is_sharp() -> void:
	# Across every edge, the surface turns by less than a sharp corner would.
	var shapes := _shapes()
	for name: String in shapes:
		var arrays: Array = shapes[name][0].surface_get_arrays(0)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var seen := {}
		var sharpest := 0.0
		for i in vertices.size():
			var key := vertices[i].snappedf(0.0001)
			if seen.has(key):
				sharpest = maxf(sharpest, rad_to_deg(normals[i].angle_to(seen[key])))
			else:
				seen[key] = normals[i]
		assert_lt(sharpest, 1.0, "%s: points shared between faces agree on which way is out" % name)


func test_the_rounding_is_no_more_than_the_shape_allows() -> void:
	var slab := Rounded.box(Vector3(4, 0.1, 4), 0.5)
	assert_almost_eq(slab.get_aabb().size.y, 0.1, 0.001)


func test_a_shape_is_made_once_and_shared() -> void:
	assert_same(Rounded.box(Vector3(1, 2, 3), 0.1), Rounded.box(Vector3(1, 2, 3), 0.1))
	assert_ne(Rounded.box(Vector3(1, 2, 3), 0.1), Rounded.box(Vector3(1, 2, 3), 0.2))
	assert_same(Rounded.wedge(Vector3(1, 1, 1)), Rounded.wedge(Vector3(1, 1, 1)))
	assert_same(Rounded.cylinder(1.0, 1.0), Rounded.cylinder(1.0, 1.0))


func test_an_instance_shows_the_mesh_in_its_own_material() -> void:
	var material := StandardMaterial3D.new()
	var node := Rounded.instance(Rounded.box(Vector3.ONE), material, Vector3(1, 2, 3))
	assert_eq(node.material_override, material)
	assert_eq(node.position, Vector3(1, 2, 3))
	node.free()
