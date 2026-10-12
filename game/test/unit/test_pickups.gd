extends GutTest

var state: MatchState


func before_each() -> void:
	state = MatchState.new()
	state.set_pickup_count(3)
	state.add_player(1)
	state.add_player(2)


func test_a_player_on_the_pad_can_reach_it() -> void:
	assert_true(Pickups.within_reach(Vector3(5, 0, 5), Vector3(5.5, 0, 4.5)))


func test_a_player_beside_the_pad_cannot() -> void:
	assert_false(Pickups.within_reach(Vector3(5, 0, 5), Vector3(7, 0, 5)))


func test_a_player_jumping_over_the_pad_can_but_not_one_far_above() -> void:
	assert_true(Pickups.within_reach(Vector3(5, 0, 5), Vector3(5, 1.5, 5)))
	assert_false(Pickups.within_reach(Vector3(5, 0, 5), Vector3(5, 4.0, 5)))
	assert_false(Pickups.within_reach(Vector3(5, 3, 5), Vector3(5, 0, 5)), "from the floor below a raised pad")


func test_all_pads_start_full() -> void:
	for index in 3:
		assert_true(state.pickup_ready(index))
	assert_false(state.pickup_ready(3))
	assert_false(state.pickup_ready(-1))


func test_a_hurt_player_takes_a_wedge_and_heals() -> void:
	state.damage(2, 1, 70.0)
	assert_eq(state.collect(0, Pickups.HEALTH, 1), "taken")
	assert_eq(state.player(1).health, 80)
	assert_false(state.pickup_ready(0))


func test_healing_stops_at_full_health() -> void:
	state.damage(2, 1, 10.0)
	state.collect(0, Pickups.HEALTH, 1)
	assert_eq(state.player(1).health, PlayerRecord.MAX_HEALTH)


func test_a_player_at_full_health_leaves_the_wedge() -> void:
	assert_eq(state.collect(0, Pickups.HEALTH, 1), "")
	assert_true(state.pickup_ready(0))


func test_a_new_weapon_is_carried() -> void:
	assert_eq(state.collect(1, "shotgun", 1), "taken")
	assert_eq(state.player(1).weapons, [Weapons.STARTING, "shotgun"])


func test_a_weapon_already_carried_is_a_refill() -> void:
	state.collect(1, "shotgun", 1)
	state.tick(Pickups.WEAPON_RESPAWN)
	assert_eq(state.collect(1, "shotgun", 1), "refill")
	assert_eq(state.player(1).weapons, [Weapons.STARTING, "shotgun"], "not carried twice")


func test_a_collected_pickup_comes_back_after_its_time() -> void:
	state.damage(2, 1, 70.0)
	state.collect(0, Pickups.HEALTH, 1)
	state.tick(Pickups.respawn_time(Pickups.HEALTH) - 0.1)
	assert_false(state.pickup_ready(0))
	state.tick(0.1)
	assert_true(state.pickup_ready(0))


func test_weapons_take_longer_to_come_back_than_wedges() -> void:
	assert_gt(Pickups.respawn_time("launcher"), Pickups.respawn_time(Pickups.HEALTH))


func test_an_empty_pad_gives_nothing() -> void:
	state.collect(1, "shotgun", 1)
	assert_eq(state.collect(1, "shotgun", 2), "")
	assert_eq(state.player(2).weapons, [Weapons.STARTING])


func test_a_knocked_out_player_collects_nothing() -> void:
	state.damage(2, 1, 500.0)
	assert_eq(state.collect(1, "shotgun", 1), "")


func test_nothing_is_collected_between_matches() -> void:
	state.end_match()
	assert_eq(state.collect(1, "shotgun", 1), "")


func test_a_new_match_fills_every_pad() -> void:
	state.collect(1, "shotgun", 1)
	state.start_match()
	assert_true(state.pickup_ready(1))


func test_unknown_kinds_are_never_wanted() -> void:
	assert_false(Pickups.wanted("spoon", state.player(1)))
	assert_eq(state.collect(2, "spoon", 1), "")


func test_pads_survive_the_trip() -> void:
	state.collect(1, "shotgun", 1)
	var copy := MatchState.from_dict(bytes_to_var(var_to_bytes(state.to_dict())))
	assert_eq(copy.pickups.size(), 3)
	assert_false(copy.pickup_ready(1))
	assert_true(copy.pickup_ready(0))


func test_names_for_the_pickup_note() -> void:
	assert_eq(Pickups.display_name(Pickups.HEALTH), "Cheese wedge")
	assert_eq(Pickups.display_name("launcher"), "Gouda Launcher")
