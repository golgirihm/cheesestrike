extends GutTest
## The cheese blocks get eaten: by mice, through a match, and by shots.

const WORLD := preload("res://world/world.tscn")
const CHEST := BodyShape.CHEST.y * BodyShape.SIZE

var world: Node3D
var match_: Match


func _start(map := "box") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map(map)
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": map}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3, weapon := "blaster") -> Node3D:
	match_.add_player(id)
	var record := match_.state.player(id)
	if not record.weapons.has(weapon):
		record.weapons.append(weapon)
	record.weapon = weapon
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- The rules alone --------------------------------------------------------

func test_a_block_takes_so_much_and_then_is_gone() -> void:
	var rules := BlockRules.new(3)
	assert_eq(rules.health, [BlockRules.HEALTH, BlockRules.HEALTH, BlockRules.HEALTH] as Array[int])
	assert_false(rules.hurt(1, BlockRules.HEALTH - 1.0))
	assert_true(rules.standing(1))
	assert_true(rules.hurt(1, 5.0), "that finished it")
	assert_false(rules.standing(1))
	assert_eq(rules.health[1], 0)
	assert_false(rules.hurt(1, 50.0), "it can't go twice")
	assert_true(rules.standing(0), "the others are untouched")


func test_nonsense_hurts_nothing() -> void:
	var rules := BlockRules.new(2)
	for amount: float in [0.0, -5.0, NAN, INF]:
		assert_false(rules.hurt(0, amount))
	assert_false(rules.hurt(-1, 50.0))
	assert_false(rules.hurt(9, 50.0))
	assert_eq(rules.health[0], BlockRules.HEALTH)
	assert_false(rules.standing(9))


func test_the_smallest_hit_still_takes_something() -> void:
	var rules := BlockRules.new(1)
	rules.hurt(0, 0.2)
	assert_eq(rules.health[0], BlockRules.HEALTH - 1)


func test_a_block_shows_how_far_gone_it_is() -> void:
	var rules := BlockRules.new(1)
	assert_eq(rules.stage(0), 0)
	var seen := [0]
	for step in BlockRules.HEALTH / 10:
		rules.hurt(0, 10.0)
		if rules.stage(0) != seen[-1]:
			assert_eq(rules.stage(0), seen[-1] + 1, "one stage at a time")
			seen.append(rules.stage(0))
	assert_eq(seen, range(BlockRules.STAGES + 2), "whole, each stage, then gone")
	assert_eq(rules.stage(7), BlockRules.STAGES + 1, "one that isn't there is gone")


func test_the_mice_come_out_after_a_while_and_then_every_so_often() -> void:
	var rules := BlockRules.new(4, 5)
	assert_eq(rules.tick(BlockRules.FIRST_GNAW - 0.5), -1)
	var first := rules.tick(1.0)
	assert_between(first, 0, 3)
	assert_eq(rules.tick(BlockRules.GNAW_EVERY - 0.5), -1)
	assert_between(rules.tick(1.0), 0, 3)
	assert_gt(BlockRules.GNAW * 3, BlockRules.HEALTH - 1, "three visits finish a block")
	assert_lt(BlockRules.GNAW * 2, BlockRules.HEALTH, "two do not")


func test_the_mice_go_only_to_blocks_that_are_left_and_nowhere_when_none_are() -> void:
	var rules := BlockRules.new(3, 11)
	rules.remove(0)
	rules.remove(2)
	for visit in 6:
		rules.gnaw_in = 0.0
		assert_eq(rules.tick(0.1), 1)
	rules.remove(1)
	rules.gnaw_in = 0.0
	assert_eq(rules.tick(0.1), -1)
	assert_eq(BlockRules.new(0).tick(BlockRules.FIRST_GNAW + 1.0), -1, "an arena with no cheese")


func test_the_same_seed_sends_them_to_the_same_blocks() -> void:
	var one := BlockRules.new(8, 42)
	var other := BlockRules.new(8, 42)
	for visit in 10:
		one.gnaw_in = 0.0
		other.gnaw_in = 0.0
		assert_eq(one.tick(0.1), other.tick(0.1))


func test_a_new_match_makes_every_block_whole_and_sends_the_mice_home() -> void:
	var rules := BlockRules.new(2)
	rules.hurt(0, 9999.0)
	rules.tick(BlockRules.FIRST_GNAW - 1.0)
	rules.reset(2)
	assert_eq(rules.health, [BlockRules.HEALTH, BlockRules.HEALTH] as Array[int])
	assert_eq(rules.gnaw_in, BlockRules.FIRST_GNAW)


func test_what_stands_on_a_block_goes_with_it() -> void:
	# Two side by side, one on top of the first, one on top of that, and one
	# on the ground touching nothing.
	var boxes := [AABB(Vector3(0, 0, 0), Vector3(2, 2, 2)), AABB(Vector3(2, 0, 0), Vector3(2, 2, 2)), AABB(Vector3(0, 2, 0), Vector3(2, 2, 2)),
		AABB(Vector3(0, 4, 0), Vector3(2, 2, 2)), AABB(Vector3(9, 0, 9), Vector3(2, 2, 2))]
	assert_eq(BlockRules.resting_on(boxes, 0), [2, 3] as Array[int], "the one on it, and the one on that")
	assert_eq(BlockRules.resting_on(boxes, 1), [] as Array[int], "its neighbour stands by itself")
	assert_eq(BlockRules.resting_on(boxes, 2), [3] as Array[int])
	assert_eq(BlockRules.resting_on(boxes, 3), [] as Array[int])
	# One across two: it goes when either does.
	var bridge := [AABB(Vector3(0, 0, 0), Vector3(2, 2, 2)), AABB(Vector3(2, 0, 0), Vector3(2, 2, 2)), AABB(Vector3(1, 2, 0), Vector3(2, 2, 2))]
	assert_eq(BlockRules.resting_on(bridge, 0), [2] as Array[int])
	assert_eq(BlockRules.resting_on(bridge, 1), [2] as Array[int])


func test_the_blocks_survive_the_trip_and_nonsense_is_whole() -> void:
	var state := MatchState.new()
	state.blocks = BlockRules.new(3)
	state.blocks.hurt(1, 100.0)
	state.blocks.remove(2)
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(copy.blocks.health, [BlockRules.HEALTH, BlockRules.HEALTH - 100, 0] as Array[int])
	assert_eq(BlockRules.from_array([9999, -4, "x", NAN]).health, [BlockRules.HEALTH, 0, BlockRules.HEALTH, BlockRules.HEALTH] as Array[int])
	assert_eq(BlockRules.from_array("nonsense").health.size(), 0)
	assert_eq(BlockRules.from_array([1, 2, 3], 2).health, [1, 2] as Array[int], "no more than the arena has")
	assert_false(MatchState.new().to_dict().has("blocks"), "nothing sent where there is no cheese")
	state.start_match()
	assert_eq(state.blocks.health, [BlockRules.HEALTH, BlockRules.HEALTH, BlockRules.HEALTH] as Array[int])


# --- In the arena -----------------------------------------------------------

func test_every_arena_has_cheese_and_the_host_counts_it() -> void:
	await _start()
	for id: String in Maps.ORDER:
		world.load_map(id)
		match_.start(MatchSettings.from_dict({"map": id}))
		var blocks: Array = world.cheese_blocks()
		assert_gt(blocks.size(), 0, id)
		assert_eq(match_.state.blocks.health.size(), blocks.size(), id)
		assert_eq(world.block_boxes().size(), blocks.size())
		assert_eq(world.block_index(blocks[0]), 0)
		assert_eq(world.block_index(world.get_node("Match")), -1)
		for box: AABB in world.block_boxes():
			assert_gt(box.size.x * box.size.y * box.size.z, 1.0, id)


func test_shooting_a_block_wears_it_away_and_shooting_past_it_does_not() -> void:
	await _start()
	var block: Node3D = world.cheese_blocks()[0]
	var shooter := _add(1, block.global_position + Vector3(0, -block.global_position.y, 8))
	var origin: Vector3 = shooter.head.global_position
	var before: int = match_.state.blocks.health[0]
	match_._shot(1, origin, (block.global_position - origin).normalized(), 1, "blaster", WeaponStats.AIMED)
	assert_eq(match_.state.blocks.health[0], before - 20)
	match_._clock += 1.0
	match_._shot(1, origin, Vector3.UP, 1, "blaster", WeaponStats.AIMED)
	assert_eq(match_.state.blocks.health[0], before - 20, "the sky is not cheese")
	for index in range(1, match_.state.blocks.health.size()):
		assert_eq(match_.state.blocks.health[index], BlockRules.HEALTH, "only the one that was hit")


func test_a_player_in_front_of_a_block_takes_the_shot_instead() -> void:
	await _start()
	var block: Node3D = world.cheese_blocks()[0]
	var feet := Vector3(block.global_position.x, 0, block.global_position.z)
	var shooter := _add(1, feet + Vector3(0, 0, 12))
	_add(2, feet + Vector3(0, 0, 5))
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (feet + Vector3(0, CHEST, 5) - origin).normalized(), 1, "blaster", WeaponStats.AIMED)
	assert_lt(match_.state.player(2).health, 100)
	assert_eq(match_.state.blocks.health[0], BlockRules.HEALTH)


func test_a_burst_takes_from_every_block_it_reaches() -> void:
	await _start()
	_add(1, Vector3(0, 0, 12))
	var boxes: Array = world.block_boxes()
	var at: Vector3 = boxes[0].get_center() + Vector3(0, 0, boxes[0].size.z / 2.0 + 0.3)
	match_._burst_blocks(at, Weapons.find("launcher"))
	assert_lt(match_.state.blocks.health[0], BlockRules.HEALTH)
	var far := 0
	for index in boxes.size():
		if boxes[index].get_center().distance_to(at) > 12.0:
			assert_eq(match_.state.blocks.health[index], BlockRules.HEALTH, "out of its reach")
			far += 1
	assert_gt(far, 0)


func test_a_block_shot_to_pieces_is_gone_for_everyone_and_stops_nothing() -> void:
	await _start()
	var block: StaticBody3D = world.cheese_blocks()[0]
	var shooter := _add(1, Vector3(block.global_position.x, 0, block.global_position.z + 8))
	var origin: Vector3 = shooter.head.global_position
	var aim := (block.global_position - origin).normalized()
	assert_lt(Match.wall_caster(world.get_world_3d().direct_space_state).call(origin, origin + aim * 9.0), 9.0, "in the way")
	var standing: int = world.blocks_standing()
	match_._hurt_block(0, BlockRules.HEALTH)
	assert_false(match_.state.blocks.standing(0))
	assert_false(block.visible)
	assert_eq(block.collision_layer, 0)
	var gone := 1 + BlockRules.resting_on(world.block_boxes(), 0).size()
	assert_eq(world.blocks_standing(), standing - gone)
	await wait_physics_frames(2)
	assert_eq(Match.wall_caster(world.get_world_3d().direct_space_state).call(origin, block.global_position), INF, "a shot goes where it was")
	assert_gt(Sounds.counts.get("crumble", 0), 0)
	# A new match puts it back.
	match_._new_match()
	assert_true(block.visible)
	assert_eq(block.collision_layer, 1)
	assert_eq(world.blocks_standing(), standing)


func test_a_block_goes_with_the_one_it_stands_on() -> void:
	await _start()
	var boxes: Array = world.block_boxes()
	var under := -1
	for index in boxes.size():
		if not BlockRules.resting_on(boxes, index).is_empty():
			under = index
	if under == -1:
		pass_test("no block on this arena stands on another")
		return
	var above := BlockRules.resting_on(boxes, under)
	match_._hurt_block(under, BlockRules.HEALTH)
	for index: int in above:
		assert_false(match_.state.blocks.standing(index))
		assert_false(world.cheese_blocks()[index].visible)


func test_a_block_shows_bites_as_it_is_eaten() -> void:
	await _start()
	var block: Node3D = world.cheese_blocks()[1]
	assert_null(block.get_node_or_null("Bites"))
	var last := 0
	for stage in range(1, BlockRules.STAGES + 1):
		match_.state.blocks.health[1] = BlockRules.HEALTH - stage * BlockRules.HEALTH / (BlockRules.STAGES + 1)
		match_.state_changed.emit()
		assert_eq(match_.state.blocks.stage(1), stage)
		var bites: int = block.get_node("Bites").get_child_count()
		assert_gt(bites, last, "more at stage %d" % stage)
		last = bites
	# Darker than the cheese round them, as its holes are.
	for bite: MeshInstance3D in block.get_node("Bites").get_children():
		assert_lt((bite.material_override as StandardMaterial3D).albedo_color.v, Scenery.CHEESE.v * 0.7)
	match_._new_match()
	await wait_process_frames(2)
	assert_eq(block.get_node("Bites").get_child_count(), 0, "whole again")


func test_the_host_sends_the_mice_out_and_everyone_sees_and_hears_them() -> void:
	await _start()
	_add(1, Vector3(0, 0, 12))
	watch_signals(match_)
	match_._process(BlockRules.FIRST_GNAW + 0.1)
	assert_signal_emitted(match_, "block_gnawed")
	var index: int = get_signal_parameters(match_, "block_gnawed")[0]
	assert_eq(match_.state.blocks.health[index], BlockRules.HEALTH - BlockRules.GNAW)
	assert_eq(get_tree().get_nodes_in_group("mice").size(), 3)
	assert_gt(Sounds.counts.get("gnaw", 0), 0)
	for mouse: Node3D in get_tree().get_nodes_in_group("mice"):
		assert_eq(mouse.find_children("*", "CollisionObject3D", true, false).size(), 0, "nothing to bump into")
		for part: MeshInstance3D in mouse.find_children("*", "MeshInstance3D", true, false):
			assert_false(Scenery.is_yellow((part.material_override as StandardMaterial3D).albedo_color))
	# Three visits to the same block finish it.
	match_._hurt_block(index, BlockRules.GNAW)
	match_._hurt_block(index, BlockRules.GNAW)
	assert_false(match_.state.blocks.standing(index))


func test_the_mice_stay_in_between_matches() -> void:
	await _start()
	match_.state.end_match()
	watch_signals(match_)
	match_._process(MatchState.INTERMISSION - 1.0)
	assert_signal_not_emitted(match_, "block_gnawed")


func test_there_are_sounds_and_captions_for_it() -> void:
	const CAPTIONS := preload("res://ui/captions.gd")
	for sound: String in ["gnaw", "crumble"]:
		assert_not_null(SoundBank.get_sound(sound))
		assert_true(CAPTIONS.TEXTS.has(sound))
