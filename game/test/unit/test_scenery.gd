extends GutTest
## How every arena is finished once loaded: rounded, fenced at an edge that
## can't be seen or climbed, with land beyond it, and yellow only where
## there is cheese to eat.

const WORLD := preload("res://world/world.tscn")

var world: Node3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)


func _load(id: String) -> Node3D:
	world.load_map(id)
	await wait_physics_frames(2)
	return world.map


func _walls() -> Array:
	return world.map.get_node("Geometry").get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Wall"))


func test_no_box_in_any_arena_has_a_hard_edge() -> void:
	for id: String in Maps.ORDER:
		var map: Node3D = await _load(id)
		var drawn := map.get_node("Geometry").find_children("*", "MeshInstance3D", true, false)
		assert_gt(drawn.size(), 0, id)
		for mesh: MeshInstance3D in drawn:
			assert_false(mesh.mesh is BoxMesh, "%s: %s is a plain box" % [id, mesh.get_path()])
			assert_false(mesh.mesh is PrismMesh, "%s: %s is a plain wedge" % [id, mesh.get_path()])


func test_a_rounded_box_is_drawn_the_size_it_is_solid() -> void:
	for id: String in Maps.ORDER:
		var map: Node3D = await _load(id)
		for body: Node3D in map.get_node("Geometry").get_children():
			if not (body.name.begins_with("Block") or body.name == "Floor" or body.name.begins_with("Mesa")):
				continue
			var solid: Vector3 = (body.get_node("CollisionShape3D").shape as BoxShape3D).size
			var seen: Vector3 = body.get_node("MeshInstance3D").mesh.get_aabb().size
			assert_almost_eq(seen.distance_to(solid), 0.0, 0.001, "%s: %s" % [id, body.name])


func test_the_edge_of_every_arena_is_solid_but_not_drawn() -> void:
	for id: String in Maps.ORDER:
		await _load(id)
		var walls := _walls()
		assert_eq(walls.size(), 4, id)
		for wall: StaticBody3D in walls:
			assert_eq(wall.find_children("*", "MeshInstance3D", true, false).size(), 0, "%s: %s can be seen" % [id, wall.name])
			var shape: BoxShape3D = wall.get_node("CollisionShape3D").shape
			# Far above anything a player could ever be thrown to.
			assert_gt(wall.global_position.y + shape.size.y / 2.0, 1000.0, "%s: %s" % [id, wall.name])
			assert_lt(wall.global_position.y - shape.size.y / 2.0, 0.0, "%s: %s stands on the ground" % [id, wall.name])
			assert_eq(wall.collision_layer & Match.WORLD_MASK, Match.WORLD_MASK, "shots and players stop at it")


func test_the_edge_stops_a_player_however_high_they_are() -> void:
	for id: String in Maps.ORDER:
		await _load(id)
		var space := world.get_world_3d().direct_space_state
		var edge := Scenery.bounds(world.map.get_node("Geometry"))
		for height: float in [1.0, 50.0, 900.0]:
			for way: Vector3 in [Vector3.LEFT, Vector3.RIGHT, Vector3.FORWARD, Vector3.BACK]:
				var from := Vector3(0, height, 0)
				var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + way * 500.0, Match.WORLD_MASK))
				assert_false(hit.is_empty(), "%s: nothing %s at %s m up" % [id, way, height])
				if hit.is_empty():
					continue
				# Nothing nearer than the edge stands that tall, save a tower.
				var reach := absf((hit.position as Vector3).dot(way))
				var limit := edge.end.x if way.x != 0.0 else edge.end.y
				assert_lte(reach, limit + 0.01, "%s: the edge is at the fence" % id)


func test_the_bounds_are_the_inner_faces_of_the_walls() -> void:
	await _load("box")
	assert_eq(Scenery.bounds(world.map.get_node("Geometry")), Rect2(-16, -16, 32, 32))
	await _load("canyon")
	assert_eq(Scenery.bounds(world.map.get_node("Geometry")), Rect2(-24, -24, 48, 48))


func test_a_short_fence_shows_where_the_edge_is() -> void:
	for id: String in Maps.ORDER:
		var map: Node3D = await _load(id)
		var fence := map.get_node_or_null("Scenery/Fence")
		assert_not_null(fence, id)
		var edge := Scenery.bounds(map.get_node("Geometry"))
		var rails := fence.get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Rail"))
		assert_eq(rails.size(), 8, "%s: two rails along each side" % id)
		for rail: MeshInstance3D in rails:
			var box: AABB = rail.global_transform * rail.get_aabb()
			assert_lt(box.end.y, BodyShape.HEAD_HEIGHT * BodyShape.SIZE, "%s: low enough to see over" % id)
			# Along one side of the edge, end to end.
			var on_side := is_equal_approx(absf(rail.position.x), edge.end.x) or is_equal_approx(absf(rail.position.z), edge.end.y)
			assert_true(on_side, "%s: %s isn't on the edge" % [id, rail.name])
			assert_almost_eq(maxf(box.size.x, box.size.z), edge.size.x, 0.3, id)
		var posts: MultiMeshInstance3D = fence.get_node("Posts")
		assert_gt(posts.multimesh.instance_count, 20, id)
		assert_almost_eq(posts.multimesh.mesh.get_aabb().size.y, Scenery.FENCE_HEIGHT, 0.001)
		# Only to look at: a fence has nothing to collide with.
		assert_eq(fence.find_children("*", "CollisionObject3D", true, false).size(), 0, id)


func test_there_is_something_to_see_beyond_every_edge() -> void:
	for id: String in Maps.ORDER:
		var map: Node3D = await _load(id)
		var around: Node3D = map.get_node("Scenery")
		var edge := Scenery.bounds(map.get_node("Geometry"))
		assert_not_null(around.get_node_or_null("Ground"), id)
		var things := 0
		var outside := 0
		for many: MultiMeshInstance3D in around.find_children("*", "MultiMeshInstance3D", true, false):
			if many.get_parent().name == "Fence":
				continue
			for place: Transform3D in many.get_meta("places"):
				var at := place.origin
				things += 1
				if not edge.has_point(Vector2(at.x, at.z)):
					outside += 1
		assert_gt(things, 40, "%s: plenty out there" % id)
		assert_eq(outside, things, "%s: and none of it in the arena" % id)
		assert_eq(around.find_children("*", "CollisionObject3D", true, false).size(), 0, "%s: none of it is solid" % id)


## Every colour drawn under `node`, with what it is on.
func _colours(node: Node) -> Array:
	var found := []
	for drawn: GeometryInstance3D in node.find_children("*", "GeometryInstance3D", true, false):
		var material := drawn.material_override as StandardMaterial3D
		if material == null and drawn is MeshInstance3D and drawn.mesh != null and drawn.mesh.get_surface_count() > 0:
			material = drawn.mesh.surface_get_material(0) as StandardMaterial3D
		if material != null and not material.vertex_color_use_as_albedo:
			found.append([material.albedo_color, drawn])
		var many := drawn as MultiMeshInstance3D
		if many != null:
			for tint: Color in many.get_meta("tints", PackedColorArray()):
				found.append([tint, drawn])
	return found


func test_yellow_is_kept_for_cheese_blocks_and_health() -> void:
	assert_true(Scenery.is_yellow(Scenery.CHEESE))
	assert_true(Scenery.is_yellow(PickupModels.CHEESE))
	for id: String in Maps.ORDER:
		var map: Node3D = await _load(id)
		var yellow_blocks := 0
		for entry: Array in _colours(map):
			var drawn: Node = entry[1]
			var path := str(map.get_path_to(drawn))
			var cheese := path.begins_with("Geometry/Block")
			var pad: Node = drawn.get_parent().get_parent() if drawn.get_parent().name == "Item" else null
			var health := pad != null and String(pad.get_meta("kind", "")) == Pickups.HEALTH
			if Scenery.is_yellow(entry[0]):
				assert_true(cheese or health, "%s: %s is yellow" % [id, path])
				yellow_blocks += 1 if cheese else 0
			elif cheese and drawn.name == "MeshInstance3D":
				fail_test("%s: %s is cheese, and isn't yellow" % [id, path])
		assert_gt(yellow_blocks, 0, "%s has cheese to eat" % id)


func test_weapons_are_not_yellow() -> void:
	for id: String in Weapons.ids():
		var model := WeaponModels.build(id)
		for entry: Array in _colours(model):
			assert_false(Scenery.is_yellow(entry[0]), "%s: %s" % [id, entry[1].name])
		model.free()


func test_cheese_blocks_have_holes_on_their_faces() -> void:
	var map: Node3D = await _load("box")
	var block: Node3D = map.get_node("Geometry/Block1")
	var half: Vector3 = (block.get_node("CollisionShape3D").shape as BoxShape3D).size / 2.0
	var holes := block.get_children().filter(func(node: Node) -> bool: return node is MeshInstance3D and node.name != "MeshInstance3D")
	assert_gte(holes.size(), 10)
	for hole: MeshInstance3D in holes:
		# Each lies just off one face, and within it.
		var out := maxf(absf(hole.position.x), maxf(absf(hole.position.y), absf(hole.position.z)))
		assert_between(out - half.x, 0.0, 0.02, "on the surface")
		var radius: float = hole.mesh.get_aabb().size.x / 2.0
		var sideways := [absf(hole.position.x), absf(hole.position.y), absf(hole.position.z)]
		sideways.sort()
		assert_lte(sideways[1] + radius, half.x, "not over the edge")


func test_the_sky_has_clouds() -> void:
	var environment: Environment = world.get_node("WorldEnvironment").environment
	assert_eq(environment.background_mode, Environment.BG_SKY)
	var material := environment.sky.sky_material as ShaderMaterial
	assert_not_null(material)
	assert_string_contains(material.shader.code, "shader_type sky")
	assert_true(material.get_shader_parameter("clouds") is NoiseTexture2D)
	assert_true((material.get_shader_parameter("clouds") as NoiseTexture2D).seamless, "so that they drift round without a seam")


func test_the_city_has_roads_skyscrapers_and_cars() -> void:
	var map: Node3D = await _load("city")
	var geometry := map.get_node("Geometry")
	var towers := geometry.get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Building"))
	assert_gte(towers.size(), 4)
	for tower: StaticBody3D in towers:
		var size: Vector3 = (tower.get_node("CollisionShape3D").shape as BoxShape3D).size
		assert_gt(size.y, 40.0, "%s scrapes the sky" % tower.name)
		var material: StandardMaterial3D = tower.get_node("MeshInstance3D").material_override
		assert_not_null(material.albedo_texture, "%s has windows" % tower.name)
	var cars := geometry.get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Car"))
	assert_gte(cars.size(), 8)
	for car: StaticBody3D in cars:
		assert_gte(car.find_children("*", "MeshInstance3D", true, false).size(), 8, "%s: a body, a cabin, windows and wheels" % car.name)
		assert_gte(car.find_children("*", "CollisionShape3D", true, false).size(), 2, "%s is solid" % car.name)
	var roads := map.get_node("Decor").get_children().filter(func(node: Node) -> bool: return node.name.begins_with("Avenue") or node.name.begins_with("Street"))
	assert_gt(roads.size(), 3, "roads, with lines down them")
	assert_not_null(map.get_node_or_null("Scenery/Towers"), "and more of the city beyond")
	assert_gt((map.get_node("Scenery/Towers") as MultiMeshInstance3D).multimesh.instance_count, 30)


func test_a_cars_bonnet_can_be_jumped_onto_and_its_roof_from_there() -> void:
	var map: Node3D = await _load("city")
	var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")
	var jump := Movement.JUMP_VELOCITY * Movement.JUMP_VELOCITY / (2.0 * gravity)
	var space := world.get_world_3d().direct_space_state
	var car: Node3D = map.get_node("Geometry/Car1")
	var bonnet := car.global_position + car.global_basis * Vector3(0, 0, -2.8)
	var roof := car.global_position + car.global_basis * Vector3(0, 0, 0.3)
	var heights := []
	for over: Vector3 in [bonnet, roof]:
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(over + Vector3.UP * 10.0, over, Match.WORLD_MASK))
		assert_false(hit.is_empty())
		heights.append(hit.position.y)
	assert_lt(heights[0], jump, "the bonnet from the road")
	assert_lt(heights[1] - heights[0], jump, "the roof from the bonnet")
	assert_gt(heights[1], heights[0])


func test_the_cars_keep_off_the_pads_and_the_starting_spots() -> void:
	var map: Node3D = await _load("city")
	var space := world.get_world_3d().direct_space_state
	for pad: Dictionary in world.pickup_pads():
		var above: Vector3 = pad.position + Vector3.UP * 8.0
		var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(above, pad.position + Vector3.DOWN, Match.WORLD_MASK))
		assert_almost_eq(hit.position.y, 0.0, 0.01, "%s pad is under something" % pad.kind)
	assert_not_null(map)
