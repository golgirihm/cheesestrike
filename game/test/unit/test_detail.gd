extends GutTest
## Less detail far away: small dressing stops being drawn at a distance, and
## a budget for how much each arena draws and how long a full match's frame
## takes.

const WORLD := preload("res://world/world.tscn")
const LOW := GraphicsQuality.LOW
const MEDIUM := GraphicsQuality.MEDIUM
const HIGH := GraphicsQuality.HIGH
## The most things an arena may draw from where a player stands, on low
## graphics and on any.
const DRAWN_ON_LOW := 460
const DRAWN_AT_MOST := 650
## The most milliseconds the game's own work may take in its worst frame of
## a second, with sixteen playing on the biggest arena: a frame at thirty a
## second, on a machine several times slower than a player's.
const FRAME_BUDGET_MS := 33.0

var saved := {}


func before_each() -> void:
	for key: String in ["quality", "gfx_effects"]:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, "")


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _world(quality: String) -> Node3D:
	Settings.set_value("quality", quality)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	return world


func test_smaller_things_go_sooner_and_sooner_still_on_lower_graphics() -> void:
	for level: String in GraphicsQuality.LEVELS:
		assert_has(Detail.REACH_PER_METRE, level)
		assert_lt(Detail.reach(0.5, level), Detail.reach(1.5, level), level)
		assert_eq(Detail.reach(Detail.ALWAYS_FROM, level), 0.0, "big things are always drawn")
		assert_eq(Detail.reach(40.0, level), 0.0)
		assert_eq(Detail.reach(0.01, level), Detail.NEAREST, "nothing goes while it is close")
		assert_eq(Detail.reach(0.0, level), 0.0, "what has no size is left alone")
	assert_lt(Detail.reach(1.0, LOW), Detail.reach(1.0, MEDIUM))
	assert_lt(Detail.reach(1.0, MEDIUM), Detail.reach(1.0, HIGH))
	assert_eq(Detail.reach(1.0, "nonsense"), Detail.reach(1.0, HIGH))
	# A thing is last drawn when it is a small part of the view: under a
	# hundredth of the distance across even on low.
	assert_gt(Detail.REACH_PER_METRE[LOW], 30.0)


func test_a_things_size_is_its_longest_side_as_it_stands() -> void:
	var post := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(0.1, 1.5, 0.1)
	post.mesh = box
	add_child_autofree(post)
	assert_almost_eq(Detail.size_of(post), 1.5, 0.001)
	post.scale = Vector3(1, 2, 1)
	assert_almost_eq(Detail.size_of(post), 3.0, 0.001)


func test_only_dressing_is_thinned_never_what_is_played_on() -> void:
	var world := _world(LOW)
	for id: String in Maps.ORDER:
		world.load_map(id)
		var thinned := 0
		for body in world.map.get_node("Geometry").get_children():
			var own: MeshInstance3D = body.get_node_or_null("MeshInstance3D")
			if own != null:
				assert_eq(own.visibility_range_end, 0.0, "%s: %s is solid, and always drawn" % [id, body.name])
		for holder: String in ["Pickups", "JumpPads", "SpawnPoints", "Objectives"]:
			var kept: Node = world.map.get_node_or_null(holder)
			if kept == null:
				continue
			for node in kept.find_children("*", "GeometryInstance3D", true, false):
				assert_eq(node.visibility_range_end, 0.0, "%s: %s/%s matters to play" % [id, holder, node.name])
		for node: GeometryInstance3D in world.map.find_children("*", "GeometryInstance3D", true, false):
			if node.visibility_range_end > 0.0:
				thinned += 1
				assert_lt(Detail.size_of(node), Detail.ALWAYS_FROM, "%s: %s is too big to go" % [id, node.name])
				assert_gte(node.visibility_range_end, Detail.NEAREST)
		assert_gt(thinned, 20, "%s has dressing to thin" % id)


func test_every_arena_draws_within_its_budget_from_wherever_a_player_starts() -> void:
	var eye := Vector3.UP * BodyShape.HEAD_HEIGHT * BodyShape.SIZE
	var drawn := {}
	for level: String in [LOW, HIGH]:
		var world := _world(level)
		for id: String in Maps.ORDER:
			world.load_map(id)
			await wait_process_frames(1)
			var most := 0
			for spot: Vector3 in world.spawn_positions():
				most = maxi(most, Detail.drawn_from(world.map, spot + eye))
			drawn[[id, level]] = most
			assert_lte(most, DRAWN_ON_LOW if level == LOW else DRAWN_AT_MOST, "%s on %s" % [id, level])
		world.free()
	for id: String in Maps.ORDER:
		assert_lt(drawn[[id, LOW]], drawn[[id, HIGH]], "%s draws less on low" % id)
	gut.p("most drawn from a spawn (low, high): %s" % [Maps.ORDER.map(func(id: String) -> String: return "%s %d/%d" % [id, drawn[[id, LOW]], drawn[[id, HIGH]]])])


func test_changing_the_graphics_thins_the_arena_again() -> void:
	var world := _world(HIGH)
	world.load_map("city")
	var eye: Vector3 = world.spawn_positions()[0] + Vector3.UP * 2.7
	var on_high := Detail.drawn_from(world.map, eye)
	Settings.set_value("quality", LOW)
	var on_low := Detail.drawn_from(world.map, eye)
	assert_lt(on_low, on_high)
	Settings.set_value("gfx_effects", HIGH)
	assert_eq(Detail.drawn_from(world.map, eye), on_high, "it follows the effects, chosen on their own")


func test_counting_what_is_drawn_heeds_distance_and_what_is_hidden() -> void:
	var holder := Node3D.new()
	add_child_autofree(holder)
	for index in 4:
		var thing := MeshInstance3D.new()
		thing.mesh = BoxMesh.new()
		holder.add_child(thing)
		thing.global_position = Vector3(index * 20.0, 0, 0)
		thing.visibility_range_end = 30.0 if index > 0 else 0.0
	assert_eq(Detail.drawn_from(holder, Vector3.ZERO), 2, "the two within thirty metres")
	assert_eq(Detail.drawn_from(holder, Vector3(60, 0, 0)), 3, "the two near it, and the one that is always drawn")
	assert_eq(Detail.drawn_from(holder, Vector3.ZERO, 10.0), 1)
	holder.get_child(0).visible = false
	assert_eq(Detail.drawn_from(holder, Vector3.ZERO), 1)


func test_a_full_match_on_the_biggest_arena_keeps_within_its_frame_budget() -> void:
	# The benchmark: sixteen players, fifteen of them bots the host plays,
	# on the arena with the most in it.
	var world := _world(HIGH)
	world.start(MatchSettings.from_dict({"map": "city", "bots": MatchSettings.MAX_PLAYERS - 1}))
	# Let the match start and the fighting begin: starting is not the test.
	await wait_seconds(3.0)
	assert_eq(world.players.get_child_count(), MatchSettings.MAX_PLAYERS)
	# The engine keeps the longest frame of each second.
	var worst := 0.0
	for second in 3:
		await wait_seconds(1.0)
		worst = maxf(worst, (Performance.get_monitor(Performance.TIME_PROCESS) + Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS)) * 1000.0)
	gut.p("worst frame of the game's own work: %.1f ms" % worst)
	assert_lt(worst, FRAME_BUDGET_MS)
