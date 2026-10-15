extends GutTest
## The large maps (see BigMap): a minute or more from one side's start to
## the other's, the same for both sides, built mostly of cheese, with ways
## across them that bots can find.

const WORLD := preload("res://world/world.tscn")

var world: Node3D


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)


func _load(id: String) -> PhysicsDirectSpaceState3D:
	world.load_map(id)
	await wait_physics_frames(2)
	return world.get_world_3d().direct_space_state


func _large() -> Array:
	return Maps.ORDER.filter(func(id: String) -> bool: return Maps.is_large(id))


# A square of points, linked round its edge, with one across the middle.
func _square() -> Routes:
	return Routes.new({"points": [Vector3(0, 0, 0), Vector3(100, 0, 0), Vector3(100, 0, 100), Vector3(0, 0, 100), Vector3(50, 0, 50)],
		"links": [[0, 1], [1, 2], [2, 3], [3, 0], [0, 4]]})


func test_the_shortest_way_goes_along_the_links() -> void:
	var routes := _square()
	assert_eq(routes.shortest(0, 2), [0, 1, 2] as Array[int], "round one side or the other: the first found of two the same")
	assert_eq(routes.shortest(4, 2), [4, 0, 1, 2] as Array[int], "back out the way in, there being no other")
	assert_almost_eq(routes.distance(4, 2), Vector2(50, 50).length() + 200.0, 0.01)
	assert_eq(routes.shortest(1, 1), [1] as Array[int])
	assert_eq(routes.shortest(0, 9), [] as Array[int], "to nowhere")
	var apart := Routes.new({"points": [Vector3.ZERO, Vector3(80, 0, 0)], "links": []})
	assert_eq(apart.shortest(0, 1), [] as Array[int], "with no link, no way")
	assert_eq(apart.distance(0, 1), INF)


func test_a_way_is_only_wanted_for_somewhere_far_and_starts_past_where_one_is() -> void:
	var routes := _square()
	assert_eq(routes.way(Vector3(1, 0, 1), Vector3(20, 0, 20)), [] as Array[Vector3], "near enough to walk straight")
	# Standing on the corner already, the next is the first to make for.
	assert_eq(routes.way(Vector3(1, 0, 1), Vector3(99, 0, 99)), [Vector3(100, 0, 0), Vector3(100, 0, 100)] as Array[Vector3])
	assert_eq(Routes.new().way(Vector3.ZERO, Vector3(500, 0, 0)), [] as Array[Vector3], "a map with none")
	# A floor's difference counts double in what is nearest.
	var floors := Routes.new({"points": [Vector3(0, 7, 0), Vector3(10, 0, 0)], "links": []})
	assert_eq(floors.nearest(Vector3(0, 0, 0)), 1)
	assert_eq(floors.nearest(Vector3(0, 6, 0)), 0)


func test_a_way_is_clear_unless_it_goes_through_something() -> void:
	var box := Rect2(10, -5, 10, 10)
	assert_true(BigMap.crosses(Vector2(0, 0), Vector2(30, 0), box), "straight through")
	assert_true(BigMap.crosses(Vector2(0, -20), Vector2(30, 20), box), "across a corner of it")
	assert_true(BigMap.crosses(Vector2(12, 0), Vector2(40, 40), box), "from inside it")
	assert_false(BigMap.crosses(Vector2(0, 6), Vector2(30, 6), box), "along beside it")
	assert_false(BigMap.crosses(Vector2(0, 0), Vector2(9, 0), box), "stopping short of it")
	assert_false(BigMap.crosses(Vector2(15, 10), Vector2(15, 40), box), "going away from it")
	var town := BigMap.new("Test", Scenery.MEADOW, Vector2(100, 100))
	town.cheese(Vector3(0, 1.5, 0), Vector3(3, 3, 3))
	assert_false(town.is_clear(Vector3(-10, 0, 0), Vector3(10, 0, 0)))
	assert_false(town.is_clear(Vector3(-10, 0, 2.0), Vector3(10, 0, 2.0)), "nor so close that a player would scrape it")
	assert_true(town.is_clear(Vector3(-10, 0, 4.0), Vector3(10, 0, 4.0)))
	town.map.free()


func test_each_building_is_drawn_as_one_until_a_piece_of_it_is_eaten() -> void:
	world.start(MatchSettings.from_dict({"map": "city"}))
	await wait_physics_frames(2)
	var clusters: Clusters = world.clusters
	var before: Array = clusters.counts()
	assert_eq(before.size(), 18, "every house")
	for building: Array in before:
		assert_gt(building[1], 10, "each of many pieces")
	# Its pieces are solid, each on its own, but not drawn on their own.
	var piece: Node3D = world.cheese_blocks().filter(func(body: Node3D) -> bool: return body.get_meta("group", -1) == before[0][0])[0]
	assert_false(piece.get_node("MeshInstance3D").visible)
	var drawn: MeshInstance3D = clusters.get_node("Building%d" % before[0][0])
	var faces := drawn.mesh.get_faces().size()
	world.match_._hurt_block(world.block_index(piece), BlockRules.HEALTH)
	world._on_state_changed()
	assert_lt(clusters.counts()[0][1], before[0][1], "fewer pieces standing")
	assert_lt(drawn.mesh.get_faces().size(), faces, "and fewer drawn")
	assert_eq(clusters.counts().slice(1), before.slice(1), "the rest as they were")


func test_nonsense_makes_no_points_or_links() -> void:
	var routes := Routes.new({"points": [Vector3.ZERO, "x", Vector3.ONE], "links": [[0, 1], [0, 7], "y", [1]]})
	assert_eq(routes.points.size(), 2)
	assert_eq(routes.linked(0), PackedInt32Array([1]))
	assert_eq(routes.linked(5), PackedInt32Array())


func test_a_bot_far_from_where_it_wants_goes_by_the_way_there() -> void:
	var brain := BotBrain.new(BotBrain.NORMAL, 3)
	brain.routes = _square()
	var me := {"at": Vector3(2, 0, 2), "health": 100, "ammo": 10, "magazine": 10, "reloading": false, "range": 50.0}
	var clear := func(_from: Vector3, _to: Vector3) -> bool: return false
	var orders := brain.think(0.1, me, [], {"objective": Vector3(98, 0, 98)}, clear)
	assert_almost_eq((orders.move as Vector3).dot(Vector3.RIGHT), 1.0, 0.01, "along the first side, not across the middle")
	assert_eq(brain.way()[0], Vector3(100, 0, 0))
	# Once near, straight at it.
	me.at = Vector3(99, 0, 70)
	orders = brain.think(0.1, me, [], {"objective": Vector3(98, 0, 98)}, clear)
	assert_gt((orders.move as Vector3).dot(Vector3.BACK), 0.99)


func test_every_large_map_is_a_minute_or_two_from_one_start_to_the_other() -> void:
	assert_true(Maps.is_large("city"))
	assert_false(Maps.is_large("duel"), "a duel stays close")
	assert_false(Maps.is_large(Maps.RANGE))
	var running := Movement.WALK_SPEED * Movement.RUN_FACTOR
	var arenas := 0.0
	var small := Maps.ORDER.filter(func(id: String) -> bool: return not Maps.is_large(id))
	for id: String in small:
		await _load(id)
		arenas += Scenery.bounds(world.map.get_node("Geometry")).get_area() / small.size()
	for id: String in _large():
		await _load(id)
		var bases: Array = world.objective_spots().bases
		assert_eq(bases.size(), 2, id)
		var routes: Routes = world.routes
		assert_false(routes.is_empty(), "%s has ways across it" % id)
		var across := routes.distance(routes.nearest(bases[0]), routes.nearest(bases[1]))
		gut.p("%s: %.0f m from base to base, %.0f s at a run" % [id, across, across / running])
		assert_between(across / running, 60.0, 120.0, "%s: a minute or two at a run" % id)
		assert_gt(Scenery.bounds(world.map.get_node("Geometry")).get_area(), 100.0 * arenas, "%s: a hundred times the area of an arena" % id)


func test_every_way_bots_take_can_be_walked() -> void:
	for id: String in _large():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		var routes: Routes = world.routes
		# Through the doors, which open for whoever comes.
		var doors: Array[RID] = []
		for door: Node in get_tree().get_nodes_in_group("doors"):
			doors.append((door as CollisionObject3D).get_rid())
		for index in routes.points.size():
			var point: Vector3 = routes.points[index]
			for other: int in routes.linked(index):
				if other < index:
					continue
				for height: float in [0.8, BodyShape.HEIGHT - 0.3]:
					var lift := Vector3.UP * height
					var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(point + lift, routes.points[other] + lift, Match.WORLD_MASK, doors))
					assert_true(hit.is_empty(), "%s: from %s to %s is blocked at %.1f m by %s" % [id, point, routes.points[other], height, hit.get("collider", {}).get("name") if not hit.is_empty() else ""])
		# And every point is joined to every other.
		for index in routes.points.size():
			assert_lt(routes.distance(0, index), INF, "%s: %s can be got to" % [id, routes.points[index]])


func test_both_sides_have_the_same_map_turned_round() -> void:
	for id: String in _large():
		await _load(id)
		var boxes: Array = world.block_boxes()
		var centres := {}
		for box: AABB in boxes:
			centres[(box.get_center() * 10.0).round()] = box.size
		for box: AABB in boxes:
			var turned := box.get_center() * Vector3(-1, 1, -1)
			assert_true(centres.has((turned * 10.0).round()), "%s: the cheese at %s has none opposite" % [id, box.get_center()])
		var spots: Array = world.spawn_positions()
		for spot: Vector3 in spots:
			assert_true(spots.any(func(other: Vector3) -> bool: return other.distance_to(spot * Vector3(-1, 1, -1)) < 0.01), "%s: %s" % [id, spot])


func test_most_of_what_stands_is_cheese_and_it_all_goes_to_everyone() -> void:
	for id: String in _large():
		await _load(id)
		var standing: Array = world.map.get_node("Geometry").get_children().filter(func(body: Node) -> bool:
			return not (body.name == "Floor" or body.name.begins_with("Wall") or body.name.begins_with("Car") or body.name.begins_with("Bus") or body.name.begins_with("Ramp")))
		var cheese: int = world.cheese_blocks().size()
		gut.p("%s: %d pieces of cheese of %d" % [id, cheese, standing.size()])
		assert_gt(float(cheese) / standing.size(), 0.9, "%s: nearly all of it" % id)
		assert_lt(cheese, BlockRules.MOST, id)
		var rules := BlockRules.new(cheese)
		rules.hurt(cheese - 1, 30.0)
		var sent := MatchState.new()
		sent.blocks = rules
		var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(sent.to_dict())))
		assert_eq(copy.blocks.health, rules.health, "%s: every block, exactly" % id)
		assert_lt(var_to_bytes(sent.to_dict()).size() - var_to_bytes(sent.to_dict(false)).size(), cheese * 2 + 64, "%s: in two bytes a block" % id)


func test_every_ladder_leads_somewhere_to_stand() -> void:
	for id: String in _large():
		var space: PhysicsDirectSpaceState3D = await _load(id)
		var ladders: Array = world.fixtures.ladders
		assert_gt(ladders.size(), 10, "%s: roofs to climb to" % id)
		for ladder: Dictionary in ladders:
			var over: Vector3 = ladder.position + ladder.facing * (BigMap.LADDER_OUT + 1.6) + Vector3.UP * (ladder.height + 1.0)
			var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(over, over + Vector3.DOWN * 2.0, Match.WORLD_MASK))
			assert_false(hit.is_empty(), "%s: nothing at the top of the ladder at %s" % [id, ladder.position])
			if not hit.is_empty():
				assert_almost_eq(hit.position.y, ladder.position.y + ladder.height, 0.05, "%s: the top of the ladder at %s" % [id, ladder.position])


func test_on_a_large_map_each_team_starts_and_comes_back_at_its_own_end() -> void:
	world.start(MatchSettings.from_dict({"map": "city", "mode": MatchSettings.TEAMS, "bots": 5}))
	await wait_physics_frames(3)
	var match_: Match = world.match_
	var bases: Array = world.objective_spots().bases
	match_._new_match()
	await wait_physics_frames(2)
	for id: int in match_.state.players:
		var record: PlayerRecord = match_.state.player(id)
		var at: Vector3 = world.player_node(id).global_position
		assert_lt(at.distance_to(bases[record.team]), at.distance_to(bases[1 - record.team]), "%d starts at their own end" % id)
		# And comes back there, wherever they were knocked out.
		world.player_node(id).global_position = bases[1 - record.team]
	for id: int in match_.state.players:
		match_._respawn(id)
	await wait_physics_frames(2)
	for id: int in match_.state.players:
		var record: PlayerRecord = match_.state.player(id)
		var at: Vector3 = world.player_node(id).global_position
		assert_lt(at.distance_to(bases[record.team]), 60.0, "%d comes back at their own end" % id)


func test_bots_on_a_large_map_set_off_across_it() -> void:
	world.start(MatchSettings.from_dict({"map": "city", "bots": 3}))
	# Any that sees an enemy fights instead, so this machine's own player is
	# kept out of sight, under the town; and however long a slow machine
	# takes for the match to begin, the bots are watched for a while after.
	var going := {}
	var watched := 0
	for frame in 1200:
		await wait_physics_frames(1)
		world.player_node(1).global_position = Vector3(0, -500, 0)
		if not world.match_.state.is_playing():
			continue
		watched += 1
		if watched > 300 or not going.is_empty():
			break
		for id: int in world.bot_ids():
			var bot: Node3D = world.player_node(id)
			if bot != null and not bot.get_node("BotDriver").brain.way().is_empty():
				going[id] = true
	assert_gt(going.size(), 0, "at least one is going by the streets")
