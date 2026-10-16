extends GutTest
## That shots are tested against the soldier everyone sees: rays fired at
## the meshes a player's head, torso and legs are drawn with, as the physics
## engine finds them, hit and miss where BodyShape says they do.

const PLAYER := preload("res://player/player.tscn")
## A physics layer nothing else in these tests is on.
const DRAWN_LAYER := 1 << 15
## The parts are drawn with their edges rounded off and tested with them
## sharp, so a ray that just grazes an edge may count on one side only.
## Anything else out of place would show in far more rays than this.
const ALLOWED_DISAGREEMENT := 0.03
const DISTANCE_TOLERANCE := BodyShape.ROUNDING * BodyShape.SIZE
## The parts of a character that can be hit.
const HIT_PARTS := ["Torso", "Head", "Leg1", "Leg2"]

var _drawn: StaticBody3D


func _player(yaw: float, pitch: float) -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	# Not this process's own player, so its body is drawn.
	player.name = "2"
	add_child_autofree(player)
	player.rotation.y = yaw
	player.head.rotation.x = pitch
	return player


## A body that collides exactly where the meshes under `parts` are drawn.
func _collide_as_drawn(parts: Array) -> void:
	if _drawn != null:
		_drawn.free()
	var faces := PackedVector3Array()
	var meshes: Array = []
	for part: Node3D in parts:
		meshes.append_array(part.find_children("*", "MeshInstance3D", true, false))
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
func _hit_parts(player: Node3D) -> Array:
	return HIT_PARTS.map(func(part: String) -> Node3D: return player.body.get_node(part))


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


func test_shots_hit_and_miss_where_the_soldier_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var reach := (BodyShape.RADIUS + 3.0) * 2.0 + BodyShape.HEIGHT
	for pose: Vector2 in [Vector2(0.0, 0.0), Vector2(0.7, -0.5), Vector2(2.4, 0.4), Vector2(-1.9, 1.2)]:
		var player := _player(pose.x, pose.y)
		await wait_process_frames(1)
		_collide_as_drawn(_hit_parts(player))
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
		assert_gt(hits, rays / 10, "pose %s: plenty of rays meet the soldier" % pose)
		assert_lte(float(disagreements) / rays, ALLOWED_DISAGREEMENT,
				"pose %s: %d of %d rays hit only one of the drawn soldier and its hit shape" % [pose, disagreements, rays])
		player.free()


func test_the_free_arm_is_drawn_but_cannot_be_hit() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var player := _player(0.0, 0.0)
	await wait_process_frames(1)
	var feet := player.global_position
	var arm: Node3D = player.body.get_node("Arm1")
	_collide_as_drawn([arm])
	await wait_physics_frames(2)
	# From in front, through the glove at the end of the arm, clear of the
	# torso and the legs.
	var meshes := arm.find_children("*", "MeshInstance3D", true, false)
	var glove: MeshInstance3D = meshes[meshes.size() - 1]
	var from := glove.global_position + Vector3.FORWARD * 6.0
	assert_false(space.intersect_ray(PhysicsRayQueryParameters3D.create(from, from + Vector3.BACK * 12.0, DRAWN_LAYER)).is_empty(), "the arm is drawn there")
	assert_true(BodyShape.ray_hit(from, Vector3.BACK, feet).is_empty(), "the arm can't be hit")
	player.free()


func test_a_shot_straight_ahead_at_someone_level_meets_their_face() -> void:
	var eye_level := BodyShape.HEAD_HEIGHT * BodyShape.SIZE
	for yaw in [0.0, PI, 0.5, -2.0]:
		var hit := BodyShape.ray_hit(Vector3(0, eye_level, 10), Vector3.FORWARD, Vector3.ZERO, yaw)
		assert_false(hit.is_empty(), "facing %s" % yaw)
		assert_true(hit.head, "facing %s" % yaw)


func test_the_head_is_the_face_and_the_helmet_above_it() -> void:
	var feet := Vector3.ZERO
	var eye: Vector3 = BodyShape.eye_centres()[0] * BodyShape.SIZE
	var at_eye := BodyShape.ray_hit(eye + Vector3.FORWARD * 5.0, Vector3.BACK, feet)
	assert_true(at_eye.head)
	assert_almost_eq(at_eye.distance, 5.0, 0.001, "the eyes are on the surface of the head")
	var helmet := Vector3(0, (BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y * 0.4) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(helmet, Vector3.BACK, feet).head)
	var chest := Vector3(0, (BodyShape.HIP_HEIGHT + 0.2) * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(chest, Vector3.BACK, feet).head)
	var shin := Vector3(BodyShape.LEGS[0][0].x * BodyShape.SIZE, 0.3, -5)
	var at_shin := BodyShape.ray_hit(shin, Vector3.BACK, feet)
	assert_false(at_shin.is_empty(), "the legs can be hit")
	assert_false(at_shin.head)
	assert_true(BodyShape.ray_hit(Vector3(BodyShape.HEAD_SIZE.x * BodyShape.SIZE, eye.y, -5), Vector3.BACK, feet).is_empty(), "beside the head, nothing")


func test_a_burst_is_measured_to_the_nearest_point_up_the_middle() -> void:
	var feet := Vector3(1, 2, 3)
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 4.5, 3)), Vector3(1, 4.5, 3))
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, -10, 3)), feet + Vector3.UP * BodyShape.RADIUS, "from below, low down")
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 99, 3)), feet + Vector3.UP * (BodyShape.HEIGHT - BodyShape.RADIUS), "from above, high up")


func test_a_crouched_player_is_lower_and_shots_follow() -> void:
	var feet := Vector3.ZERO
	var top := Vector3(0, (BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y * 0.4) * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(top, Vector3.BACK, feet).is_empty(), "standing, the helmet is there")
	assert_true(BodyShape.ray_hit(top, Vector3.BACK, feet, 0.0, 1.0).is_empty(), "crouched, it has come down")
	# What was between the legs is now the bottom of the torso.
	var low := Vector3(0, (BodyShape.HIP_HEIGHT - 0.1) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(low, Vector3.BACK, feet).is_empty())
	assert_false(BodyShape.ray_hit(low, Vector3.BACK, feet, 0.0, 1.0).is_empty())
	# The face is still the head, wherever it is.
	var eye := Vector3(0, BodyShape.head_height(1.0) * BodyShape.SIZE, -5)
	assert_true(BodyShape.ray_hit(eye, Vector3.BACK, feet, 0.0, 1.0).head)
	assert_eq(BodyShape.head_height(0.0), BodyShape.HEAD_HEIGHT)
	assert_almost_eq(BodyShape.head_height(1.0), BodyShape.HEAD_HEIGHT - BodyShape.CROUCH_DROP, 0.0001)
	assert_lt(BodyShape.CROUCH_DROP, BodyShape.HIP_HEIGHT, "the knees bend; the legs don't vanish")
	assert_eq(BodyShape.knee_bend(0.0), 0.0, "standing straight")
	assert_almost_eq(BodyShape.THIGH * 2.0 * cos(BodyShape.knee_bend(1.0)), BodyShape.HIP_HEIGHT - BodyShape.CROUCH_DROP, 0.0001, "the hips down by the drop")


func test_a_crouched_soldier_is_hit_where_it_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var player := _player(0.6, 0.0)
	player.body.set_crouch(1.0)
	await wait_process_frames(1)
	player.body.set_crouch(1.0)
	_collide_as_drawn(_hit_parts(player))
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


func test_leaning_tips_a_player_to_one_side_on_their_feet() -> void:
	var right := BodyShape.head_offset(0.0, 1.0)
	var left := BodyShape.head_offset(0.0, -1.0)
	assert_gt(right.x, 0.2, "the eyes go out to the right")
	assert_almost_eq(left.x, -right.x, 0.0001)
	assert_lt(right.y, BodyShape.HEAD_HEIGHT, "and come down a little")
	assert_almost_eq(right.length(), BodyShape.HEAD_HEIGHT, 0.0001, "the same distance from the feet")
	assert_eq(BodyShape.head_offset(0.0, 0.0), Vector3(0, BodyShape.HEAD_HEIGHT, 0))
	assert_almost_eq(BodyShape.head_offset(1.0, 1.0).length(), BodyShape.head_height(1.0), 0.0001, "crouched as well")
	assert_eq(BodyShape.lean_basis(5.0), BodyShape.lean_basis(1.0), "no further than all the way")


func test_shots_follow_a_leaning_player() -> void:
	var feet := Vector3.ZERO
	# Facing -Z, so their right is +X. A shot from in front, at where the
	# top of the helmet is when they stand upright.
	var top := Vector3(0, (BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y * 0.4) * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(top, Vector3.BACK, feet).is_empty())
	assert_true(BodyShape.ray_hit(top, Vector3.BACK, feet, 0.0, 0.0, 1.0).is_empty(), "leaning, the head has gone to one side")
	# It is where their head now is.
	var head := BodyShape.head_offset(0.0, 1.0) * BodyShape.SIZE + Vector3(0, 0, -5)
	var hit := BodyShape.ray_hit(head, Vector3.BACK, feet, 0.0, 0.0, 1.0)
	assert_false(hit.is_empty())
	assert_true(hit.head)
	# And the other way for the other lean, and for a player turned about.
	assert_true(BodyShape.ray_hit(head, Vector3.BACK, feet, 0.0, 0.0, -1.0).is_empty())
	var mirrored := Vector3(-head.x, head.y, 5)
	assert_false(BodyShape.ray_hit(mirrored, Vector3.FORWARD, feet, PI, 0.0, 1.0).is_empty(), "their right is the other way when they face the other way")


func test_a_leaning_soldier_is_hit_where_it_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var player := _player(0.6, 0.0)
	player.net_state = PackedFloat32Array([0, 0, 0, 0.6, 0, 0, player.POSE_LEAN_RIGHT])
	await wait_process_frames(1)
	player._animate(1.0)
	assert_eq(player.lean(), 1.0)
	_collide_as_drawn(_hit_parts(player))
	await wait_physics_frames(2)
	var feet := player.global_position
	var reach := (BodyShape.RADIUS + 3.0) * 2.0 + BodyShape.HEIGHT
	var rays := 0
	var hits := 0
	var disagreements := 0
	for side in [Vector3.FORWARD, Vector3.LEFT, Vector3(1, 0.4, 1)]:
		for ray: Array in _rays_from(feet, side):
			var drawn := space.intersect_ray(PhysicsRayQueryParameters3D.create(ray[0], ray[0] + ray[1] * reach, DRAWN_LAYER))
			var tested := BodyShape.ray_hit(ray[0], ray[1], feet, 0.6, 0.0, 1.0)
			rays += 1
			hits += 0 if tested.is_empty() else 1
			if drawn.is_empty() != tested.is_empty():
				disagreements += 1
	assert_gt(hits, rays / 10)
	assert_lte(float(disagreements) / rays, ALLOWED_DISAGREEMENT, "%d of %d rays" % [disagreements, rays])
	player.free()
