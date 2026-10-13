extends GutTest
## The compass, and the pings players put down for their teammates: what
## each shows and hides.

const WORLD := preload("res://world/world.tscn")
const CHEDDAR := MatchState.CHEDDAR
const BRIE := MatchState.BRIE

var world: Node3D


func _state(mode: String, teams_of := {}) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": mode}))
	for id: int in [1, 2, 3, 4]:
		state.add_player(id)
		state.player(id).team = teams_of.get(id, MatchState.NO_TEAM)
	state.start_match()
	return state


func _kinds(marks: Array) -> Array:
	var kinds := marks.map(func(mark: Dictionary) -> String: return mark.kind)
	kinds.sort()
	return kinds


func _start(mode: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)


func _add(id: int, team: int, at: Vector3) -> Node3D:
	world.match_.add_player(id)
	world.match_.state.player(id).team = team
	var player: Node3D = world._create_player([id, at])
	world.players.add_child(player)
	player.global_position = at
	return player


func test_which_way_something_is_from_where_a_player_faces() -> void:
	# Facing down -z (yaw 0): ahead is 0, to the right positive.
	assert_almost_eq(Compass.bearing(Vector3.ZERO, 0.0, Vector3(0, 0, -10)), 0.0, 0.001)
	assert_almost_eq(Compass.bearing(Vector3.ZERO, 0.0, Vector3(10, 0, 0)), PI / 2.0, 0.001)
	assert_almost_eq(Compass.bearing(Vector3.ZERO, 0.0, Vector3(-10, 0, 0)), -PI / 2.0, 0.001)
	assert_almost_eq(absf(Compass.bearing(Vector3.ZERO, 0.0, Vector3(0, 0, 10))), PI, 0.001)
	# Height makes no difference.
	assert_almost_eq(Compass.bearing(Vector3.ZERO, 0.0, Vector3(10, 50, -10)), PI / 4.0, 0.001)


func test_what_is_ahead_is_in_the_middle_and_what_is_behind_at_the_ends() -> void:
	assert_eq(Compass.along(0.0), 0.0)
	assert_gt(Compass.along(0.5), 0.0)
	assert_lt(Compass.along(-0.5), 0.0)
	assert_almost_eq(Compass.along(Compass.SPAN / 2.0), 1.0, 0.001)
	assert_eq(Compass.along(PI * 0.9), 1.0, "behind to the right: held at the right end")
	assert_eq(Compass.along(-PI * 0.9), -1.0)
	assert_false(Compass.off_the_end(0.3))
	assert_true(Compass.off_the_end(PI * 0.9))
	assert_almost_eq(Compass.along(TAU + 0.2), Compass.along(0.2), 0.001, "however many times round")


func test_teammates_are_on_it_and_opponents_never_are() -> void:
	var state := _state(MatchSettings.TEAMS, {1: CHEDDAR, 2: CHEDDAR, 3: BRIE, 4: BRIE})
	var positions := {1: Vector3.ZERO, 2: Vector3(5, 0, 0), 3: Vector3(0, 0, -8), 4: Vector3(-3, 0, 2)}
	var marks := Compass.marks(state, 1, positions, [], [], [])
	assert_eq(marks.size(), 1)
	assert_eq(marks[0].kind, Compass.TEAMMATE)
	assert_eq(marks[0].at, Vector3(5, 0, 0))
	assert_eq(marks[0].colour, TeamColors.of(CHEDDAR))
	# From the other side, the other two.
	assert_eq(Compass.marks(state, 3, positions, [], [], []).map(func(mark: Dictionary) -> Vector3: return mark.at), [Vector3(-3, 0, 2)])
	# Not one who is knocked out, or only watching.
	state.player(2).alive = false
	assert_eq(Compass.marks(state, 1, positions, [], [], []), [])
	state.player(2).alive = true
	state.player(2).watching = true
	assert_eq(Compass.marks(state, 1, positions, [], [], []), [])


func test_in_a_free_for_all_nobody_is_on_it() -> void:
	var state := _state(MatchSettings.FREE_FOR_ALL)
	var positions := {1: Vector3.ZERO, 2: Vector3(5, 0, 0), 3: Vector3(0, 0, -8)}
	assert_eq(Compass.marks(state, 1, positions, [], [], []), [])


func test_objectives_and_pings_given_to_it_are_shown() -> void:
	var state := _state(MatchSettings.FREE_FOR_ALL)
	var marks := Compass.marks(state, 1, {1: Vector3.ZERO}, [{"at": Vector3(9, 0, 9), "colour": Color.RED}], [{"at": Vector3(1, 0, 1), "colour": Color.GREEN}], [])
	assert_eq(_kinds(marks), [Compass.OBJECTIVE, Compass.PING])
	assert_eq(marks[0].colour, Color.RED)
	# Nothing for someone who isn't in the match.
	assert_eq(Compass.marks(state, 99, {}, [{"at": Vector3.ZERO}], [], []), [])
	assert_eq(Compass.marks(null, 1, {}, [], [], []), [])


func test_an_opponents_gunfire_is_shown_only_from_as_far_as_it_is_heard() -> void:
	var state := _state(MatchSettings.TEAMS, {1: CHEDDAR, 2: CHEDDAR, 3: BRIE})
	var positions := {1: Vector3.ZERO, 2: Vector3(5, 0, 0), 3: Vector3(0, 0, -30)}
	var near := [{"at": Vector3(0, 0, -30), "by": 3}]
	assert_eq(_kinds(Compass.marks(state, 1, positions, [], [], near)), [Compass.SHOT, Compass.TEAMMATE])
	var far := [{"at": Vector3(0, 0, -(Compass.HEARING + 5.0)), "by": 3}]
	assert_eq(_kinds(Compass.marks(state, 1, positions, [], [], far)), [Compass.TEAMMATE], "too far to hear")
	# One's own shots, and a teammate's, are not marked.
	assert_eq(_kinds(Compass.marks(state, 1, positions, [], [], [{"at": Vector3.ZERO, "by": 1}, {"at": Vector3(5, 0, 0), "by": 2}])), [Compass.TEAMMATE])
	# Without teams everyone else's are.
	var alone := _state(MatchSettings.FREE_FOR_ALL)
	assert_eq(_kinds(Compass.marks(alone, 1, positions, [], [], [{"at": Vector3(5, 0, 0), "by": 2}])), [Compass.SHOT])


func test_a_shot_is_remembered_for_a_moment_one_for_each_shooter() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var compass := Compass.new(world)
	add_child_autofree(compass)
	_add(5, MatchState.NO_TEAM, Vector3(6, 0.1, 0))
	compass.hear_shot(5, Vector3(6, 0, 0))
	compass.hear_shot(5, Vector3(7, 0, 0))
	assert_eq(_kinds(compass.shown()), [Compass.SHOT], "moved, not doubled")
	assert_eq(compass.shown()[0].at, Vector3(7, 0, 0))
	compass._process(Compass.SHOT_LIFE + 0.1)
	assert_eq(compass.shown(), [], "and gone")
	# The match's own shots reach it.
	world.match_.shot_shown.emit(5, [])
	assert_eq(_kinds(compass.shown()), [Compass.SHOT])
	for shooter in 40:
		compass.hear_shot(100 + shooter, Vector3.ZERO)
	assert_lte(compass._shots.size(), Compass.MAX_SHOTS)


func test_the_modes_objectives_are_on_the_compass() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var compass := Compass.new(world)
	add_child_autofree(compass)
	assert_eq(compass.shown(), [], "none in a free-for-all")
	world.stop()
	world.start(MatchSettings.from_dict({"map": "box", "mode": MatchSettings.HILL}))
	await wait_physics_frames(3)
	world.objective_marks.refresh()
	assert_has(_kinds(compass.shown()), Compass.OBJECTIVE)
	assert_eq(world.objective_marks.shown().size(), 1, "the one hill that is live")


func test_what_a_ping_is_a_ping_on() -> void:
	var pads := [Vector3(4, 0, 4), Vector3(-4, 0, 4)]
	assert_eq(Pings.kind_at(Vector3(20, 0, 0), false, pads), Pings.PLACE)
	assert_eq(Pings.kind_at(Vector3(4.5, 0, 4.5), false, pads), Pings.PICKUP)
	assert_eq(Pings.kind_at(Vector3(4, 0, 4 + Pings.PICKUP_WITHIN + 0.1), false, pads), Pings.PLACE)
	assert_eq(Pings.kind_at(Vector3(4, 0, 4), true, pads), Pings.ENEMY, "an opponent, even standing on a pad")
	for kind: String in Pings.KINDS:
		assert_has(Pings.COLOURS, kind)
		assert_has(Pings.WORDS, kind)


func test_a_teammate_sees_a_ping_and_an_opponent_does_not() -> void:
	var state := _state(MatchSettings.TEAMS, {1: CHEDDAR, 2: CHEDDAR, 3: BRIE})
	assert_true(Pings.seen_by(state.player(1), state.player(2), true))
	assert_false(Pings.seen_by(state.player(1), state.player(3), true))
	assert_true(Pings.seen_by(state.player(1), state.player(1), true), "and the pinger")
	assert_false(Pings.seen_by(null, state.player(1), true))
	# Without teams a ping is the pinger's own.
	var alone := _state(MatchSettings.FREE_FOR_ALL)
	assert_true(Pings.seen_by(alone.player(1), alone.player(1), false))
	assert_false(Pings.seen_by(alone.player(1), alone.player(2), false))


func test_what_is_not_a_ping_is_known() -> void:
	assert_true(Pings.is_ping(Vector3(3, 0, 3), Pings.PLACE))
	assert_false(Pings.is_ping(Vector3(3, 0, 3), "treasure"))
	assert_false(Pings.is_ping("here", Pings.PLACE))
	assert_false(Pings.is_ping(Vector3(NAN, 0, 0), Pings.PLACE))
	assert_false(Pings.is_ping(Vector3(INF, 0, 0), Pings.PLACE))
	assert_false(Pings.is_ping(Vector3(Pings.BOUNDS + 1.0, 0, 0), Pings.PLACE), "nowhere near the arena")
	assert_false(Pings.is_ping(Vector3.ZERO, 7))
	assert_false(Pings.may_ping(0.2))
	assert_true(Pings.may_ping(Pings.GAP))


func test_the_host_passes_a_ping_to_who_should_see_it() -> void:
	await _start(MatchSettings.TEAMS)
	var match_: Match = world.match_
	var own_team: int = match_.own_record().team
	var other_team := BRIE if own_team == CHEDDAR else CHEDDAR
	_add(5, own_team, Vector3(4, 0.1, 0))
	_add(6, other_team, Vector3(-4, 0.1, 0))
	watch_signals(match_)
	match_._pass_on_ping(5, Vector3(2, 0, 2), Pings.PLACE)
	assert_signal_emitted_with_parameters(match_, "pinged", [5, Vector3(2, 0, 2), Pings.PLACE])
	match_._pass_on_ping(6, Vector3(2, 0, 2), Pings.PLACE)
	assert_signal_emit_count(match_, "pinged", 1, "an opponent's is not for the host's eyes")
	# Too soon after their last, not a ping, or from nobody: nothing.
	match_._pass_on_ping(5, Vector3(3, 0, 3), Pings.PLACE)
	match_._clock += Pings.GAP + 0.1
	match_._pass_on_ping(5, Vector3(3, 0, 3), "treasure")
	match_._pass_on_ping(77, Vector3(3, 0, 3), Pings.PLACE)
	assert_signal_emit_count(match_, "pinged", 1)
	match_._pass_on_ping(5, Vector3(3, 0, 3), Pings.ENEMY)
	assert_signal_emit_count(match_, "pinged", 2)
	# Nor from someone knocked out.
	match_._clock += Pings.GAP + 0.1
	match_.state.player(5).alive = false
	match_._pass_on_ping(5, Vector3(3, 0, 3), Pings.PLACE)
	assert_signal_emit_count(match_, "pinged", 2)


func test_a_ping_puts_a_marker_down_that_fades_and_a_new_one_replaces_it() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var sounds: int = Sounds.counts.get("ping", 0)
	world.match_.pinged.emit(1, Vector3(3, 0, 3), Pings.PICKUP)
	assert_eq(world.pings.shown().size(), 1)
	assert_eq(world.pings.shown()[0].kind, Pings.PICKUP)
	assert_eq(world.pings.shown()[0].colour, Pings.COLOURS[Pings.PICKUP])
	assert_eq(Sounds.counts.get("ping", 0), sounds + 1, "with a sound")
	var marker: Label3D = world.pings.get_child(0)
	assert_true(marker.no_depth_test, "seen through whatever is in front")
	assert_gt(marker.global_position.y, 1.0, "over the spot")
	world.match_.pinged.emit(1, Vector3(-5, 0, 1), Pings.PLACE)
	assert_eq(world.pings.shown().size(), 1, "one each")
	assert_eq(world.pings.shown()[0].at, Vector3(-5, 0, 1))
	world.match_.pinged.emit(2, Vector3(0, 0, 9), Pings.ENEMY)
	assert_eq(world.pings.shown().size(), 2)
	world.pings._process(Pings.LIFE - 0.5)
	assert_almost_eq((world.pings.shown().size() as float), 2.0, 0.0)
	world.pings._process(0.6)
	assert_eq(world.pings.shown(), [])
	world.match_.pinged.emit(1, Vector3(3, 0, 3), Pings.PLACE)
	world.stop()
	assert_eq(world.pings.shown(), [], "cleared with the session")


func test_a_player_pings_what_they_point_at() -> void:
	await _start(MatchSettings.TEAMS)
	var own: Node3D = world.player_node(1)
	var own_team: int = world.match_.own_record().team
	own.global_position = Vector3(0, 0.1, 8)
	own.rotation.y = 0.0
	own.head.rotation.x = -0.35
	await wait_physics_frames(2)
	var pinged: Dictionary = own.ping()
	# The ground ahead: a place, or the pickup lying there.
	var pads: Array = world.pickup_pads().map(func(pad: Dictionary) -> Vector3: return pad.position)
	assert_eq(pinged.kind, Pings.kind_at(pinged.at, false, pads))
	assert_ne(pinged.kind, Pings.ENEMY)
	assert_lt(pinged.at.z, 8.0)
	assert_almost_eq(pinged.at.y, 0.0, 0.05)
	assert_eq(world.pings.shown().size(), 1)
	# An opponent straight ahead.
	own.head.rotation.x = 0.0
	var enemy := _add(6, BRIE if own_team == CHEDDAR else CHEDDAR, Vector3(0, 0.1, 0))
	await wait_physics_frames(2)
	world.match_._clock += Pings.GAP + 0.1
	pinged = own.ping()
	assert_eq(pinged.kind, Pings.ENEMY)
	assert_almost_eq(pinged.at, enemy.global_position, Vector3.ONE * 0.01, "at their feet")
	# A teammate there instead is only a place.
	world.match_.state.player(6).team = own_team
	pinged = own.ping()
	assert_ne(pinged.kind, Pings.ENEMY)
	# Knocked out, a player pings nothing.
	own.alive = false
	assert_eq(own.ping(), {})


func test_there_are_keys_for_pinging_and_they_are_nobody_elses() -> void:
	for action: String in ["ping", "chat", "chat_team", "quick_chat"]:
		assert_true(InputMap.has_action(action), action)
		for event in InputMap.action_get_events(action):
			if not event is InputEventKey:
				continue
			for other: StringName in InputMap.get_actions():
				if String(other) == action or String(other).begins_with("ui_"):
					continue
				for theirs in InputMap.action_get_events(other):
					if theirs is InputEventKey:
						assert_ne(theirs.physical_keycode, event.physical_keycode, "%s and %s" % [action, other])
