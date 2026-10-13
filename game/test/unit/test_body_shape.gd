extends GutTest
## That shots are tested against the wedge everyone sees: rays fired at the
## mesh a player's wedge is drawn with, as the physics engine finds it, hit
## and miss where BodyShape says they do.

const PLAYER := preload("res://player/player.tscn")
## A physics layer nothing else in these tests is on.
const DRAWN_LAYER := 1 << 15
## The wedge is drawn with its edges rounded off and tested with them sharp,
## so a ray that just grazes an edge or the tip may count on one side only.
## Anything else out of place would show in far more rays than this.
const ALLOWED_DISAGREEMENT := 0.03
const DISTANCE_TOLERANCE := BodyShape.WEDGE_ROUNDING * BodyShape.SIZE

var _drawn: StaticBody3D


func _player(yaw: float, pitch: float) -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	# Not this process's own player, so its body is drawn.
	player.name = "2"
	add_child_autofree(player)
	player.rotation.y = yaw
	player.head.rotation.x = pitch
	return player


## A body that collides exactly where the meshes under `part` are drawn.
func _collide_as_drawn(part: Node3D) -> void:
	if _drawn != null:
		_drawn.free()
	var faces := PackedVector3Array()
	var meshes: Array = part.find_children("*", "MeshInstance3D", true, false)
	if part is MeshInstance3D:
		meshes.append(part)
	for mesh: MeshInstance3D in meshes:
		var place := mesh.global_transform
		for corner in mesh.mesh.get_faces():
			faces.append(place * corner)
	var shape := ConcavePolygonShape3D.new()
	shape.backface_collision = true
	shape.set_faces(faces)
	var collider := CollisionShape3D.new()
	collider.shape = shape
	_drawn = StaticBody3D.new()
	_drawn.collision_layer = DRAWN_LAYER
	_drawn.collision_mask = 0
	_drawn.add_child(collider)
	add_child_autofree(_drawn)


## Rays across the whole character from `side`, a grid of them filling the
## space its capsule could take up, aimed through the player's upright middle.
func _rays_from(feet: Vector3, side: Vector3) -> Array:
	var away := side.normalized() * (BodyShape.RADIUS + 3.0)
	var across := Vector3.UP.cross(side).normalized()
	if across.is_zero_approx():
		across = Vector3.RIGHT
	var up := side.cross(across).normalized()
	var rays := []
	var steps := 24
	for i in steps + 1:
		for j in steps + 1:
			var sideways := (float(i) / steps - 0.5) * 2.0 * BodyShape.RADIUS
			var height := float(j) / steps * BodyShape.HEIGHT
			var through := feet + Vector3.UP * height + across * sideways
			if side.normalized().is_equal_approx(Vector3.UP):
				through = feet + Vector3.UP * (BodyShape.HEIGHT / 2.0) + across * sideways + up * (float(j) / steps - 0.5) * 2.0 * BodyShape.RADIUS
			rays.append([through + away, -side.normalized()])
	return rays


func test_shots_hit_and_miss_where_the_wedge_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var reach := (BodyShape.RADIUS + 3.0) * 2.0 + BodyShape.HEIGHT
	for pose: Vector2 in [Vector2(0.0, 0.0), Vector2(0.7, -0.5), Vector2(2.4, 0.4), Vector2(-1.9, 1.2)]:
		var player := _player(pose.x, pose.y)
		await wait_process_frames(1)
		_collide_as_drawn(player.body.get_node("Wedge"))
		await wait_physics_frames(2)
		var feet := player.global_position
		var rays := 0
		var hits := 0
		var disagreements := 0
		for side in [Vector3.FORWARD, Vector3.BACK, Vector3.LEFT, Vector3(1, 0, 1), Vector3(-1, 0.6, 0.5), Vector3(0.3, 1, 0.2)]:
			for ray: Array in _rays_from(feet, side):
				var from: Vector3 = ray[0]
				var direction: Vector3 = ray[1]
				var query := PhysicsRayQueryParameters3D.create(from, from + direction * reach, DRAWN_LAYER)
				var drawn := space.intersect_ray(query)
				var tested := BodyShape.ray_hit(from, direction, feet, pose.x)
				rays += 1
				hits += 0 if tested.is_empty() else 1
				if drawn.is_empty() != tested.is_empty():
					disagreements += 1
				elif not drawn.is_empty() and absf(tested.distance - from.distance_to(drawn.position)) > DISTANCE_TOLERANCE:
					disagreements += 1
		assert_gt(hits, rays / 10, "pose %s: plenty of rays meet the wedge" % pose)
		assert_lte(float(disagreements) / rays, ALLOWED_DISAGREEMENT,
				"pose %s: %d of %d rays hit only one of the drawn wedge and its hit shape" % [pose, disagreements, rays])
		player.free()


func test_legs_are_drawn_but_cannot_be_hit() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var player := _player(0.0, 0.0)
	await wait_process_frames(1)
	var feet := player.global_position
	for limb: String in ["Leg1", "Leg2"]:
		_collide_as_drawn(player.body.get_node(limb))
		await wait_physics_frames(2)
		# From in front, through the far end of the limb, clear of the wedge.
		var stick: MeshInstance3D = player.body.get_node(limb).find_children("*", "MeshInstance3D", true, false)[0]
		var bounds: AABB = stick.global_transform * stick.get_aabb()
		var end := Vector3(bounds.get_center().x, bounds.position.y + 0.15, bounds.get_center().z)
		var from := end + Vector3.FORWARD * 6.0
		assert_false(space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.BACK * 12.0, DRAWN_LAYER)).is_empty(), "%s is drawn there" % limb)
		assert_true(BodyShape.ray_hit(from, Vector3.BACK, feet).is_empty(), "%s can't be hit" % limb)
	player.free()


func test_a_shot_straight_ahead_at_someone_level_meets_their_face() -> void:
	var eye_level := BodyShape.HEAD_HEIGHT * BodyShape.SIZE
	for yaw in [0.0, PI, 0.5, -2.0]:
		var hit := BodyShape.ray_hit(Vector3(0, eye_level, 10), Vector3.FORWARD, Vector3.ZERO, yaw)
		assert_false(hit.is_empty(), "facing %s" % yaw)
		assert_true(hit.head, "facing %s" % yaw)


func test_the_head_is_the_face_and_the_tip_above_it() -> void:
	var feet := Vector3.ZERO
	var eye: Vector3 = BodyShape.eye_centres()[0] * BodyShape.SIZE
	var at_eye := BodyShape.ray_hit(eye + Vector3.FORWARD * 5.0, Vector3.BACK, feet)
	assert_true(at_eye.head)
	assert_almost_eq(at_eye.distance, 5.0, 0.001, "the eyes are on the surface of the wedge")
	var tip := Vector3(0, (BodyShape.WEDGE_CENTRE.y + BodyShape.WEDGE_SIZE.y * 0.4) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(tip, Vector3.BACK, feet).head)
	var low := Vector3(0, (BodyShape.HIP_HEIGHT + 0.2) * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(low, Vector3.BACK, feet).head)


func test_every_hole_lies_flat_on_a_face_of_the_wedge() -> void:
	for hole: Array in BodyShape.HOLES:
		var place := BodyShape.hole_place(hole)
		# A ray straight down onto the hole meets the wedge right at it.
		var above: Vector3 = (place.centre + place.normal * 1.0) * BodyShape.SIZE
		var hit := BodyShape.ray_hit(above, -place.normal, Vector3.ZERO)
		assert_false(hit.is_empty(), str(hole))
		assert_almost_eq(hit.distance, BodyShape.SIZE, 0.001, "%s is on the surface" % [hole])
		# And all of it is on that face: its rim is too.
		var along: Vector3 = place.normal.cross(Vector3.UP if absf(place.normal.y) < 0.9 else Vector3.RIGHT).normalized()
		for way: Vector3 in [along, -along, place.normal.cross(along), -place.normal.cross(along)]:
			var rim: Vector3 = (place.centre + way * place.radius + place.normal * 1.0) * BodyShape.SIZE
			var at_rim := BodyShape.ray_hit(rim, -place.normal, Vector3.ZERO)
			assert_false(at_rim.is_empty(), "%s hangs over the edge" % [hole])
			if not at_rim.is_empty():
				assert_almost_eq(at_rim.distance, BodyShape.SIZE, 0.001, "%s wraps round an edge" % [hole])


func test_a_burst_is_measured_to_the_nearest_point_up_the_middle() -> void:
	var feet := Vector3(1, 2, 3)
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 4.5, 3)), Vector3(1, 4.5, 3))
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, -10, 3)), feet + Vector3.UP * BodyShape.RADIUS, "from below, low down")
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 99, 3)), feet + Vector3.UP * (BodyShape.HEIGHT - BodyShape.RADIUS), "from above, high up")


func test_a_crouched_player_is_lower_and_shots_follow() -> void:
	var feet := Vector3.ZERO
	var tip := Vector3(0, (BodyShape.WEDGE_CENTRE.y + BodyShape.WEDGE_SIZE.y * 0.45) * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(tip, Vector3.BACK, feet).is_empty(), "standing, the tip is there")
	assert_true(BodyShape.ray_hit(tip, Vector3.BACK, feet, 0.0, 1.0).is_empty(), "crouched, it has come down")
	# What was between the legs is now the bottom of the wedge.
	var low := Vector3(0, (BodyShape.HIP_HEIGHT - 0.1) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(low, Vector3.BACK, feet).is_empty())
	assert_false(BodyShape.ray_hit(low, Vector3.BACK, feet, 0.0, 1.0).is_empty())
	# The face is still the head, wherever it is.
	var eye := Vector3(0, BodyShape.head_height(1.0) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(eye, Vector3.BACK, feet, 0.0, 1.0).head)
	assert_eq(BodyShape.head_height(0.0), BodyShape.HEAD_HEIGHT)
	assert_almost_eq(BodyShape.head_height(1.0), BodyShape.HEAD_HEIGHT - BodyShape.CROUCH_DROP, 0.0001)
	assert_lt(BodyShape.CROUCH_DROP, BodyShape.HIP_HEIGHT, "the legs fold; they don't vanish")


func test_a_crouched_wedge_is_hit_where_it_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var player := _player(0.6, 0.0)
	player.body.set_crouch(1.0)
	await wait_process_frames(1)
	player.body.set_crouch(1.0)
	_collide_as_drawn(player.body.get_node("Wedge"))
	await wait_physics_frames(2)
	var feet := player.global_position
	var reach := (BodyShape.RADIUS + 3.0) * 2.0 + BodyShape.HEIGHT
	var rays := 0
	var disagreements := 0
	for side in [Vector3.FORWARD, Vector3.LEFT, Vector3(1, 0.4, 1)]:
		for ray: Array in _rays_from(feet, side):
			var drawn := space.intersect_ray(PhysicsRayQueryParameters3D.create(ray[0], ray[0] + ray[1] * reach, DRAWN_LAYER))
			var tested := BodyShape.ray_hit(ray[0], ray[1], feet, 0.6, 1.0)
			rays += 1
			if drawn.is_empty() != tested.is_empty():
				disagreements += 1
	assert_lte(float(disagreements) / rays, ALLOWED_DISAGREEMENT, "%d of %d rays" % [disagreements, rays])
	player.free()
