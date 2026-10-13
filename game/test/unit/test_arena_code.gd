extends GutTest
## Arenas players lay out themselves: the rules of placing pieces, saving an
## arena as a code and loading it back, sharing it by link, building it,
## hosting it, and the editor that makes one.

const WORLD := preload("res://world/world.tscn")
const SAMPLE := preload("res://test/sample_arena.gd")
const EDITOR := preload("res://ui/arena_editor.gd")

var saved := {}


func before_each() -> void:
	for key in ["custom_arena", "host_map"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("custom_arena", "")


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _piece(kind: int, x: float, z: float, turn := 0, level := 0) -> Dictionary:
	return {"kind": kind, "x": x, "z": z, "turn": turn, "level": level}


# The code of whatever `bytes` hold.
func _code(bytes: Array) -> String:
	return Marshalls.raw_to_base64(PackedByteArray(bytes)).replace("+", "-").replace("/", "_").replace("=", "")


func test_a_saved_arena_loads_back_the_same() -> void:
	var arena: Dictionary = SAMPLE.arena()
	var code := ArenaCode.encode(arena)
	assert_eq(ArenaCode.decode(code), arena)
	assert_eq(ArenaCode.encode(ArenaCode.decode(code)), code)
	for size: int in ArenaCode.SIZES:
		assert_eq(ArenaCode.decode(ArenaCode.encode(ArenaCode.blank(size))), ArenaCode.blank(size))


func test_a_code_is_short_and_safe_in_a_link() -> void:
	var arena := ArenaCode.blank(56)
	for x in range(-24, 25, 3):
		for z in range(-24, 25, 3):
			ArenaCode.place(arena, _piece(ArenaCode.BLOCK, x, z))
	assert_eq(arena.pieces.size(), ArenaCode.MAX_PIECES, "as full as an arena gets")
	var code := ArenaCode.encode(arena)
	assert_lte(code.length(), ArenaCode.MAX_LENGTH)
	assert_eq(code.uri_encode(), code, "nothing a link would change")
	assert_eq(ArenaCode.decode(code), arena)


func test_what_is_not_a_code_loads_nothing() -> void:
	var good: String = SAMPLE.code()
	for junk: Variant in ["", null, 5, ["a"], "!!!", "<script>", "a b", good + "=", "A".repeat(ArenaCode.MAX_LENGTH + 1),
			"A", "AAAAA", _code([1]), _code([1, 0]), _code([ArenaCode.VERSION + 1, 0, 0]), _code([ArenaCode.VERSION, ArenaCode.SIZES.size(), 0])]:
		assert_eq(ArenaCode.decode(junk), {}, str(junk).left(30))


func test_a_code_from_anyone_only_gives_pieces_that_can_be_placed() -> void:
	var block := ArenaCode.BLOCK
	# A block, one through it, one off the floor, one in the air, a kind there
	# isn't, and a count of more pieces than follow.
	var arena := ArenaCode.decode(_code([ArenaCode.VERSION, 0, 200,
		block, 128, 128, block, 128, 128, block, 250, 128, block | (1 << 6), 133, 128, 15, 120, 120]))
	assert_eq(arena.size, 32)
	assert_eq(arena.pieces, [_piece(block, 0.0, 0.0)])
	# Cut short, it gives what it has.
	assert_eq(ArenaCode.decode(_code([ArenaCode.VERSION, 0, 3, block, 128, 128, block, 131])).pieces.size(), 1)


func test_pieces_stay_on_the_floor_and_out_of_each_other() -> void:
	var arena := ArenaCode.blank(32)
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 0, 0)))
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 1, 0)), "through the first")
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 2, 0)), "beside it")
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.HEALTH, 0, 0)), "a pad under a block")
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.CRATE, 15, 0)), "over the edge")
	assert_false(ArenaCode.place(arena, _piece(99, 5, 5)), "no such piece")
	assert_false(ArenaCode.place(arena, {"x": 5, "z": 5}))
	# A wall is long one way, and the other when turned.
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.WALL, 0, 6)))
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.HEALTH, 2.4, 6)))
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.WALL, 8, -6, 1)))
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.HEALTH, 10, -6)), "beside a wall turned end on")
	# Pieces sit on the grid.
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.AMMO, -6.4, -6.6)))
	assert_eq(Vector2(arena.pieces[-1].x, arena.pieces[-1].z), Vector2(-6, -7))


func test_blocks_stack_two_high_and_nothing_else_does() -> void:
	var arena := ArenaCode.blank()
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 0, 0, 0, 1)), "nothing to stand on")
	ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 0, 0))
	ArenaCode.place(arena, _piece(ArenaCode.CRATE, 6, 0))
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 1, 0, 0, 1)), "half on")
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 6, 0, 0, 1)), "on a crate")
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.HEALTH, 0, 0, 0, 1)), "a pad on a block")
	assert_true(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 0, 0, 0, 1)))
	assert_false(ArenaCode.place(arena, _piece(ArenaCode.BLOCK, 0, 0, 0, 1)), "a third")
	# The top one comes off first.
	assert_true(ArenaCode.remove_at(arena, 0.4, -0.4))
	assert_eq(arena.pieces.map(func(piece: Dictionary) -> int: return piece.level), [0, 0])
	assert_true(ArenaCode.remove_at(arena, 0, 0))
	assert_false(ArenaCode.remove_at(arena, 0, 0), "nothing left there")
	assert_eq(arena.pieces.size(), 1)


func test_there_are_only_so_many_of_the_strongest_pieces() -> void:
	var arena := ArenaCode.blank(56)
	for kind: int in ArenaCode.MOST:
		var went := 0
		for index in 6:
			went += int(ArenaCode.place(arena, _piece(kind, -20 + index * 3, -20 + kind * 3)))
		assert_eq(went, ArenaCode.MOST[kind], ArenaCode.NAMES[kind])


func test_every_piece_has_a_name_and_pads_hold_known_pickups() -> void:
	for kind: int in ArenaCode.KINDS:
		assert_has(ArenaCode.NAMES, kind)
		assert_true(ArenaCode.SOLID.has(kind) or ArenaCode.PICKUPS.has(kind) or kind == ArenaCode.JUMP)
		assert_lt(kind, 16, "fits its part of a code")
	for kind: int in ArenaCode.PICKUPS:
		assert_true(Pickups.is_kind(ArenaCode.PICKUPS[kind]))


func test_sixteen_start_round_the_edge_clear_of_the_pieces() -> void:
	var crowded := ArenaCode.blank(32)
	# Crates all along the edge where players would start.
	for along in range(-12, 13, 3):
		for side in [-12, 12]:
			ArenaCode.place(crowded, _piece(ArenaCode.CRATE, along, side))
			ArenaCode.place(crowded, _piece(ArenaCode.CRATE, side, along))
	for arena: Dictionary in [ArenaCode.blank(32), ArenaCode.blank(56), SAMPLE.arena(), crowded]:
		var spots := ArenaCode.spawns(arena)
		assert_eq(spots.size(), MatchSettings.MAX_PLAYERS)
		for index in spots.size():
			assert_true(ArenaCode.clear_at(arena, spots[index].x, spots[index].z, BodyShape.RADIUS), "spot %d" % index)
			assert_lt(maxf(absf(spots[index].x), absf(spots[index].z)), arena.size / 2.0 - BodyShape.RADIUS)
			for other in range(index + 1, spots.size()):
				assert_gt(spots[index].distance_to(spots[other]), BodyShape.RADIUS * 2.0)
	# On an open floor the first two face each other across the middle.
	var first := ArenaCode.spawns(ArenaCode.blank())
	assert_almost_eq(first[0] * Vector3(1, 0, 1), -first[1] * Vector3(1, 0, 1), Vector3.ONE * 0.01)


func test_a_built_arena_has_all_that_an_arena_has() -> void:
	var map := ArenaCode.build(SAMPLE.arena())
	add_child_autofree(map)
	var geometry := map.get_node("Geometry")
	var names: Array = geometry.get_children().map(func(node: Node) -> String: return String(node.name))
	assert_has(names, "Floor")
	assert_eq(names.filter(func(name: String) -> bool: return name.begins_with("Wall")).size(), 4, "only the edge is called a wall")
	assert_eq(names.filter(func(name: String) -> bool: return name.begins_with("Block")).size(), 4)
	assert_eq(names.filter(func(name: String) -> bool: return name.begins_with("CoverWall")).size(), 2)
	assert_eq(names.filter(func(name: String) -> bool: return name.begins_with("Crate")).size(), 2)
	# The block on a block is above it, and a turned wall runs the other way.
	var tops: Array = geometry.get_children().filter(func(node: Node) -> bool: return String(node.name).begins_with("Block")).map(func(node: Node3D) -> float: return node.position.y)
	tops.sort()
	assert_eq(tops, [1.0, 1.0, 1.0, 3.0])
	assert_eq((geometry.get_node("CoverWall1/CollisionShape3D").shape as BoxShape3D).size, Vector3(6, 4, 1))
	assert_eq((geometry.get_node("CoverWall2/CollisionShape3D").shape as BoxShape3D).size, Vector3(1, 4, 6))
	var held: Array = map.get_node("Pickups").get_children().map(func(pad: Node) -> String: return pad.get_meta("kind"))
	held.sort()
	assert_eq(held, ["ammo", "ammo", "armour", "health", "health", "launcher", "power"])
	assert_eq(map.get_node("SpawnPoints").get_child_count(), MatchSettings.MAX_PLAYERS)
	assert_not_null(map.get_node_or_null("MenuCamera"))
	# The jump pad, turned a quarter, throws up and to the east.
	var pad := map.get_node("JumpPads").get_child(0)
	assert_eq(pad.position, Vector3(5, 0, 9))
	var launch: Vector3 = pad.get_meta("launch")
	assert_almost_eq(launch, Vector3(ArenaCode.JUMP_ALONG, ArenaCode.JUMP_UP, 0), Vector3.ONE * 0.001)


func test_a_built_arena_has_what_the_modes_and_the_names_need() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	for arena: Dictionary in [SAMPLE.arena(), ArenaCode.blank(32)]:
		world.load_custom(ArenaCode.encode(arena))
		assert_eq(world.map_id, Maps.CUSTOM)
		var spots: Dictionary = world.objective_spots()
		assert_eq(spots.bases.size(), 2)
		assert_gt(spots.bases[0].distance_to(spots.bases[1]), 20.0)
		assert_gte(spots.hills.size(), 3)
		assert_eq(spots.points.size(), 3)
		for kind: String in spots:
			for at: Vector3 in spots[kind]:
				assert_true(ArenaCode.clear_at(arena, at.x, at.z, BodyShape.RADIUS), "%s at %s" % [kind, at])
		assert_gte(world.places().size(), 4)
		assert_eq(world.place_at(Vector3.ZERO), "The Middle")


func test_a_link_carries_the_arena_and_reads_back() -> void:
	var code: String = SAMPLE.code()
	assert_eq(ArenaCode.link("https://example.com/cheese/", code), "https://example.com/cheese/?arena=" + code)
	# Other parameters are kept, but not an older arena or a session to join.
	var link := ArenaCode.link("http://localhost:8060/?signal=ws://pc:9080&join=ABCD&arena=old#top", code)
	assert_eq(link, "http://localhost:8060/?signal=ws://pc:9080&arena=" + code)
	assert_eq(ArenaCode.code_in(link.get_slice("?", 1)), code)
	assert_eq(ArenaCode.code_in("?test&arena=" + code), code)
	for query: String in ["", "?", "?arena", "?arena=", "?arena=nonsense!", "?arenas=" + code, "?join=ABCD"]:
		assert_eq(ArenaCode.code_in(query), "", query)
	assert_eq(ArenaCode.without_code("?test&arena=%s&join=ABCD" % code), "?test&join=ABCD")
	assert_eq(ArenaCode.without_code("?arena=" + code), "")


func test_the_custom_arena_is_an_arena_but_not_one_of_the_usual_ones() -> void:
	assert_true(Maps.exists(Maps.CUSTOM))
	assert_has(Maps.NAMES, Maps.CUSTOM)
	assert_does_not_have(Maps.ORDER, Maps.CUSTOM)
	var settings := MatchSettings.from_dict({"map": Maps.CUSTOM})
	assert_eq(settings.map, Maps.CUSTOM)
	# The session list is told of an arena it knows.
	assert_has(Maps.LISTED, settings.advertised().map)
	# After a match on it, a vote offers the usual arenas and the same again.
	var ballot := Ballot.from_array(Ballot.for_next(settings).to_array())
	assert_eq(ballot.choices.size(), 3)
	assert_eq(ballot.choices[2].map, Maps.CUSTOM)
	assert_has(Maps.ORDER, ballot.choices[0].map)


func test_hosting_plays_on_the_arena_that_was_made() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	Settings.set_value("custom_arena", SAMPLE.code())
	world.start(MatchSettings.from_dict({"map": Maps.CUSTOM}))
	await wait_physics_frames(2)
	assert_eq(world.match_.state.settings.map, Maps.CUSTOM)
	assert_eq(world.map_id, Maps.CUSTOM)
	assert_eq(world.custom_code, SAMPLE.code())
	assert_eq(world.map.get_node("Geometry/Block1").position, Vector3(-8, 1, 6))
	assert_not_null(world.player_node(1), "and the host is standing in it")
	assert_eq(world.pickup_pads().size(), 7)
	world.stop()
	assert_eq(world.map_id, Maps.DEFAULT, "and it is put away afterwards")
	assert_eq(world.custom_code, "")


func test_hosting_with_no_arena_made_plays_on_the_usual_one() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	for kept: String in ["", "nonsense"]:
		Settings.set_value("custom_arena", kept)
		world.start(MatchSettings.from_dict({"map": Maps.CUSTOM}))
		assert_eq(world.match_.state.settings.map, Maps.DEFAULT)
		assert_eq(world.map_id, Maps.DEFAULT)
		world.stop()


func test_someone_joining_waits_for_the_arena_and_then_builds_it() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	# Told the match is on a custom arena, with nothing to build yet.
	world.load_map(Maps.CUSTOM)
	assert_eq(world.map_id, "box")
	# What isn't an arena is not built either.
	world.match_._custom_arena("nonsense")
	world.match_._custom_arena(7)
	assert_eq(world.map_id, "box")
	world.match_._custom_arena(SAMPLE.code())
	assert_eq(world.map_id, Maps.CUSTOM)
	assert_not_null(world.map.get_node_or_null("Geometry/Crate2"))
	# Another host's arena replaces it.
	var other := ArenaCode.blank(32)
	ArenaCode.place(other, _piece(ArenaCode.CRATE, 0, 0))
	world.match_._custom_arena(ArenaCode.encode(other))
	await wait_process_frames(1)
	assert_null(world.map.get_node_or_null("Geometry/Crate2"))
	assert_not_null(world.map.get_node_or_null("Geometry/Crate1"))


func _editor(world: Node3D) -> Control:
	var editor: Control = EDITOR.new()
	add_child_autofree(editor)
	editor.open(world)
	return editor


func test_the_editor_places_stacks_and_removes_where_it_points() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var editor := _editor(world)
	assert_true(editor.visible)
	assert_eq(world.map_id, Maps.CUSTOM, "showing the floor to build on")
	assert_false(editor.place_here(), "pointing at nothing")
	editor.aim = Vector3(4, 0, -3)
	assert_true(editor.place_here())
	assert_not_null(world.map.get_node_or_null("Geometry/Block1"), "and it is there to see")
	assert_true(editor.place_here(), "a second goes on top")
	assert_false(editor.place_here(), "and that is as high as they go")
	assert_eq(editor.arena.pieces.map(func(piece: Dictionary) -> int: return piece.level), [0, 1])
	assert_true(editor.remove_here())
	assert_true(editor.remove_here())
	assert_false(editor.remove_here())
	assert_eq(editor.code(), "", "nothing made")
	# The wheel goes through every piece and round again; the piece is turned.
	var seen := []
	for step in ArenaCode.KINDS.size():
		seen.append(editor.kind)
		editor.step_kind(1)
	assert_eq(seen, ArenaCode.KINDS)
	assert_eq(editor.kind, ArenaCode.BLOCK)
	editor.step_kind(-1)
	assert_eq(editor.kind, ArenaCode.JUMP)
	editor.kind = ArenaCode.WALL
	editor.turn_piece()
	editor.place_here()
	assert_eq((world.map.get_node("Geometry/CoverWall1/CollisionShape3D").shape as BoxShape3D).size, Vector3(1, 4, 6))
	assert_string_contains(editor.text()[0], "1 of %d" % ArenaCode.MAX_PIECES)
	assert_string_contains(editor.text()[1], "Wall")


func test_the_editor_keeps_what_was_made_and_opens_on_it_again() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var editor := _editor(world)
	watch_signals(editor)
	editor.aim = Vector3(-5, 0, 5)
	editor.kind = ArenaCode.CRATE
	editor.place_here()
	var code: String = editor.code()
	editor.close()
	assert_signal_emitted(editor, "closed")
	assert_false(editor.visible)
	assert_eq(Settings.get_value("custom_arena"), code)
	assert_eq(world.map_id, Maps.DEFAULT, "back to the menu's arena")
	assert_true(world.menu_camera.current)
	editor.open(world)
	assert_eq(editor.arena.pieces, [_piece(ArenaCode.CRATE, -5.0, 5.0)])
	# Cleared and closed, there is no arena to host.
	editor.clear()
	editor.close()
	assert_eq(Settings.get_value("custom_arena"), "")


func test_a_smaller_floor_leaves_out_what_no_longer_fits() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var editor := _editor(world)
	editor.set_floor(56)
	assert_eq(editor.arena.size, 56)
	for at: Vector3 in [Vector3(0, 0, 0), Vector3(24, 0, 24), Vector3(-10, 0, 8)]:
		editor.aim = at
		editor.place_here()
	editor.set_floor(32)
	assert_eq(editor.arena.size, 32)
	assert_eq(editor.arena.pieces.size(), 2)
	editor.set_floor(33)
	assert_eq(editor.arena.size, 32, "not a size there is")
	assert_string_contains(editor.text()[0], "32 m")


func test_the_link_the_editor_gives_makes_the_same_arena() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var editor := _editor(world)
	editor.aim = Vector3(3, 0, 3)
	editor.place_here()
	assert_eq(ArenaCode.code_in(editor.link().get_slice("?", 1)), editor.code())
	assert_eq(ArenaCode.decode(ArenaCode.code_in(editor.link().get_slice("?", 1))), editor.arena)
	editor.copy_link()
	assert_has(editor.text(), "Link copied")
	editor.place_here()
	assert_does_not_have(editor.text(), "Link copied")


func test_the_editors_buttons_are_on_the_screen() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var editor := _editor(world)
	await wait_process_frames(2)
	var screen := get_viewport().get_visible_rect()
	for name: String in ["EditorDone", "EditorCopy", "EditorClear"]:
		var button: Button = editor.find_child(name, true, false)
		assert_true(screen.encloses(button.get_global_rect()), name)
