extends GutTest
## Voting: on what to play next, and on removing a player.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func _start(settings := {"map": "box"}) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict(settings))
	match_ = world.match_
	await wait_physics_frames(2)


func _add(id: int, at := Vector3(0, 0, 6)) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- The count --------------------------------------------------------------

func test_the_choices_for_the_next_match_end_with_the_same_again() -> void:
	var settings := MatchSettings.from_dict({"map": "box", "mode": "ffa"})
	var ballot := Ballot.for_next(settings)
	assert_eq(ballot.kind, Ballot.NEXT)
	assert_eq(ballot.choices.size(), 3)
	assert_eq(ballot.choices[-1], {"map": "box", "mode": "ffa"})
	for choice: Dictionary in ballot.choices.slice(0, 2):
		assert_ne(choice.map, "box", "somewhere else")
		assert_ne(choice.mode, "ffa", "and something else")
		assert_has(Maps.ORDER, choice.map)
		assert_has(MatchSettings.MODES, choice.mode)
	assert_ne(ballot.choices[0], ballot.choices[1])
	assert_eq(ballot.left, MatchState.INTERMISSION)
	# From the last arena and mode, they come round.
	var last := Ballot.for_next(MatchSettings.from_dict({"map": Maps.ORDER[-1], "mode": MatchSettings.MODES[-1]}))
	assert_eq(last.choices[0], {"map": Maps.ORDER[0], "mode": MatchSettings.MODES[0]})


func test_the_choice_with_most_votes_wins() -> void:
	var ballot := Ballot.for_next(MatchSettings.new())
	assert_true(ballot.vote(1, 0))
	assert_true(ballot.vote(2, 1))
	assert_true(ballot.vote(3, 1))
	assert_eq(ballot.counts(), [1, 2, 0] as Array[int])
	assert_eq(ballot.winner(), ballot.choices[1])


func test_with_no_votes_it_is_the_same_again_and_a_tie_goes_to_the_earlier() -> void:
	var ballot := Ballot.for_next(MatchSettings.new())
	assert_eq(ballot.winner(), ballot.choices[-1])
	ballot.vote(1, 1)
	ballot.vote(2, 0)
	assert_eq(ballot.winner(), ballot.choices[0])


func test_a_voter_may_change_their_mind_but_has_one_vote() -> void:
	var ballot := Ballot.for_next(MatchSettings.new())
	ballot.vote(1, 0)
	ballot.vote(1, 2)
	assert_eq(ballot.counts(), [0, 0, 1] as Array[int])
	ballot.forget(1)
	assert_eq(ballot.counts(), [0, 0, 0] as Array[int], "and takes it with them when they go")


func test_a_vote_for_nothing_on_the_ballot_does_not_count() -> void:
	var ballot := Ballot.for_next(MatchSettings.new())
	for odd: Variant in [3, -1, "1", null, 1.0]:
		assert_false(ballot.vote(1, odd))
	assert_eq(ballot.votes.size(), 0)


func test_removing_someone_needs_more_than_half_of_the_others() -> void:
	var ballot := Ballot.to_remove(9, 4)
	assert_eq(ballot.needed(), 3)
	ballot.vote(1, Ballot.YES)
	ballot.vote(2, Ballot.YES)
	assert_eq(ballot.outcome(), "", "two of four is not enough")
	ballot.vote(3, Ballot.YES)
	assert_eq(ballot.outcome(), "yes")
	assert_eq(Ballot.to_remove(9, 2).needed(), 2, "both, of two")
	assert_eq(Ballot.to_remove(9, 5).needed(), 3)


func test_a_vote_to_remove_fails_once_it_cannot_pass_or_its_time_is_up() -> void:
	var ballot := Ballot.to_remove(9, 4)
	ballot.vote(1, Ballot.YES)
	ballot.vote(2, Ballot.NO)
	assert_eq(ballot.outcome(), "", "three could still say yes")
	ballot.vote(3, Ballot.NO)
	assert_eq(ballot.outcome(), "no", "now they can't")
	var slow := Ballot.to_remove(9, 4)
	slow.vote(1, Ballot.YES)
	slow.left = 0.0
	assert_eq(slow.outcome(), "no")


func test_the_player_it_is_about_has_no_say() -> void:
	var ballot := Ballot.to_remove(9, 2)
	assert_false(ballot.vote(9, Ballot.NO))
	assert_eq(ballot.votes.size(), 0)


func test_what_everyone_is_shown() -> void:
	var names := func(id: int) -> String: return "Player %d" % id
	var next := Ballot.for_next(MatchSettings.from_dict({"map": "box", "mode": "ffa"}))
	next.vote(1, 0)
	var lines := next.lines(names)
	assert_eq(lines[0], "Vote for the next match")
	assert_eq(lines[1], "F1   %s   (1)" % Ballot.choice_text(next.choices[0]))
	assert_eq(lines[3], "F3   The same again   (0)")
	assert_eq(Ballot.choice_text({"map": "city", "mode": "ctf"}), "Curd City  ·  Capture the wheel")
	var remove := Ballot.to_remove(9, 4)
	remove.vote(1, Ballot.YES)
	remove.left = 12.3
	assert_eq(remove.lines(names), ["Remove Player 9?   3 needed   ·   13s", "F1   Yes   (1)", "F2   No   (0)"] as Array[String])


func test_a_ballot_survives_the_trip_and_nonsense_is_none() -> void:
	var state := MatchState.new()
	state.add_player(1)
	state.add_player(2)
	state.end_match()
	state.ballot.vote(2, 1)
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(copy.ballot.kind, Ballot.NEXT)
	assert_eq(copy.ballot.choices, state.ballot.choices)
	assert_eq(copy.ballot.counts(), [0, 1, 0] as Array[int])
	state.ballot = Ballot.to_remove(2, 3)
	state.ballot.vote(1, Ballot.YES)
	copy = MatchState.from_dict(state.to_dict())
	assert_eq(copy.ballot.subject, 2)
	assert_eq(copy.ballot.needed(), 2)
	assert_eq(copy.ballot.counts(), [1, 0] as Array[int])
	for odd: Variant in [null, 7, [], ["next", [], 0, 0, {}, 5.0], ["x", [], 0, 0, {}, 5.0], ["next", [["moon", "ffa"]], 0, 0, {}, 5.0], ["remove", [], "a", 0, {}, 5.0]]:
		assert_null(Ballot.from_array(odd), str(odd))
	var wild := Ballot.from_array(["remove", [], 2, 999, {1: 0, "x": 1, 3: 7, 2: 1}, NAN])
	assert_eq(wild.eligible, MatchSettings.MAX_PLAYERS)
	assert_eq(wild.votes, {1: 0}, "only votes that could have been cast")
	assert_eq(wild.left, 0.0)
	assert_false(MatchState.new().to_dict().has("ballot"), "nothing sent when nothing is being voted on")


# --- In the match -----------------------------------------------------------

func test_a_match_ends_with_a_vote_and_the_next_begins_without_one() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"map": "box"}))
	state.add_player(1)
	state.add_player(2)
	assert_null(state.ballot)
	state.end_match()
	assert_not_null(state.ballot)
	assert_eq(state.ballot.kind, Ballot.NEXT)
	state.tick(4.0)
	assert_almost_eq(state.ballot.left, MatchState.INTERMISSION - 4.0, 0.001)
	state.start_match()
	assert_null(state.ballot)
	var practice := MatchState.new(MatchSettings.practice())
	practice.end_match()
	assert_null(practice.ballot, "nothing to vote on, on the range")


func test_a_state_for_new_settings_keeps_who_everyone_is() -> void:
	var state := MatchState.new(MatchSettings.from_dict({"map": "box", "mode": "ffa"}))
	for id: int in [1, 2, 3, -1]:
		state.add_player(id)
	state.rename(2, "Hunter")
	state.player(2).look = Looks.pack("blue", "crown", "none")
	state.player(2).main = "rifle"
	state.player(2).attachment = Attachments.GRIP
	state.player(2).overload_kind = Overload.RAIN
	state.player(2).kills = 9
	state.set_watching(3, true)
	var next := state.with_settings(MatchSettings.from_dict({"map": "city", "mode": "ctf"}))
	assert_eq(next.settings.map, "city")
	assert_not_null(next.capture, "the new mode's own things")
	assert_eq(next.players.keys(), state.players.keys())
	var record := next.player(2)
	assert_eq(record.name, "Hunter")
	assert_eq(record.look, state.player(2).look)
	assert_eq(record.main, "rifle")
	assert_eq(record.weapons, [Weapons.STARTING, "rifle"])
	assert_eq(record.fitted, Attachments.GRIP)
	assert_eq(record.overload_kind, Overload.RAIN)
	assert_eq(record.kills, 0, "but not the score")
	assert_ne(record.team, MatchState.NO_TEAM, "on a team, now that there are teams")
	assert_true(next.player(3).watching, "and the watcher still watches")
	assert_eq(next.player(3).team, MatchState.NO_TEAM)


func test_the_host_counts_votes_from_players_only() -> void:
	await _start()
	_add(5)
	_add(-1)
	match_.state.end_match()
	match_._cast(5, 0)
	match_._cast(-1, 1)
	match_._cast(77, 1)
	match_._cast(5, "x")
	assert_eq(match_.state.ballot.counts(), [1, 0, 0] as Array[int], "not a bot's, nor someone's who isn't here")
	match_.cast(1)
	assert_eq(match_.state.ballot.counts(), [1, 1, 0] as Array[int], "the host's own counts too")


func test_the_next_match_is_what_was_voted_for() -> void:
	await _start({"map": "box", "mode": "ffa", "minutes": 5, "rules": "tough"})
	var own: Node3D = world.player_node(1)
	_add(5)
	match_.state.rename(5, "Hunter")
	match_.state.end_match()
	var chosen: Dictionary = match_.state.ballot.choices[0]
	match_._cast(1, 0)
	match_._cast(5, 0)
	watch_signals(match_)
	match_._new_match()
	assert_eq(match_.state.settings.map, chosen.map)
	assert_eq(match_.state.settings.mode, chosen.mode)
	assert_eq(world.map_id, chosen.map, "the host is on the new arena")
	assert_eq(match_.state.settings.minutes, 5, "the rest of the settings are as they were")
	assert_eq(match_.state.settings.rules, "tough")
	assert_true(match_.state.is_playing())
	assert_null(match_.state.ballot)
	assert_eq(match_.state.name_of(5), "Hunter")
	assert_eq(match_.state.pickups.size(), world.pickup_pads().size(), "with the new arena's pads")
	assert_eq(match_.state.blocks.health.size(), world.cheese_blocks().size(), "and its cheese")
	assert_signal_emitted_with_parameters(match_, "announced", ["Next: %s" % Ballot.choice_text(chosen), MatchState.NO_TEAM])
	# Everyone is on one of the new arena's starting spots.
	var spots: Array[Vector3] = world.spawn_positions()
	assert_true(spots.any(func(spot: Vector3) -> bool: return spot.distance_to(own.global_position) < 0.5))
	assert_eq(match_.state.player(1).health, 200)


func test_with_no_votes_the_next_match_is_the_same_again() -> void:
	await _start({"map": "box", "mode": "ffa"})
	_add(5)
	match_.state.end_match()
	var before := match_.state
	match_._new_match()
	assert_same(match_.state, before, "nothing made anew")
	assert_eq(world.map_id, "box")
	assert_eq(match_.state.settings.mode, "ffa")


func test_who_may_be_put_to_a_vote() -> void:
	await _start()
	_add(5)
	assert_false(match_.can_propose(1, 5), "two players can't vote a third out, there being none")
	_add(6)
	_add(-1)
	assert_true(match_.can_propose(5, 6))
	assert_true(match_.can_propose(1, 6), "the host may ask too")
	assert_false(match_.can_propose(5, 5), "not themselves")
	assert_false(match_.can_propose(5, 1), "not the host, whose session it is")
	assert_false(match_.can_propose(5, -1), "not a bot")
	assert_false(match_.can_propose(-1, 5), "nor may a bot ask")
	assert_false(match_.can_propose(5, 99))
	match_._propose(5, 6)
	assert_false(match_.can_propose(1, 5), "one vote at a time")


func test_a_vote_to_remove_passes_when_enough_say_yes() -> void:
	await _start()
	_add(5)
	_add(6)
	_add(7)
	match_._known_as(7, "key-of-seven")
	watch_signals(match_)
	match_._propose(5, 7)
	var ballot: Ballot = match_.state.ballot
	assert_eq(ballot.kind, Ballot.REMOVE)
	assert_eq(ballot.subject, 7)
	assert_eq(ballot.eligible, 3)
	assert_eq(ballot.counts(), [1, 0] as Array[int], "whoever asked says yes")
	assert_signal_emitted(match_, "announced")
	match_._cast(7, Ballot.NO)
	assert_eq(ballot.counts(), [1, 0] as Array[int], "they have no say")
	match_._cast(6, Ballot.YES)
	assert_null(match_.state.ballot, "two of three: it is settled")
	assert_true(match_._removed_keys.has("key-of-seven"), "and they are kept out")
	assert_string_contains(get_signal_parameters(match_, "announced")[0], "was removed by a vote")


func test_a_vote_to_remove_fails_when_enough_say_no_or_nobody_answers() -> void:
	await _start()
	_add(5)
	_add(6)
	_add(7)
	match_._propose(5, 7)
	match_._cast(6, Ballot.NO)
	match_._cast(1, Ballot.NO)
	assert_null(match_.state.ballot)
	assert_eq(match_._removed_keys.size(), 0)
	assert_not_null(match_.state.player(7))
	match_._propose(5, 7)
	watch_signals(match_)
	match_._process(Ballot.REMOVE_TIME + 0.1)
	assert_null(match_.state.ballot, "its time ran out")
	assert_string_ends_with(get_signal_parameters(match_, "announced")[0], "stays")


func test_someone_removed_who_leaves_and_comes_back_is_removed_again() -> void:
	await _start()
	_add(5)
	match_._removed_keys["bad-key"] = true
	assert_true(match_._removed_keys.has("bad-key"))
	# A new session forgets them.
	match_.start(MatchSettings.from_dict({"map": "box"}))
	assert_eq(match_._removed_keys.size(), 0)


func test_the_hud_shows_the_vote_and_the_keys_cast_one() -> void:
	await _start()
	var own: Node3D = world.player_node(1)
	_add(5)
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	hud._process(0.1)
	assert_eq(hud.vote_shown(), [])
	match_.state.end_match()
	hud._process(0.1)
	assert_eq(hud.vote_shown()[0], "Vote for the next match")
	assert_eq(hud.vote_shown().size(), 4)
	for option in 3:
		assert_true(InputMap.has_action("vote_%d" % (option + 1)))
	var press := InputEventAction.new()
	press.action = "vote_2"
	press.pressed = true
	own._input(press)
	assert_eq(match_.state.ballot.counts(), [0, 1, 0] as Array[int])
	hud._vote_buttons[0].pressed.emit()
	assert_eq(match_.state.ballot.counts(), [1, 0, 0] as Array[int], "or the buttons, without a keyboard")
	hud._process(0.1)
	assert_string_ends_with(hud.vote_shown()[1], "(1)")
