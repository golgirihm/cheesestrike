extends GutTest
## Inside the city's north-east tower: a lobby behind a door, a ramp to the
## floor above, and windows to shoot from.

const WORLD := preload("res://world/world.tscn")
const SECOND := 6.4

var world: Node3D
var space: PhysicsDirectSpaceState3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "city"}))
	await wait_physics_frames(3)
	space = world.get_world_3d().direct_space_state


# Whether a player could stand with their feet at `feet`.
func _fits(feet: Vector3) -> bool:
	var body := PhysicsShapeQueryParameters3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = BodyShape.RADIUS
	capsule.height = BodyShape.HEIGHT
	body.shape = capsule
	body.collision_mask = 1
	body.transform = Transform3D(Basis.IDENTITY, feet + Vector3.UP * (BodyShape.HEIGHT / 2.0 + 0.08))
	return space.intersect_shape(body).is_empty()


func _floor_under(at: Vector3) -> float:
	var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.5, at + Vector3.DOWN * 20.0, 1))
	return hit.position.y if not hit.is_empty() else -INF


func test_a_player_fits_in_the_lobby_and_on_the_floor_above() -> void:
	for at: Vector3 in [Vector3(22, 0, -20), Vector3(25.5, 0, -18.5), Vector3(18, 0, -22), Vector3(22, SECOND, -20), Vector3(18.5, SECOND, -18.5), Vector3(26, SECOND, -17.5)]:
		assert_true(_fits(at), "room to stand at %s" % at)
	assert_almost_eq(_floor_under(Vector3(22, SECOND + 1.0, -20)), SECOND, 0.02, "a floor to stand on upstairs")
	assert_false(_fits(Vector3(22, 14.0, -22)), "the tower above is solid")


func test_the_door_is_the_way_in_and_opens_for_whoever_comes() -> void:
	var door: Dictionary = world.moving_parts.doors[0]
	assert_gt(door.size.y, BodyShape.HEIGHT)
	assert_gt(door.size.z, BodyShape.RADIUS * 2.0 + 1.0)
	# Shut, the doorway is filled; open, a player fits through it.
	var doorway := Vector3(door.position.x, 0, door.position.z)
	assert_false(_fits(doorway))
	world.player_node(1).global_position = doorway + Vector3(-4, 0.1, 0)
	for step in 120:
		world.player_node(1).global_position = doorway + Vector3(-4, 0.1, 0)
		world.match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	assert_eq(world.match_.state.doors, [true])
	assert_true(_fits(doorway), "through the open doorway")
	assert_true(_fits(doorway + Vector3(2.5, 0, 0)), "and on into the lobby")


func test_the_ramp_can_be_walked_up_to_the_floor_above() -> void:
	var ramp: Node3D = world.map.get_node("Geometry/LobbyRamp")
	var up := ramp.global_basis.y
	assert_gt(up.dot(Vector3.UP), cos(deg_to_rad(40.0)), "not too steep to walk")
	# Along it, the floor rises steadily from the lobby to the floor above,
	# with headroom all the way.
	var last := -1.0
	var x := 17.5
	while x <= 26.5:
		var height := _floor_under(Vector3(x, 12.0, -26.5))
		assert_gt(height, last - 0.01, "at x=%.1f" % x)
		# A little clear of it, as a round foot stands on a slope.
		assert_true(_fits(Vector3(x, height + 0.35, -26.5)), "headroom at x=%.1f" % x)
		last = height
		x += 1.0
	assert_gt(last, SECOND - Movement.STEP_HEIGHT, "its top is a step from the floor above")
	assert_true(_fits(Vector3(26.5, SECOND, -23.0)), "and there is somewhere to step to")


func test_the_windows_look_out_on_the_street_and_the_walls_do_not() -> void:
	var cast := Match.wall_caster(space)
	for base: float in [0.0, SECOND]:
		var eye := Vector3(18.6, base + BodyShape.HEAD_HEIGHT * BodyShape.SIZE, -18.0)
		# Out through the south window to the east road.
		assert_eq(cast.call(eye, Vector3(18.6, eye.y, -8.0)), INF, "a clear shot from %.1f up" % base)
		# The pier between the windows stops one.
		var blocked: Vector3 = Vector3(22.0, eye.y, -18.0)
		assert_lt(cast.call(blocked, Vector3(22.0, eye.y, -8.0)), 5.0)
		# And nobody sees in under the sill.
		assert_lt(cast.call(Vector3(18.6, base + 0.9, -18.0), Vector3(18.6, base + 0.9, -8.0)), 5.0)
	# Upstairs there is a window over the north road as well.
	var west := Vector3(19.0, SECOND + BodyShape.HEAD_HEIGHT * BodyShape.SIZE, -22.0)
	assert_eq(cast.call(west, Vector3(8.0, west.y, -22.0)), INF)


func test_there_is_something_to_come_in_for_and_the_floors_have_names() -> void:
	var inside: Array = world.pickup_pads().filter(func(pad: Dictionary) -> bool: return pad.position.x > 15.5 and pad.position.x < 28.5 and pad.position.z < -15.5 and pad.position.z > -28.5)
	assert_eq(inside.size(), 2)
	assert_eq(world.place_at(Vector3(22, 0, -22)), "The Lobby")
	assert_eq(world.place_at(Vector3(22, SECOND, -20)), "Upstairs")
	assert_ne(world.place_at(Vector3(0, 0, 0)), "The Lobby")


func test_the_tower_still_reads_as_a_tower_from_the_street() -> void:
	var tower: Node3D = world.map.get_node("Geometry/BuildingNorthEast")
	var size: Vector3 = (tower.get_node("CollisionShape3D").shape as BoxShape3D).size
	assert_almost_eq(tower.position.y + size.y / 2.0, 60.0, 0.01, "as tall as it was")
	assert_almost_eq(tower.position.y - size.y / 2.0, 13.0, 0.01, "from above the floors that are open")
	assert_true((tower.get_node("MeshInstance3D").material_override as StandardMaterial3D).has_meta("windows"))
