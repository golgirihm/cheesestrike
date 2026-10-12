extends GutTest
## That shots are tested against exactly the character everyone sees: rays
## fired at the meshes a player is drawn with, as the physics engine finds
## them, hit and miss where BodyShape says they do.

const PLAYER := preload("res://player/player.tscn")
## A physics layer nothing else in these tests is on.
const DRAWN_LAYER := 1 << 15
## Spheres and capsules are drawn with flat facets, a hair inside the true
## round shape, so a ray that just grazes one may count on one side only.
## Anything else out of place would show in far more rays than this.
const ALLOWED_DISAGREEMENT := 0.01
const DISTANCE_TOLERANCE := 0.02 * BodyShape.SIZE

var _drawn: StaticBody3D


func _player(yaw: float, pitch: float) -> CharacterBody3D:
	var player: CharacterBody3D = PLAYER.instantiate()
	# Not this process's own player, so its body and eyes are drawn.
	player.name = "2"
	add_child_autofree(player)
	player.rotation.y = yaw
	player.head.rotation.x = pitch
	return player


## A body that collides exactly where `player`'s body and eyes are drawn.
func _collide_as_drawn(player: Node3D) -> void:
	if _drawn != null:
		_drawn.free()
	var faces := PackedVector3Array()
	for part: Node3D in [player.body, player.visor]:
		for mesh: MeshInstance3D in part.find_children("*", "MeshInstance3D", true, false):
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


func test_shots_hit_and_miss_exactly_where_the_character_is_drawn() -> void:
	var space := get_viewport().world_3d.direct_space_state
	var reach := (BodyShape.RADIUS + 3.0) * 2.0 + BodyShape.HEIGHT
	for pose: Vector2 in [Vector2(0.0, 0.0), Vector2(0.7, -0.5), Vector2(2.4, 0.4), Vector2(-1.9, 1.2)]:
		var player := _player(pose.x, pose.y)
		await wait_process_frames(1)
		_collide_as_drawn(player)
		await wait_physics_frames(2)
		var feet := player.global_position
		var rays := 0
		var disagreements := 0
		for side in [Vector3.FORWARD, Vector3.BACK, Vector3.LEFT, Vector3(1, 0, 1), Vector3(-1, 0.6, 0.5), Vector3(0.3, 1, 0.2)]:
			for ray: Array in _rays_from(feet, side):
				var from: Vector3 = ray[0]
				var direction: Vector3 = ray[1]
				var query := PhysicsRayQueryParameters3D.create(from, from + direction * reach, DRAWN_LAYER)
				var drawn := space.intersect_ray(query)
				var tested := BodyShape.ray_hit(from, direction, feet, pose.x, pose.y)
				rays += 1
				# A ray grazing a round part may meet only one of them, or meet
				# the other further on, behind it.
				if drawn.is_empty() != tested.is_empty():
					disagreements += 1
				elif not drawn.is_empty() and absf(tested.distance - from.distance_to(drawn.position)) > DISTANCE_TOLERANCE:
					disagreements += 1
		assert_lte(float(disagreements) / rays, ALLOWED_DISAGREEMENT,
				"pose %s: %d of %d rays hit only one of the drawn character and its hit shape" % [pose, disagreements, rays])
		player.free()


func test_the_head_is_the_eyes_and_the_tip_above_them() -> void:
	var feet := Vector3.ZERO
	var eye: Vector3 = BodyShape.eye_centres(0.0)[0] * BodyShape.SIZE
	var at_eye := BodyShape.ray_hit(eye + Vector3.FORWARD * 5.0, Vector3.BACK, feet)
	assert_true(at_eye.head)
	var chest := Vector3(0, BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE, -5)
	assert_false(BodyShape.ray_hit(chest, Vector3.BACK, feet).head)


func test_a_burst_is_measured_to_the_nearest_point_up_the_middle() -> void:
	var feet := Vector3(1, 2, 3)
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 2.5, 3)), Vector3(1, 2.5, 3))
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, -10, 3)), feet, "from below, the feet")
	assert_eq(BodyShape.nearest_middle(feet, Vector3(5, 99, 3)), feet + Vector3.UP * BodyShape.HEIGHT, "from above, the top")
