extends GutTest
## Power-ups: what each does, how long it lasts, and how they are handed out.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func before_each() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box"}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


func _power_pad() -> int:
	var pads: Array = world.pickup_pads()
	for index in pads.size():
		if pads[index].kind == Pickups.POWER:
			return index
	return -1


func _state_with(ids: Array) -> MatchState:
	var state := MatchState.new()
	state.set_pickup_count(1)
	for id: int in ids:
		state.add_player(id)
	return state


func test_they_come_round_in_turn() -> void:
	var seen := []
	for taken in Powers.KINDS.size():
		seen.append(Powers.next(taken))
	assert_eq(seen, Powers.KINDS)
	assert_eq(Powers.next(Powers.KINDS.size()), Powers.KINDS[0])
	for kind: int in Powers.KINDS:
		assert_true(Powers.NAMES.has(kind))
		assert_gt(Powers.DURATION[kind], 5.0)
		assert_true(Powers.COLOURS.has(kind))
		assert_false(Scenery.is_yellow(Powers.COLOURS[kind]), "yellow is for cheese")
	assert_false(Powers.is_kind(Powers.NONE))
	assert_false(Powers.is_kind(99))


func test_double_damage_doubles_what_its_holder_deals() -> void:
	var state := _state_with([1, 2])
	state.player(1).give_power(Powers.DAMAGE, 10.0)
	assert_eq(state.damage(1, 2, 20.0).dealt, 40)
	assert_eq(state.damage(2, 1, 20.0).dealt, 20, "not what they take")


func test_a_shield_takes_most_of_a_hit() -> void:
	var state := _state_with([1, 2])
	state.player(2).give_power(Powers.SHIELD, 10.0)
	assert_eq(state.damage(1, 2, 50.0).dealt, roundi(50.0 * Powers.SHIELD_FACTOR))
	assert_eq(state.damage(2, 2, 50.0, true).dealt, roundi(50.0 * Powers.SHIELD_FACTOR), "and of their own bursts")
	state.player(1).give_power(Powers.DAMAGE, 10.0)
	assert_eq(state.damage(1, 2, 10.0).dealt, roundi(10.0 * Powers.DAMAGE_FACTOR * Powers.SHIELD_FACTOR), "against double damage, both count")


func test_speed_quickens_its_holder_and_does_not_add_to_an_overload() -> void:
	var record := PlayerRecord.new(1)
	assert_eq(Overload.speed_factor(record), 1.0)
	record.give_power(Powers.SPEED, 10.0)
	assert_eq(Overload.speed_factor(record), Powers.SPEED_FACTOR)
	record.overload_left = 3.0
	assert_eq(Overload.speed_factor(record), maxf(Powers.SPEED_FACTOR, Overload.SPEED_BOOST))
	# The host's check on how fast players move already allows for it.
	var fastest := Movement.WALK_SPEED * Movement.RUN_FACTOR * Movement.SLIDE_BOOST * maxf(Powers.SPEED_FACTOR, Overload.SPEED_BOOST)
	assert_lte(fastest, MovementCheck.MAX_SPEED)


func test_a_power_up_runs_out() -> void:
	var state := _state_with([1])
	state.player(1).give_power(Powers.DAMAGE, 2.0)
	state.tick(1.5)
	assert_true(Powers.has(state.player(1), Powers.DAMAGE))
	assert_false(state.power_ended)
	state.tick(1.0)
	assert_false(Powers.has(state.player(1), Powers.DAMAGE))
	assert_eq(state.player(1).power, Powers.NONE)
	assert_true(state.power_ended, "for the host to tell everyone")


func test_a_knock_out_takes_it_away() -> void:
	var state := _state_with([1, 2])
	state.player(2).give_power(Powers.SPEED, 10.0)
	state.damage(1, 2, 500.0)
	assert_eq(state.player(2).power, Powers.NONE)
	assert_eq(state.player(2).power_left, 0.0)
	state.player(1).give_power(Powers.SIGHT, 10.0)
	state.start_match()
	assert_eq(state.player(1).power, Powers.NONE, "and so does a new match")


func test_a_power_up_survives_the_trip_and_nonsense_does_not() -> void:
	var state := _state_with([1])
	state.player(1).give_power(Powers.SHIELD, 7.5)
	var copy := MatchState.from_dict(state.to_dict()).player(1)
	assert_eq(copy.power, Powers.SHIELD)
	assert_almost_eq(copy.power_left, 7.5, 0.01)
	var record := PlayerRecord.new(1)
	record.give_power(99, 5.0)
	assert_eq(record.power, Powers.NONE)
	record.give_power(Powers.SPEED, -1.0)
	assert_eq(record.power, Powers.NONE)
	record.give_power(Powers.SPEED, 9999.0)
	assert_lte(record.power_left, 60.0)
	record.give_power(Powers.SPEED, NAN)
	assert_eq(record.power, Powers.NONE)


func test_what_a_player_is_shown_of_their_own() -> void:
	var record := PlayerRecord.new(1)
	assert_eq(Powers.text(record), "")
	record.give_power(Powers.DAMAGE, 8.2)
	assert_eq(Powers.text(record), "Double damage 9")
	record.alive = false
	assert_eq(Powers.text(record), "")
	assert_eq(Powers.text(null), "")
	assert_eq(Powers.announcement("Hunter", Powers.SHIELD), "Hunter has SHIELD!")
	assert_eq(Powers.announcement("Hunter", 0), "")


func test_each_arena_has_one_power_pad_that_is_empty_to_begin_with() -> void:
	for id: String in Maps.ORDER:
		world.load_map(id)
		match_.start(MatchSettings.from_dict({"map": id}))
		var kinds: Array = world.pickup_pads().map(func(pad: Dictionary) -> String: return pad.kind)
		assert_eq(kinds.count(Pickups.POWER), 1, id)
		var index := _power_pad()
		assert_false(match_.state.pickup_ready(index), id)
		assert_almost_eq(match_.state.pickups[index], Pickups.POWER_FIRST, 0.001, id)
		for other in kinds.size():
			if other != index:
				assert_true(match_.state.pickup_ready(other), "%s: the rest are there from the start" % id)
	assert_gt(Pickups.respawn_time(Pickups.POWER), Pickups.respawn_time(Pickups.ARMOUR) * 2.0, "a long timer")


func test_a_new_match_empties_the_pad_again() -> void:
	var index := _power_pad()
	match_.state.pickups[index] = 0.0
	match_._new_match()
	assert_almost_eq(match_.state.pickups[index], Pickups.POWER_FIRST, 0.001)


func test_taking_one_is_announced_to_everyone_and_the_next_is_different() -> void:
	var index := _power_pad()
	var pad: Dictionary = world.pickup_pads()[index]
	var own := _add(1, Vector3(0, 0, 10))
	_add(2, Vector3(0, 0, -10))
	match_.state.name_of(1)
	watch_signals(match_)
	own.global_position = pad.position
	match_._physics_process(1.0 / 60.0)
	assert_true(match_.state.pickup_ready(index) == false)
	assert_eq(match_.state.player(1).power, Powers.NONE, "not until it is there")
	match_.state.pickups[index] = 0.0
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.player(1).power, Powers.KINDS[0])
	assert_almost_eq(match_.state.player(1).power_left, Powers.DURATION[Powers.KINDS[0]], 0.01)
	assert_signal_emitted_with_parameters(match_, "power_taken", [1, Powers.KINDS[0]])
	assert_signal_emitted_with_parameters(match_, "picked_up", [1, Pickups.POWER])
	assert_almost_eq(match_.state.pickups[index], Pickups.respawn_time(Pickups.POWER), 0.001)
	assert_eq(Sounds.counts.get("power", 0) > 0, true, "heard")
	# Whoever takes the next gets the next kind, even holding one already.
	match_.state.pickups[index] = 0.0
	match_._physics_process(1.0 / 60.0)
	assert_eq(match_.state.player(1).power, Powers.KINDS[1])


func test_the_hud_announces_it_and_shows_its_holder_what_is_left() -> void:
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	_add(1, Vector3(0, 0, 10))
	match_.state.player(1).name = "Hunter"
	assert_eq(hud.announcement_shown(), "")
	match_.state.player(1).give_power(Powers.DAMAGE, 12.0)
	match_.power_taken.emit(1, Powers.DAMAGE)
	hud._process(0.1)
	assert_eq(hud.announcement_shown(), "Hunter has DOUBLE DAMAGE!")
	assert_eq(hud.power_shown(), "Double damage 12")
	hud._process(HUD.ANNOUNCE_LIFE)
	assert_eq(hud.announcement_shown(), "", "for a few seconds")
	match_.state.player(1).give_power(Powers.NONE, 0.0)
	hud._process(0.1)
	assert_eq(hud.power_shown(), "")


func test_others_see_a_ring_over_a_holder_in_its_colour() -> void:
	_add(1, Vector3(0, 0, 10))
	var other := _add(2, Vector3(0, 0, -10))
	match_.state_changed.emit()
	assert_false(other._halo.visible)
	match_.state.player(2).give_power(Powers.SHIELD, 10.0)
	match_.state_changed.emit()
	assert_true(other._halo.visible)
	assert_eq((other._halo.material_override as StandardMaterial3D).albedo_color, Powers.COLOURS[Powers.SHIELD])
	match_.state.player(2).give_power(Powers.NONE, 0.0)
	match_.state_changed.emit()
	assert_false(other._halo.visible)


func test_sight_marks_everyone_else_through_walls_for_its_holder_only() -> void:
	var own := _add(1, Vector3(0, 0, 10))
	var other := _add(2, Vector3(0, 0, -10))
	match_.state_changed.emit()
	assert_false(other._seen_mark.visible)
	match_.state.player(1).give_power(Powers.SIGHT, 10.0)
	match_.state_changed.emit()
	assert_true(other._seen_mark.visible)
	assert_true((other._seen_mark.material_override as StandardMaterial3D).no_depth_test, "through anything")
	assert_false(own._seen_mark.visible, "not themselves")
	# Someone knocked out isn't marked.
	match_.state.damage(1, 2, 500.0)
	match_.state_changed.emit()
	assert_false(other._seen_mark.visible)
	match_.state.respawn(2)
	match_.state.player(1).give_power(Powers.NONE, 0.0)
	match_.state_changed.emit()
	assert_false(other._seen_mark.visible, "and it ends with the power-up")


func test_the_pad_holds_something_to_look_at() -> void:
	var model := PickupModels.build(Pickups.POWER)
	assert_gt(model.find_children("*", "MeshInstance3D", true, false).size(), 0)
	model.free()
	assert_eq(Pickups.display_name(Pickups.POWER), "Power-up")
	assert_true(Pickups.is_kind(Pickups.POWER))
	assert_false(Pickups.is_kind("shotgun_both"), "another way of firing is not a thing to find")
	assert_not_null(SoundBank.get_sound("power"))
