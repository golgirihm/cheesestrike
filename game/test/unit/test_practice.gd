extends GutTest
## The practice range: an arena for one, with targets at marked distances,
## every weapon on a rack, and numbers for what each has done.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func _start() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.practice())
	match_ = world.match_
	await wait_physics_frames(3)


func _middle_of(id: int) -> Vector3:
	return world.player_node(id).global_position + Vector3.UP * BodyShape.CHEST.y * BodyShape.SIZE


func test_the_range_is_an_arena_but_not_one_a_host_chooses() -> void:
	assert_true(Maps.exists(Maps.RANGE))
	assert_does_not_have(Maps.ORDER, Maps.RANGE)
	assert_true(Maps.NAMES.has(Maps.RANGE))
	assert_does_not_have(MatchSettings.MODES, MatchSettings.PRACTICE)
	var settings := MatchSettings.practice()
	assert_true(settings.is_practice())
	assert_eq(settings.map, Maps.RANGE)
	assert_false(settings.is_teams())
	assert_true(MatchSettings.from_dict(settings.to_dict()).is_practice(), "it survives the trip")
	assert_false(MatchSettings.new().is_practice())


func test_the_rack_has_every_weapon_and_there_is_ammunition() -> void:
	await _start()
	var kinds: Array = world.pickup_pads().map(func(pad: Dictionary) -> String: return pad.kind)
	for id: String in Weapons.ids():
		assert_eq(kinds.count(id), 1, id)
	assert_has(kinds, Pickups.AMMO)
	assert_has(kinds, Pickups.HEALTH)
	var space := world.get_world_3d().direct_space_state
	for pad: Dictionary in world.pickup_pads():
		assert_true(Pickups.is_kind(pad.kind), pad.kind)
		assert_false(space.intersect_ray(PhysicsRayQueryParameters3D.create(pad.position + Vector3.UP, pad.position + Vector3.DOWN, 1)).is_empty(), "%s is on the floor" % pad.kind)
	assert_gte(world.spawn_positions().size(), MatchSettings.MAX_PLAYERS)


func test_targets_stand_at_marked_distances_with_a_clear_shot_at_each() -> void:
	await _start()
	var spots: Array = world.target_spots()
	assert_eq(spots.map(func(spot: Dictionary) -> int: return spot.distance), [10, 25, 50, 75, 100])
	var space := world.get_world_3d().direct_space_state
	var eye: Vector3 = world.player_node(1).head.global_position
	for index in spots.size():
		var spot: Dictionary = spots[index]
		var id := Bots.target_id(index)
		assert_almost_eq(-spot.position.z, float(spot.distance), 0.01, "as far down the range as it says")
		var node: Node3D = world.player_node(id)
		assert_not_null(node, "a target at %d m" % spot.distance)
		assert_almost_eq(node.global_position.x, spot.position.x, 0.05)
		assert_almost_eq(node.global_position.z, spot.position.z, 0.05)
		assert_eq(match_.state.name_of(id), "Target %d m" % spot.distance)
		assert_true(space.intersect_ray(PhysicsRayQueryParameters3D.create(eye, _middle_of(id), 1)).is_empty(), "nothing in the way of the one at %d m" % spot.distance)
		assert_eq(world.home_of(id), spot.position)
	var signs := get_tree().get_nodes_in_group("range_signs")
	assert_eq(signs.map(func(sign: Label3D) -> String: return sign.text), ["10 m", "25 m", "50 m", "75 m", "100 m"])
	assert_null(world.home_of(1), "the player has no place of their own")


func test_targets_are_not_bots_and_no_match_begins() -> void:
	await _start()
	assert_eq(world.bot_ids(), [])
	assert_eq(match_.state.players.size(), 6)
	assert_true(Bots.is_target(Bots.target_id(0)))
	assert_false(Bots.is_target(Bots.id_for(MatchSettings.MAX_PLAYERS)), "no bot is taken for one")
	assert_true(match_.state.is_warm_up(), "nothing to win")
	match_._process(600.0)
	assert_true(match_.state.is_playing(), "however long it is left")
	assert_eq(HUD.match_text(match_.state), "Practice range")
	assert_string_contains(match_.state.objective_text(1), "rack")
	match_._new_match()
	assert_true(match_.state.is_warm_up())


func test_a_target_only_stands_there() -> void:
	await _start()
	var target: CharacterBody3D = world.player_node(Bots.target_id(0))
	var from := target.global_position
	match_.state.player(1).protected_for = 0.0
	watch_signals(match_)
	await wait_physics_frames(120)
	assert_almost_eq(target.global_position, from, Vector3.ONE * 0.01)
	assert_eq(match_.state.player(1).health, PlayerRecord.MAX_HEALTH)
	assert_signal_not_emitted(match_, "shot_shown")
	assert_true(target.driven)
	assert_false(target._mine)


func test_hitting_a_target_is_like_hitting_anyone_and_is_counted() -> void:
	await _start()
	var own: Node3D = world.player_node(1)
	var id := Bots.target_id(1)
	match_.state.player(id).protected_for = 0.0
	var origin: Vector3 = own.head.global_position
	watch_signals(match_)
	# Low on the body, under the face.
	var chest: Vector3 = world.player_node(id).global_position + Vector3.UP * BodyShape.CHEST.y * BodyShape.SIZE
	match_._shot(1, origin, (chest - origin).normalized(), 1, "blaster", WeaponStats.AIMED)
	assert_eq(match_.state.player(id).health, PlayerRecord.MAX_HEALTH - 20)
	assert_signal_emitted(match_, "hit_confirmed")
	match_._clock += 1.0
	match_._shot(1, origin, Vector3.UP, 1, "blaster", WeaponStats.AIMED)
	assert_eq(match_.tally.practice_lines(), ["Cheddar Blaster: 2 fired · 50% hit · 20 damage (20 a hit)"] as Array[String])


func test_a_target_knocked_out_comes_back_to_its_own_place() -> void:
	await _start()
	var id := Bots.target_id(2)
	var home: Vector3 = world.home_of(id)
	match_.state.player(id).protected_for = 0.0
	match_.state.damage(1, id, 500.0)
	assert_false(match_.state.player(id).alive)
	match_._process(MatchState.RESPAWN_DELAY + 0.1)
	assert_true(match_.state.player(id).alive)
	assert_almost_eq(world.player_node(id).global_position, home, Vector3.ONE * 0.05)
	assert_eq(match_.state.player(1).kills, 1)
	assert_true(match_.state.is_playing(), "and no number of them wins anything")


func test_a_weapon_taken_from_the_rack_comes_into_hand_and_is_back_at_once() -> void:
	await _start()
	var own: Node3D = world.player_node(1)
	var pads: Array = world.pickup_pads()
	var index := -1
	for at in pads.size():
		if pads[at].kind == "rifle":
			index = at
	own.global_position = pads[index].position
	match_._physics_process(1.0 / 60.0)
	assert_true(match_.state.player(1).weapons.has("rifle"))
	assert_eq(match_.state.player(1).weapon, "rifle")
	assert_eq(own.weapon.stats.id, "rifle", "in hand, to try")
	assert_lte(match_.state.pickups[index], 1.0, "back in a moment")
	assert_gt(Pickups.respawn_time("rifle"), 1.0, "unlike in a match")


func test_what_each_weapon_has_done() -> void:
	var tally := WeaponTally.new()
	assert_eq(tally.practice_lines(), [] as Array[String])
	for shot in 4:
		tally.shot("rifle")
	tally.hit("rifle", 75, false)
	tally.hit("rifle", 150, true)
	tally.shot("pistol")
	tally.hit("grater", 12, false)
	assert_eq(tally.practice_lines(), ["Roquefort Rifle: 4 fired · 50% hit · 225 damage (113 a hit)", "Curd Pistol: 1 fired · 0% hit · 0 damage (0 a hit)"] as Array[String])
	# A shotgun's pellets can all hit: never more than every shot.
	tally.shot("shotgun")
	for pellet in 9:
		tally.hit("shotgun", 10, false)
	assert_string_contains(tally.practice_lines()[-1], "100% hit")


func test_the_hud_shows_the_numbers_on_the_range_only() -> void:
	await _start()
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	hud._process(0.1)
	assert_eq(hud.practice_shown(), [])
	match_.tally.shot("bow")
	hud._process(0.1)
	assert_eq(hud.practice_shown(), ["Breadstick Bow: 1 fired · 0% hit · 0 damage (0 a hit)"])
	match_.state.settings.mode = MatchSettings.FREE_FOR_ALL
	hud._process(0.1)
	assert_eq(hud.practice_shown(), [])


func test_an_ordinary_arena_has_no_targets() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box"}))
	assert_eq(world.target_spots(), [])
	assert_eq(world.match_.state.players.size(), 1)
	assert_eq(get_tree().get_nodes_in_group("range_signs").size(), 0)


func test_the_menu_starts_a_session_alone_on_the_range() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	var button: Button = main.find_child("PracticeButton", true, false)
	assert_not_null(button)
	watch_signals(Net)
	button.pressed.emit()
	assert_signal_emitted(Net, "session_started")
	assert_true(Net.alone)
	assert_true(Net.in_session)
	assert_eq(main.world.map_id, Maps.RANGE)
	assert_true(main.world.match_.state.settings.is_practice())
	assert_false(main.menu.visible)
	assert_true(main.combat_hud.visible)
	main._process(0.0)
	assert_eq(main.hud.text, "Practice range")
	assert_false(main.copy_link_button.visible, "nobody to invite")
	# Leaving goes back to the menu, and the next session is an ordinary one.
	Net.leave()
	assert_false(Net.alone)
	assert_false(Net.in_session)
	assert_true(main.menu.visible)
	assert_false(main.host_settings().is_practice())
	multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
