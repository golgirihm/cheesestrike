extends GutTest
## Watching a match rather than playing in it, and watching while knocked out.

const WORLD := preload("res://world/world.tscn")
const BOARD := preload("res://ui/scoreboard.gd")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func _state(mode := "ffa", ids := [1, 2, 3]) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": mode}))
	for id: int in ids:
		state.add_player(id)
	return state


func _start(mode := "ffa") -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("box")
	match_ = world.match_
	match_.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)


func _add(id: int, at: Vector3) -> Node3D:
	match_.add_player(id)
	var player: Node3D = world._create_player([id, Vector3.ZERO])
	world.players.add_child(player)
	player.global_position = at
	return player


# --- The rules --------------------------------------------------------------

func test_a_watcher_is_out_of_the_arena_and_stays_out() -> void:
	var state := _state()
	assert_true(state.set_watching(2, true))
	var record := state.player(2)
	assert_true(record.watching)
	assert_false(record.alive)
	assert_false(state.set_watching(2, true), "no change")
	assert_false(state.set_watching(9, true), "nobody there")
	assert_eq(state.tick(60.0), [] as Array[int], "they don't come back by themselves")
	state.respawn(2)
	assert_false(record.alive, "nor when everyone is brought back")
	state.start_match()
	assert_true(record.watching, "nor for a new match")


func test_a_watcher_cannot_be_hit_or_score() -> void:
	var state := _state()
	state.set_watching(2, true)
	assert_eq(state.damage(1, 2, 50.0), {"dealt": 0, "knocked_out": false, "ended": false, "absorbed": 0})
	assert_eq(state.player(2).health, PlayerRecord.MAX_HEALTH)
	# Something they threw before they turned to watching lands on someone.
	var result := state.damage(2, 3, 500.0)
	assert_eq(result.dealt, 0)
	assert_true(state.player(3).alive)
	assert_eq(state.player(2).kills, 0)
	state.set_pickup_count(1)
	assert_eq(state.collect(0, Pickups.ARMOUR, 2), "", "nor pick anything up")
	state.player(2).overload = Overload.METER_FULL
	assert_false(state.set_off_overload(2))
	assert_false(state.throw_grenade(2))


func test_a_watcher_is_on_no_team_and_takes_one_on_coming_back() -> void:
	var state := _state("teams", [1, 2, 3, 4])
	var was: int = state.player(4).team
	state.set_watching(4, true)
	assert_eq(state.player(4).team, MatchState.NO_TEAM)
	assert_false(state.are_allies(4, 1))
	assert_false(state.are_allies(4, 2))
	assert_true(state.set_watching(4, false))
	assert_eq(state.player(4).team, was, "the smaller team, which is the one they left")
	assert_false(state.player(4).alive, "not at once")
	assert_eq(state.tick(MatchState.RESPAWN_DELAY), [4] as Array[int], "but as a knocked-out player does")
	state.respawn(4)
	assert_true(state.player(4).alive)


func test_watchers_do_not_count_as_players_for_a_match_to_begin() -> void:
	var waiting := _state("ffa", [1, 2])
	waiting.set_watching(2, true)
	waiting.start_match()
	assert_true(waiting.is_warm_up(), "one to play and one to watch is still a warm-up")
	assert_eq(waiting.playing_count(), 1)
	waiting.set_watching(2, false)
	assert_false(waiting.is_warm_up(), "until the watcher plays")


func test_a_match_begun_by_someone_who_came_to_watch_goes_back_to_waiting() -> void:
	var state := _state("ffa", [1, 2])
	assert_false(state.is_warm_up(), "for a moment they were a player")
	state.tick(5.0)
	state.set_watching(2, true)
	assert_true(state.is_warm_up())
	assert_eq(state.time_left, state.settings.minutes * 60.0, "with the clock put back")
	# But not a match in which something has happened.
	var played := _state("ffa", [1, 2, 3])
	played.damage(1, 2, 500.0)
	played.set_watching(2, true)
	played.set_watching(3, true)
	assert_false(played.is_warm_up())


func test_a_watcher_does_not_keep_a_round_going_or_end_one() -> void:
	var state := _state("rounds", [1, 2, 3])
	# Whoever is alone on their team turns to watching: the other team has
	# nobody left to beat, and no round is won.
	var alone := 1
	for id: int in [1, 2, 3]:
		if [1, 2, 3].filter(func(other: int) -> bool: return state.player(other).team == state.player(id).team).size() == 1:
			alone = id
	state.set_watching(alone, true)
	assert_eq(state.team_points, [0, 0])
	assert_eq(state.round_break, 0.0)


func test_watching_survives_the_trip() -> void:
	var state := _state()
	state.set_watching(2, true)
	var copy := MatchState.from_dict(state.to_dict())
	assert_true(copy.player(2).watching)
	assert_false(copy.player(1).watching)
	assert_false(copy.player(2).alive)
	assert_eq(copy.player(1).overload_build, 0.0, "and the fields packed beside it are as they were")


func test_who_can_be_watched() -> void:
	var state := _state("ffa", [1, 2, 3, 4])
	state.set_watching(1, true)
	assert_eq(Watching.choices(state, 1), [2, 3, 4] as Array[int])
	state.damage(2, 3, 500.0)
	assert_eq(Watching.choices(state, 1), [2, 4] as Array[int], "not the knocked out")
	state.set_watching(4, true)
	assert_eq(Watching.choices(state, 1), [2] as Array[int], "nor other watchers")
	# Knocked out in a match without teams, a player may watch anyone.
	assert_eq(Watching.choices(state, 3), [2] as Array[int])
	var teams := _state("teams", [1, 2, 3, 4])
	var enemy := 2 if teams.player(2).team != teams.player(1).team else 3
	teams.damage(enemy, 1, 500.0)
	var mates := Watching.choices(teams, 1)
	assert_eq(mates.size(), 1, "knocked out in teams, only their own")
	assert_eq(teams.player(mates[0]).team, teams.player(1).team)
	teams.set_watching(1, true)
	assert_eq(Watching.choices(teams, 1), [2, 3, 4] as Array[int], "having chosen to watch, everyone")


func test_stepping_through_them_comes_round() -> void:
	var among: Array[int] = [2, 5, 9]
	assert_eq(Watching.next(among, 2), 5)
	assert_eq(Watching.next(among, 9), 2)
	assert_eq(Watching.next(among, 2, -1), 9)
	assert_eq(Watching.next(among, 7), 2, "one who has gone gives the first")
	assert_eq(Watching.next([] as Array[int], 2), 0)


func test_a_knocked_out_player_first_watches_whoever_did_it() -> void:
	var state := _state("ffa", [1, 2, 3])
	state.damage(3, 1, 500.0)
	assert_eq(Watching.first_for(state, 1), 3)
	var teams := _state("teams", [1, 2, 3, 4])
	var enemy := 2 if teams.player(2).team != teams.player(1).team else 3
	teams.damage(enemy, 1, 500.0)
	var shown := Watching.first_for(teams, 1)
	assert_eq(teams.player(shown).team, teams.player(1).team, "in teams, a teammate instead")
	var alone := _state("ffa", [1])
	alone.damage(1, 1, 500.0, true)
	assert_eq(Watching.first_for(alone, 1), 0)


func test_the_view_sits_behind_and_above_looking_the_way_they_look() -> void:
	var view := Watching.chase(Vector3(0, 5, 0), 0.0, 0.0)
	assert_almost_eq(view.origin, Vector3(0, 5 + Watching.UP, Watching.BACK), Vector3.ONE * 0.001)
	assert_almost_eq(-view.basis.z, Vector3.FORWARD, Vector3.ONE * 0.001)
	var turned := Watching.chase(Vector3(0, 5, 0), PI / 2.0, 0.0)
	assert_almost_eq(turned.origin.x, Watching.BACK, 0.001, "behind them, whichever way they face")
	assert_almost_eq(-turned.basis.z, Vector3.LEFT, Vector3.ONE * 0.001)


func test_what_a_watcher_is_told() -> void:
	var state := _state()
	state.rename(2, "Hunter")
	assert_eq(Watching.text(state, 1, 2, false), "", "nothing, while playing")
	state.damage(2, 1, 500.0)
	assert_eq(Watching.text(state, 1, 2, false), "Watching Hunter")
	assert_eq(Watching.text(state, 1, 0, false), "")
	state.set_watching(1, true)
	assert_eq(Watching.text(state, 1, 2, false), "Watching Hunter   ·   click for the next   ·   Space to fly free")
	assert_eq(Watching.text(state, 1, 2, false, true), "Watching Hunter   ·   FIRE for the next   ·   Swap to fly free")
	assert_eq(Watching.text(state, 1, 2, true), "Flying free   ·   Space to follow a player")
	assert_eq(Watching.text(state, 1, 0, false), "Nobody to watch   ·   Space to fly free")
	assert_eq(Watching.text(state, 9, 2, false), "")


func test_the_board_puts_watchers_last_and_says_so() -> void:
	var state := _state()
	state.rename(1, "Brie")
	state.rename(2, "Edam")
	state.rename(3, "Gouda")
	state.player(1).kills = 9
	state.set_watching(1, true)
	var names := BOARD.rows(state).map(func(record: PlayerRecord) -> String: return record.name)
	assert_eq(names[-1], "Brie", "whatever their score")
	assert_eq(BOARD.ping_text(state.player(1), state), "watching")
	assert_eq(HUD.knocked_out_text(state, 1), ["", ""], "and nobody tells them they are knocked out")


# --- In the arena -----------------------------------------------------------

func test_the_host_takes_a_players_word_that_they_will_watch_or_play() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(5, Vector3(0, 0, -9))
	watch_signals(match_)
	match_._watch(5, true)
	assert_true(match_.state.player(5).watching)
	assert_signal_emitted(match_, "state_changed")
	match_._watch(5, "no")
	assert_true(match_.state.player(5).watching, "nonsense changes nothing")
	match_.watch(true)
	assert_true(match_.state.player(1).watching, "the host's own player too")
	match_.watch(false)
	assert_false(match_.state.player(1).watching)


func test_a_new_match_leaves_watchers_watching() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(5, Vector3(0, 0, -9))
	_add(6, Vector3(9, 0, 0))
	match_._watch(6, true)
	match_._new_match()
	assert_true(match_.state.player(5).alive)
	assert_false(match_.state.player(6).alive)
	assert_true(match_.state.player(6).watching)


func test_a_player_out_of_play_sees_through_the_watching_view_and_gets_their_own_back() -> void:
	await _start()
	var own := _add(1, Vector3(0, 0, 9))
	var other := _add(5, Vector3(3, 0, -9))
	own.camera.make_current()
	var view: Camera3D = world.watch_camera
	view._process(0.1)
	assert_false(view.current, "not while playing")
	assert_eq(view.text(), "")
	match_.state.player(1).protected_for = 0.0
	match_.state.damage(5, 1, 500.0)
	match_.state_changed.emit()
	view._process(0.1)
	assert_true(view.current)
	assert_eq(view.target, 5, "whoever knocked them out")
	view._process(1.0)
	assert_lt(view.global_position.distance_to(other.head.global_position), Watching.BACK + Watching.UP + 1.0, "near the one it follows")
	assert_eq(view.text(), "Watching %s" % match_.state.name_of(5))
	view.toggle_free()
	assert_false(view.free, "a knocked-out player doesn't fly about")
	match_.state.respawn(1)
	match_.state_changed.emit()
	view._process(0.1)
	assert_true(own.camera.current, "back behind their own eyes")
	assert_false(view.current)


func test_a_watcher_steps_through_the_players_and_can_fly_free() -> void:
	await _start()
	var own := _add(1, Vector3(0, 0, 9))
	_add(5, Vector3(3, 0, -9))
	_add(6, Vector3(-3, 0, -9))
	match_.watch(true)
	var view: Camera3D = world.watch_camera
	view._process(0.1)
	assert_true(view.current)
	assert_eq(view.target, 5)
	view.step(1)
	assert_eq(view.target, 6)
	view.step(1)
	assert_eq(view.target, 5, "round again")
	view.step(-1)
	assert_eq(view.target, 6)
	# The one being watched is knocked out: on to someone else.
	match_.state.player(6).protected_for = 0.0
	match_.state.damage(5, 6, 500.0)
	view._process(0.1)
	assert_eq(view.target, 5)
	view.toggle_free()
	assert_true(view.free)
	assert_string_starts_with(view.text(), "Flying free")
	own.rotation.y = 0.0
	own.head.rotation.x = 0.0
	view._process(0.0)
	var from := view.global_position
	Input.action_press("move_forward")
	view._process(0.5)
	Input.action_release("move_forward")
	assert_almost_eq(view.global_position, from + Vector3.FORWARD * Watching.FLY_SPEED * 0.5, Vector3.ONE * 0.01, "the way they look")
	view.step(1)
	assert_false(view.free, "choosing a player follows them again")
	# Back to playing: their own view again.
	match_.watch(false)
	match_._process(MatchState.RESPAWN_DELAY + 0.1)
	match_.state_changed.emit()
	view._process(0.1)
	assert_true(own.camera.current)


func test_the_hud_says_who_is_being_watched() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(5, Vector3(3, 0, -9))
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	hud._process(0.1)
	assert_eq(hud.watching_shown(), "")
	match_.watch(true)
	world.watch_camera._process(0.1)
	hud._process(0.1)
	assert_string_starts_with(hud.watching_shown(), "Watching %s" % match_.state.name_of(5))
