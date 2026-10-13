extends GutTest

const WORLD := preload("res://world/world.tscn")

var tally: WeaponTally


func before_each() -> void:
	tally = WeaponTally.new()


func test_a_weapon_that_was_never_used_has_done_nothing() -> void:
	assert_eq(tally.of("blaster"), {"shots": 0, "hits": 0, "damage": 0, "knock_outs": 0})
	assert_eq(tally.accuracy("blaster"), 0.0)
	assert_eq(tally.lines(), [] as Array[String])


func test_shots_hits_damage_and_knock_outs_are_counted_for_each_weapon() -> void:
	for shot in 4:
		tally.shot("blaster")
	tally.hit("blaster", 20, false)
	tally.hit("blaster", 15, true)
	tally.shot("shotgun")
	assert_eq(tally.of("blaster"), {"shots": 4, "hits": 2, "damage": 35, "knock_outs": 1})
	assert_eq(tally.of("shotgun"), {"shots": 1, "hits": 0, "damage": 0, "knock_outs": 0})
	assert_almost_eq(tally.accuracy("blaster"), 0.5, 0.0001)


func test_the_lines_put_the_weapon_that_did_most_first() -> void:
	tally.shot("pistol")
	tally.hit("pistol", 12, false)
	tally.shot("launcher")
	tally.hit("launcher", 100, true)
	tally.shot(Melee.ID)
	tally.hit(Melee.ID, 55, false)
	assert_eq(tally.lines(), [
		"Gouda Launcher: 1 fired, 1 hit, 100 damage, 1 knocked out",
		"Cheese Knife: 1 fired, 1 hit, 55 damage, 0 knocked out",
		"Curd Pistol: 1 fired, 1 hit, 12 damage, 0 knocked out",
	] as Array[String])


func test_clearing_starts_again() -> void:
	tally.shot("blaster")
	tally.clear()
	assert_eq(tally.lines(), [] as Array[String])


func test_the_host_counts_what_each_weapon_does_and_starts_again_each_match() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	var match_: Match = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)
	for id in [1, 2]:
		match_.add_player(id)
		var player: Node3D = world._create_player([id, Vector3.ZERO])
		world.players.add_child(player)
		player.global_position = Vector3(0, 0, 5 if id == 1 else 2)
	var shooter: Node3D = world.player_node(1)
	var origin: Vector3 = shooter.head.global_position
	match_._shot(1, origin, (Vector3(0, 2.0, 2) - origin).normalized(), 1)
	match_._shot(1, origin, Vector3.UP, 1)
	match_._strike(1, origin, Vector3.FORWARD)
	match_._throw(1, origin, Vector3.RIGHT, 0.0)
	var fired := match_.tally.of(match_.state.player(1).weapon)
	assert_eq(fired.shots, 2)
	assert_eq(fired.hits, 1)
	assert_gt(fired.damage, 0)
	assert_eq(match_.tally.of(Melee.ID).shots, 1)
	assert_eq(match_.tally.of(Melee.ID).hits, 1)
	assert_eq(match_.tally.of(Weapons.GRENADE).shots, 1)
	match_.state.end_match()
	match_._process(MatchState.INTERMISSION + 0.01)
	assert_eq(match_.tally.lines(), [] as Array[String], "a new match starts the count again")


func test_what_a_player_does_to_themself_is_not_a_hit() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var match_: Match = world.match_
	match_.start()
	match_.add_player(1)
	match_._harm(1, 1, 40.0, Vector3.ZERO, "launcher", false)
	assert_eq(match_.tally.of("launcher").hits, 0)
