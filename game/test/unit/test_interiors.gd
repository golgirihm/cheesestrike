extends GutTest
## Inside the houses a large map is built of (see BigMap): a door in at the
## front and the back, a ramp up from each floor to the next, windows to
## shoot from, a ladder up to the roof and a low wall round it.

const WORLD := preload("res://world/world.tscn")
const AT := Vector2(10, -6)
const FOOTPRINT := Vector2(30, 22)
const FLOORS := 3

var map: Node3D
var space: PhysicsDirectSpaceState3D
var found: Dictionary


# A house of three floors standing on its own, turned a quarter round so
# that its own frame and the map's differ.
func _build(turn := 1, is_cheese := true) -> void:
	var town := BigMap.new("Test", Scenery.MEADOW, Vector2(120, 120))
	found = town.house(AT, FOOTPRINT, FLOORS, turn, {"cheese": is_cheese, "label": "The House", "inside": [Pickups.HEALTH, Pickups.AMMO]})
	map = town.finish(Transform3D.IDENTITY)
	add_child_autofree(map)
	await wait_physics_frames(2)
	space = map.get_world_3d().direct_space_state


func before_each() -> void:
	await _build()


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
	var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(at + Vector3.UP * 0.5, at + Vector3.DOWN * 30.0, 1))
	return hit.position.y if not hit.is_empty() else -INF


# A point of the house's own frame (X along its front, Z from back to front)
# as it stands in the map, turned as `_build` turns it.
func _at(local: Vector3) -> Vector3:
	return BigMap._world({"at": AT, "turn": 1}, local)


func test_a_player_fits_on_every_floor_with_a_floor_to_stand_on() -> void:
	assert_eq(found.floors.size(), FLOORS)
	for storey in FLOORS:
		var middle: Vector3 = found.floors[storey]
		assert_almost_eq(middle.y, storey * BigMap.STOREY, 0.01)
		# The middle of the floor, clear of the ramps along the front and back.
		assert_true(_fits(middle + Vector3.UP * 0.05), "room to stand on floor %d" % storey)
		assert_almost_eq(_floor_under(middle + Vector3.UP), middle.y, 0.02, "something to stand on, on floor %d" % storey)
	assert_gt(BigMap.STOREY - BigMap.SLAB, BodyShape.HEIGHT + 0.5, "headroom under every ceiling")
	assert_almost_eq(_floor_under(found.roof + Vector3.UP), found.roof.y, 0.02, "and the roof can be stood on")
	assert_true(_fits(found.roof + Vector3.UP * 0.05))


func test_each_ramp_can_be_walked_up_to_the_floor_above() -> void:
	var ramps := map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return body.name.begins_with("Ramp"))
	assert_eq(ramps.size(), FLOORS - 1, "one up from every floor but the top")
	for ramp: Node3D in ramps:
		assert_gt(ramp.global_basis.y.dot(Vector3.UP), cos(deg_to_rad(35.0)), "%s is not too steep to walk" % ramp.name)
	var inner := Rect2(-FOOTPRINT / 2.0 + Vector2.ONE * BigMap.WALL, FOOTPRINT - Vector2.ONE * BigMap.WALL * 2.0)
	for storey in FLOORS - 1:
		var well := BigMap._well(inner, storey)
		var rising := 1.0 if storey % 2 == 0 else -1.0
		var bottom := well.position.x + 1.0 if rising > 0.0 else well.end.x - 1.0
		var middle := well.get_center().y
		# Along it the floor rises steadily from this floor to the one above,
		# with headroom all the way.
		var last := -INF
		for step in int(BigMap.RAMP_RUN) + 1:
			var along := _at(Vector3(bottom + rising * step, (storey + 1) * BigMap.STOREY + 0.2, middle))
			var height := _floor_under(along)
			assert_gt(height, last - 0.01, "floor %d, %d m along" % [storey, step])
			# Clear of the wall at its foot, which a player steps on from the side.
			if step >= 1:
				assert_true(_fits(Vector3(along.x, height + 0.35, along.z)), "headroom on floor %d's ramp, %d m along" % [storey, step])
			last = height
		assert_gt(last, (storey + 1) * BigMap.STOREY - Movement.STEP_HEIGHT, "its top is a step from floor %d" % (storey + 1))
		assert_lt(_floor_under(_at(Vector3(bottom, storey * BigMap.STOREY + 1.0, middle))), storey * BigMap.STOREY + Movement.STEP_HEIGHT, "its foot is a step from floor %d" % storey)
		# And off its top there is floor to step on to.
		var off := _at(Vector3(bottom + rising * (BigMap.RAMP_RUN + 2.5), (storey + 1) * BigMap.STOREY, middle))
		assert_almost_eq(_floor_under(off + Vector3.UP), (storey + 1) * BigMap.STOREY, 0.02)
		assert_true(_fits(off + Vector3.UP * 0.05), "room at the top of floor %d's ramp" % storey)


func test_the_doorways_are_ways_in_and_each_has_a_door() -> void:
	var doors: Array = MovingParts.of(map).doors
	assert_eq(doors.size(), 2, "the front and the back")
	for door: Dictionary in doors:
		assert_gt(door.size.y, BodyShape.HEIGHT, "tall enough to walk through")
		assert_gt(maxf(door.size.x, door.size.z), BodyShape.RADIUS * 2.0 + 1.0, "and wide enough")
		assert_almost_eq(door.slide.y, -(door.size.y + 0.05), 0.01, "it goes down out of the way")
		# With no door drawn, as before the scenery dresses it, the doorway is clear.
		var doorway := Vector3(door.position.x, 0.05, door.position.z)
		assert_true(_fits(doorway), "a player fits through at %s" % doorway)
	# Outside each, and just inside the front one, a player stands clear.
	for spot: Vector3 in [found.front, found.back, found.inside]:
		assert_true(_fits(spot + Vector3.UP * 0.05), "room at %s" % spot)


func test_the_windows_look_out_and_the_walls_between_do_not() -> void:
	var cast := Match.wall_caster(space)
	var half := FOOTPRINT / 2.0
	for storey in range(1, FLOORS):
		var eye := storey * BigMap.STOREY + BodyShape.HEAD_HEIGHT * BodyShape.SIZE
		# Out through the window in the middle of the front, onto the street.
		var inside := _at(Vector3(0, eye, half.y - 3.0))
		assert_eq(cast.call(inside, _at(Vector3(0, eye, half.y + 15.0))), INF, "a clear shot from floor %d" % storey)
		# The wall beside it stops one, and nobody sees in under the sill.
		var beside := _at(Vector3(half.x - 3.0, eye, half.y - 3.0))
		assert_lt(cast.call(beside, _at(Vector3(half.x - 3.0, eye, half.y + 15.0))), 5.0)
		var low := _at(Vector3(0, storey * BigMap.STOREY + 0.9, half.y - 3.0))
		assert_lt(cast.call(low, _at(Vector3(0, low.y, half.y + 15.0))), 5.0)
	# On the ground floor, only the doors: the sides are solid.
	var side := _at(Vector3(-half.x + 3.0, 2.7, 0))
	assert_lt(cast.call(side, _at(Vector3(-half.x - 15.0, 2.7, 0))), 5.0)


func test_a_ladder_goes_from_the_street_to_the_roof_through_a_gap_in_its_wall() -> void:
	var ladders: Array = Fixtures.of(map).ladders
	assert_eq(ladders.size(), 1)
	var ladder: Dictionary = ladders[0]
	assert_almost_eq(ladder.height, found.roof.y, 0.01, "from the ground to the roof")
	assert_almost_eq(ladder.position.y, 0.0, 0.01)
	# A climber stands outside the wall at its foot, facing into it.
	assert_true(_fits(ladder.position + Vector3.UP * 0.05), "room at its foot")
	var ahead: Vector3 = ladder.position + ladder.facing * (BigMap.LADDER_OUT + 0.5) + Vector3.UP * 2.0
	assert_false(_fits(ahead - Vector3.UP * 2.0), "the house is just ahead")
	assert_false(Fixtures.ladder_at(Fixtures.of(map).ladders, ladder.position).is_empty(), "held at its foot")
	assert_false(Fixtures.ladder_at(Fixtures.of(map).ladders, ladder.position + Vector3.UP * ladder.height).is_empty(), "and at its top")
	# At the top, over the wall onto the roof, through the gap in the low wall.
	var over: Vector3 = ladder.position + ladder.facing * (BigMap.LADDER_OUT + BigMap.WALL + 1.6)
	over.y = found.roof.y
	assert_almost_eq(_floor_under(over + Vector3.UP), found.roof.y, 0.02)
	assert_true(_fits(over + Vector3.UP * 0.05), "room to step off it onto the roof")
	# Elsewhere the low wall round the roof is cover to crouch behind.
	var low: float = found.roof.y + 1.0
	assert_lt(Match.wall_caster(space).call(_at(Vector3(0, low, FOOTPRINT.y / 2.0 - 3.0)), _at(Vector3(0, low, FOOTPRINT.y / 2.0 + 10.0))), 5.0)


func test_what_a_cheese_house_is_built_of_is_cheese_and_its_walls_fall_from_the_bottom() -> void:
	var pieces := map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return not (body.name == "Floor" or body.name.begins_with("Wall") or body.name.begins_with("Ramp")))
	assert_gt(pieces.size(), 40, "walls, floors and a roof")
	for piece: Node in pieces:
		assert_true(piece.name.begins_with("Block"), "%s is cheese" % piece.name)
	# Eating a wall of the ground floor takes the wall above it with it, but
	# not the floor upstairs, which is held up by all the walls round it.
	var boxes := []
	var blocks := []
	for piece: Node3D in pieces:
		var size: Vector3 = (piece.get_node("CollisionShape3D").shape as BoxShape3D).size
		boxes.append(AABB(piece.position - size / 2.0, size))
		blocks.append(piece)
	var supports := BlockRules.supports(boxes)
	var lowest_wall := -1
	for index in boxes.size():
		var box: AABB = boxes[index]
		# A whole side of the ground floor, which has no windows.
		if box.position.y < 0.01 and box.size.y > BigMap.STOREY - 0.01 and maxf(box.size.x, box.size.z) > 10.0:
			lowest_wall = index
			break
	assert_ne(lowest_wall, -1, "a ground-floor wall")
	var falls := BlockRules.above(supports, lowest_wall)
	assert_gt(falls.size(), 0, "something stands on it")
	for index in falls:
		var box: AABB = boxes[index]
		assert_lt(box.size.y, BigMap.STOREY + 0.01, "%s is a piece of wall, not a floor" % blocks[index].name)
		assert_false(is_equal_approx(box.size.y, BigMap.SLAB), "%s is not a floor" % blocks[index].name)


func test_a_house_of_stone_is_not_eaten() -> void:
	remove_child(map)
	await _build(0, false)
	var cheese := map.get_node("Geometry").get_children().filter(func(body: Node) -> bool: return body.name.begins_with("Block"))
	assert_eq(cheese.size(), 0)


func test_there_is_something_to_come_in_for_and_the_floors_have_names() -> void:
	var pads := map.get_node("Pickups").get_children()
	assert_eq(pads.size(), 2)
	for pad: Node3D in pads:
		assert_almost_eq(_floor_under(pad.position + Vector3.UP), 0.0, 0.02, "on the ground floor")
		assert_true(_fits(pad.position + Vector3.UP * 0.05), "%s pad has room to stand on" % pad.name)
	var named := map.get_node("Places").get_children().map(func(marker: Node) -> String: return marker.get_meta("label"))
	assert_eq(named, ["The House", "The House, upstairs", "The House roof"])


func test_in_the_city_a_door_opens_for_whoever_comes_and_shuts_after() -> void:
	remove_child(map)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "city"}))
	await wait_physics_frames(3)
	space = world.get_world_3d().direct_space_state
	var doors: Array = world.moving_parts.doors
	assert_gt(doors.size(), 20, "two for every house")
	var door: Dictionary = doors[0]
	var doorway := Vector3(door.position.x, 0.05, door.position.z)
	assert_false(_fits(doorway), "shut, the doorway is filled")
	# Just outside it, on whichever side is open ground.
	var out := Vector3(door.size.z, 0, door.size.x).normalized() * 4.0
	var outside := doorway + out if _fits(doorway + out * 1.5) else doorway - out
	for step in 120:
		world.player_node(1).global_position = outside
		world.match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	assert_true(world.match_.state.doors[0], "it opens")
	assert_eq(world.match_.state.doors.count(true), 1, "and only that one")
	assert_true(_fits(doorway), "through the open doorway")
