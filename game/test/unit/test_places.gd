extends GutTest
## The names of the parts of each arena.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")
const FEED := preload("res://ui/kill_feed.gd")

var world: Node3D
var match_: Match


func _start(map := "canyon") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": map}))
	match_ = world.match_
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


func test_the_nearest_place_names_a_spot_and_height_counts_double() -> void:
	var named := [{"name": "Low", "at": Vector3.ZERO}, {"name": "High", "at": Vector3(0, 4, 0)}, {"name": "Far", "at": Vector3(30, 0, 0)}]
	assert_eq(world_script().nearest_place(named, Vector3(1, 0, 1)), "Low")
	assert_eq(world_script().nearest_place(named, Vector3(1, 4, 1)), "High")
	assert_eq(world_script().nearest_place(named, Vector3(5, 0, 0)), "Low", "under the high place, not on it")
	assert_eq(world_script().nearest_place(named, Vector3(25, 3, 0)), "Far")
	assert_eq(world_script().nearest_place([], Vector3.ZERO), "", "an arena with no names")


func world_script() -> GDScript:
	return preload("res://world/world.gd")


func test_every_part_of_every_arena_has_a_name() -> void:
	await _start()
	for id: String in Maps.ORDER + [Maps.RANGE]:
		world.load_map(id)
		await wait_physics_frames(1)
		var named: Array = world.places()
		assert_gte(named.size(), 4, id)
		var seen := {}
		for place: Dictionary in named:
			assert_false(place.name.is_empty(), id)
			assert_false(seen.has(place.name), "%s: two places called %s" % [id, place.name])
			seen[place.name] = true
		# Everywhere a player starts, picks something up or fights over is
		# in some named part, and not far from the middle of it.
		var spots: Array = world.spawn_positions() + world.pickup_pads().map(func(pad: Dictionary) -> Vector3: return pad.position)
		for kind: String in world.objective_spots():
			spots += world.objective_spots()[kind]
		var used := {}
		for spot: Vector3 in spots:
			var place: String = world.place_at(spot)
			assert_false(place.is_empty(), "%s at %s" % [id, spot])
			used[place] = true
			var at: Vector3 = named.filter(func(found: Dictionary) -> bool: return found.name == place)[0].at
			assert_lt(spot.distance_to(at), 30.0 if id == Maps.RANGE else 18.0, "%s: %s is a long way from %s" % [id, spot, place])
		# On the range everything a player starts by is at one end of it.
		assert_gte(used.size(), 1 if id == Maps.RANGE else 4, "%s: its names are spread over it" % id)


func test_the_hud_says_where_the_player_is() -> void:
	await _start()
	var own: Node3D = world.player_node(1)
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	own.global_position = Vector3(0, 0, 0)
	hud._process(0.1)
	assert_eq(hud.place_shown(), "The Gulch")
	own.global_position = Vector3(0, 3, -18)
	hud._process(0.1)
	assert_eq(hud.place_shown(), "North Mesa")
	match_.state.player(1).alive = false
	hud._process(0.1)
	assert_eq(hud.place_shown(), "", "nowhere, while out of play")


func test_the_feed_says_where_a_knock_out_happened() -> void:
	assert_eq(FEED.entry_text("Hunter", "Target", "blaster", false, "North Mesa"), "Hunter  [Cheddar Blaster]  Target  ·  North Mesa")
	assert_eq(FEED.entry_text("Hunter", "Target", "blaster", true), "Hunter  [Cheddar Blaster, head]  Target", "or nothing, if it isn't known")
	assert_eq(FEED.entry_text("", "Target", "launcher", false, "The Gulch"), "Target  knocked themself out  [Gouda Launcher]  ·  The Gulch")
	await _start()
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	_add(5, Vector3(0, 3, 18))
	match_.state.rename(1, "Hunter")
	match_.state.rename(5, "Target")
	match_.state.player(5).protected_for = 0.0
	match_._harm(1, 5, 500.0, Vector3.ZERO, "blaster", false)
	assert_eq(hud.kill_feed.latest, "Hunter  [Cheddar Blaster]  Target  ·  South Mesa")
	# The next is where it happened, not where the last one did.
	_add(6, Vector3(0, 0, 0))
	match_.state.rename(6, "Other")
	match_.state.player(6).protected_for = 0.0
	match_._harm(1, 6, 500.0, Vector3.ZERO, "blaster", false)
	assert_string_ends_with(hud.kill_feed.latest, "Other  ·  The Gulch")
