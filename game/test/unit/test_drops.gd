extends GutTest
## Weapons left behind: a knocked-out player drops what they held, for
## anyone to take, for a while.

const WORLD := preload("res://world/world.tscn")

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3, weapon: String) -> Node3D:
	match_.add_player(id)
	var record := match_.state.player(id)
	if not record.weapons.has(weapon):
		record.weapons.append(weapon)
	record.weapon = weapon
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	match_.state_changed.emit()
	return player


func _state() -> MatchState:
	var state := MatchState.new()
	state.add_player(1)
	state.add_player(2)
	return state


func test_a_weapon_dropped_lies_there_for_a_while() -> void:
	var state := _state()
	var id := state.drop("shotgun", Vector3(1, 0, 2))
	assert_gt(id, 0)
	assert_eq(state.drops.size(), 1)
	assert_eq(state.drops[0].weapon, "shotgun")
	assert_eq(state.drops[0].at, Vector3(1, 0, 2))
	state.tick(Pickups.DROP_LIFE - 1.0)
	assert_eq(state.drops.size(), 1)
	assert_false(state.drop_ended)
	state.tick(1.5)
	assert_eq(state.drops.size(), 0)
	assert_true(state.drop_ended, "for the host to tell everyone")


func test_a_sidearm_or_anything_else_is_not_left_behind() -> void:
	var state := _state()
	for id: String in Weapons.SIDES:
		assert_eq(state.drop(id, Vector3.ZERO), 0, id)
	assert_eq(state.drop("banana", Vector3.ZERO), 0)
	assert_eq(state.drop("shotgun_both", Vector3.ZERO), 0)
	assert_eq(state.drop(Weapons.GRENADE, Vector3.ZERO), 0)
	assert_eq(state.drop("blaster", Vector3(NAN, 0, 0)), 0)
	assert_eq(state.drops.size(), 0)
	state.end_match()
	assert_eq(state.drop("blaster", Vector3.ZERO), 0, "nor between matches")


func test_only_so_many_lie_about_and_the_oldest_go_first() -> void:
	var state := _state()
	var first := state.drop("blaster", Vector3.ZERO)
	for more in Pickups.MAX_DROPS:
		state.drop("shotgun", Vector3.ZERO)
	assert_eq(state.drops.size(), Pickups.MAX_DROPS)
	assert_false(state.drops.any(func(lying: Dictionary) -> bool: return lying.id == first))


func test_taking_a_drop_gives_the_weapon_once() -> void:
	var state := _state()
	var id := state.drop("grater", Vector3.ZERO)
	assert_eq(state.take_drop(id, 1), "taken")
	assert_true(state.player(1).weapons.has("grater"))
	assert_eq(state.drops.size(), 0)
	assert_eq(state.take_drop(id, 2), "", "gone")
	assert_eq(state.take_drop(999, 2), "")


func test_a_drop_of_a_weapon_already_carried_fills_it() -> void:
	var state := _state()
	state.player(1).weapons.append("grater")
	var id := state.drop("grater", Vector3.ZERO)
	assert_eq(state.take_drop(id, 1), "refill")
	assert_eq(state.player(1).weapons.count("grater"), 1)
	assert_eq(state.drops.size(), 0)


func test_a_knocked_out_player_takes_nothing() -> void:
	var state := _state()
	var id := state.drop("grater", Vector3.ZERO)
	state.player(1).alive = false
	assert_eq(state.take_drop(id, 1), "")
	assert_eq(state.take_drop(id, 7), "", "nor someone who isn't there")
	assert_eq(state.drops.size(), 1)


func test_drops_survive_the_trip_and_nonsense_does_not() -> void:
	var state := _state()
	state.drop("rifle", Vector3(3, 1, -2))
	state.tick(5.0)
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(copy.drops.size(), 1)
	assert_eq(copy.drops[0].weapon, "rifle")
	assert_eq(copy.drops[0].at, Vector3(3, 1, -2))
	assert_almost_eq(copy.drops[0].left, Pickups.DROP_LIFE - 5.0, 0.01)
	var data := state.to_dict()
	data["drops"] = [7, "x", [1, 2], [1, 999, Vector3.ZERO, 5.0], [1, 1, Vector3(INF, 0, 0), 5.0], [1, 1, Vector3.ZERO, "soon"], [2, 2, Vector3.ZERO, 9999.0]]
	var odd := MatchState.from_dict(data)
	assert_eq(odd.drops.size(), 1)
	assert_eq(odd.drops[0].left, Pickups.DROP_LIFE, "no longer than a drop lasts")


func test_a_new_match_clears_them() -> void:
	var state := _state()
	state.drop("blaster", Vector3.ZERO)
	state.start_match()
	assert_eq(state.drops.size(), 0)


func test_a_knock_out_leaves_the_weapon_in_hand_on_the_ground_where_they_were() -> void:
	_add(1, Vector3(0, 0, 9), "blaster")
	var victim := _add(2, Vector3(3, 0, -9), "shotgun")
	victim.global_position = Vector3(3, 2.0, -9)
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", false)
	assert_eq(match_.state.drops.size(), 1)
	var lying: Dictionary = match_.state.drops[0]
	assert_eq(lying.weapon, "shotgun")
	assert_almost_eq(lying.at.x, 3.0, 0.01)
	assert_almost_eq(lying.at.z, -9.0, 0.01)
	assert_almost_eq(lying.at.y, 0.0, 0.05, "on the ground, not where they hung in the air")
	assert_eq(world.drops_shown(), 1)


func test_a_player_knocked_out_holding_their_sidearm_leaves_nothing() -> void:
	_add(1, Vector3(0, 0, 9), "blaster")
	_add(2, Vector3(0, 0, -9), "pistol")
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", false)
	assert_eq(match_.state.drops.size(), 0)


func test_walking_over_a_drop_takes_it_without_changing_what_is_in_hand() -> void:
	var own := _add(1, Vector3(0, 0, 9), "blaster")
	own.equip("blaster")
	_add(2, Vector3(5, 0, -9), "grater")
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", false)
	watch_signals(match_)
	own.global_position = Vector3(5, 0, -9)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.drops.size(), 0)
	assert_true(match_.state.player(1).weapons.has("grater"))
	assert_signal_emitted_with_parameters(match_, "picked_up", [1, "grater"])
	assert_true(own.arsenal.has("grater"))
	assert_eq(own.weapon.stats.id, "blaster", "still what they were fighting with")
	assert_eq(own.arsenal["grater"].ammo, Weapons.find("grater").magazine, "and it comes full")
	assert_eq(world.drops_shown(), 0)


func test_a_player_just_brought_back_does_not_take_what_they_left_where_they_fell() -> void:
	_add(1, Vector3(0, 0, 9), "blaster")
	var victim := _add(2, Vector3(5, 0, -9), "grater")
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", false)
	# Back in play, but still seen where they fell until they say otherwise.
	match_._bring_back(2, Vector3(-12, 0, 12))
	assert_true(match_.state.player(2).alive)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.drops.size(), 1, "it lies there still")
	# Seen where they were sent, they are themselves again: and can walk back.
	victim.global_position = Vector3(-12, 0, 12)
	match_._physics_process(1.0 / 60.0)
	victim.global_position = Vector3(5, 0, -9)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.drops.size(), 0)
	assert_true(match_.state.player(2).weapons.has("grater"))


func test_the_hosts_own_player_is_always_where_they_are_seen() -> void:
	var own := _add(1, Vector3(0, 0, 9), "blaster")
	match_.state.drop("rifle", Vector3(-12, 0, 12))
	match_._bring_back(1, Vector3(-12, 0, 12))
	assert_almost_eq(own.global_position.x, -12.0, 0.01)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.drops.size(), 0)


func test_a_weapon_found_on_a_pad_still_comes_into_hand() -> void:
	var own := _add(1, Vector3(0, 0, 9), "blaster")
	own.equip("blaster")
	match_.state.player(1).weapons.append("launcher")
	match_.state_changed.emit()
	assert_eq(own.weapon.stats.id, "launcher")


func test_a_drop_of_a_weapon_carried_refills_it_for_the_host_and_the_player() -> void:
	var own := _add(1, Vector3(0, 0, 9), "shotgun")
	own.equip("shotgun")
	own.weapon.ammo = 1
	own.weapon.reserve = 0
	var rules := match_._rules_for(1)
	rules._fired["shotgun"] = 5
	_add(2, Vector3(5, 0, -9), "shotgun")
	match_._harm(1, 2, 500.0, Vector3.ZERO, "shotgun", false)
	own.global_position = Vector3(5, 0, -9)
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.drops.size(), 0)
	assert_eq(own.weapon.ammo, own.weapon.stats.magazine)
	assert_eq(own.weapon.reserve, own.weapon.stats.reserve)
	assert_false(rules._fired.has("shotgun"))


func test_the_weapons_lying_about_are_shown_and_cleared_away() -> void:
	_add(1, Vector3(0, 0, 9), "blaster")
	match_.state.drop("rifle", Vector3(2, 0, 2))
	match_.state.drop("bow", Vector3(-2, 0, 2))
	match_.state_changed.emit()
	assert_eq(world.drops_shown(), 2)
	var shown: Node3D = world.get_node("Drops").get_child(0)
	assert_almost_eq(shown.global_position.x, 2.0, 0.01)
	assert_false(shown.get_node("Hand").visible, "nobody is holding it")
	match_._process(Pickups.DROP_LIFE + 0.1)
	await wait_process_frames(2)
	assert_eq(match_.state.drops.size(), 0)
	assert_eq(world.drops_shown(), 0)
