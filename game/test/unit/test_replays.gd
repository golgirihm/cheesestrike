extends GutTest
## Showing part of a match again: what the host keeps of it, the clips cut
## from that, how the play of the match is chosen, and the playing of a clip.

const WORLD := preload("res://world/world.tscn")
const HUD := preload("res://ui/combat_hud.gd")

var world: Node3D
var match_: Match


func _start() -> void:
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


# A recording of player 1 walking along x at a metre a second, and player 2
# standing still, for `seconds`.
func _walked(seconds: float) -> Recording:
	var recording := Recording.new()
	var time := 100.0
	while time <= 100.0 + seconds + 0.0001:
		recording.add(time, 1, Vector3(time - 100.0, 0, 0), Vector2(0.5, 0.1))
		recording.add(time, 2, Vector3(0, 0, -9), Vector2.ZERO)
		time += 1.0 / 60.0
	return recording


# --- What the host keeps ----------------------------------------------------

func test_it_keeps_a_few_moments_a_second_and_only_the_last_few_seconds() -> void:
	var recording := _walked(20.0)
	var track: PackedFloat32Array = recording._tracks[1]
	var moments := track.size() / 6
	assert_between(moments, int(Recording.KEEP / Recording.INTERVAL * 0.7), int(Recording.KEEP / Recording.INTERVAL) + 3, "not every frame, and not for ever")
	assert_gte(track[0], 120.0 - Recording.KEEP - Recording.INTERVAL, "the oldest is recent")


func test_a_clip_is_the_last_seconds_of_the_players_asked_for() -> void:
	var recording := _walked(10.0)
	var clip := recording.clip([1], 110.0, 3.0)
	assert_eq(clip.length, 3.0)
	assert_eq(clip.tracks.keys(), [1], "only who was asked for")
	var track: PackedFloat32Array = clip.tracks[1]
	assert_between(track[0], -Recording.INTERVAL, Recording.INTERVAL, "starting at its beginning")
	assert_almost_eq(track[track.size() - 6], 3.0, Recording.INTERVAL + 0.01, "and ending at its end")
	assert_eq(recording.clip([7], 110.0, 3.0).tracks.size(), 0, "nothing of someone who was never there")
	assert_lte(recording.clip([1, 2, 3, 4, 5, 6], 110.0, 3.0).tracks.size(), Recording.MAX_TRACKS)


func test_a_player_is_found_between_the_moments_kept() -> void:
	var clip := _walked(10.0).clip([1, 2], 110.0, 3.0)
	# Three seconds from the end they were at 7 m, walking a metre a second.
	for time: float in [0.0, 0.77, 1.5, 2.93]:
		var pose := Recording.pose(clip.tracks[1], time)
		assert_almost_eq(pose.at.x, 7.0 + time, 0.02, "at %.2f s" % time)
		assert_almost_eq(pose.yaw, 0.5, 0.001)
		assert_almost_eq(pose.pitch, 0.1, 0.001)
	assert_almost_eq(Recording.pose(clip.tracks[1], -5.0).at.x, 7.0, 0.11, "before it began, where they began")
	assert_almost_eq(Recording.pose(clip.tracks[1], 99.0).at.x, 10.0, 0.11, "after it ended, where they ended")
	assert_eq(Recording.pose(clip.tracks[2], 1.0).at, Vector3(0, 0, -9))
	assert_eq(Recording.pose(PackedFloat32Array(), 1.0), {})


func test_turning_goes_the_short_way_round() -> void:
	var track := PackedFloat32Array([0.0, 0, 0, 0, PI - 0.1, 0.0, 1.0, 0, 0, 0, -PI + 0.1, 0.0])
	assert_almost_eq(absf(Recording.pose(track, 0.5).yaw), PI, 0.001)


func test_shots_are_kept_with_who_fired_them() -> void:
	var recording := _walked(5.0)
	recording.shot(103.0, 1, Vector3(3, 2, 0), [Vector3(3, 2, -20), Vector3(4, 2, -20)])
	recording.shot(104.0, 2, Vector3(0, 2, -9), [Vector3(0, 2, 20)])
	recording.shot(104.5, 1, Vector3.ZERO, [])
	var clip := recording.clip([1], 105.0, 3.0)
	assert_eq(clip.shots.size(), 1, "theirs, with an end to draw it to")
	assert_almost_eq(clip.shots[0][0], 1.0, 0.001, "a second into the clip")
	assert_eq(clip.shots[0][1], 1)
	assert_eq(clip.shots[0][3], Vector3(3, 2, -20), "one line for all its pellets")
	assert_eq(recording.clip([1, 2], 105.0, 3.0).shots.size(), 2)
	recording.shot(200.0, 1, Vector3.ZERO, [Vector3.ONE])
	assert_eq(recording._shots.size(), 1, "old ones are forgotten")


func test_a_player_who_leaves_is_forgotten_and_a_new_match_starts_clean() -> void:
	var recording := _walked(2.0)
	recording.forget(1)
	assert_eq(recording.clip([1, 2], 102.0, 2.0).tracks.keys(), [2])
	recording.clear()
	assert_eq(recording.clip([1, 2], 102.0, 2.0).tracks.size(), 0)


func test_a_clip_from_another_machine_is_checked() -> void:
	var clip := _walked(5.0).clip([1, 2], 105.0, 3.0)
	clip.shots = [[1.0, 1, Vector3.ZERO, Vector3.ONE]]
	var sent: Variant = bytes_to_var(var_to_bytes(clip))
	var clean := Recording.clean(sent)
	assert_eq(clean.tracks.keys(), [1, 2])
	assert_eq(clean.shots.size(), 1)
	assert_lt(var_to_bytes(clip).size(), 3000, "small enough to send")
	for odd: Variant in [null, 7, {}, {"length": 3.0}, {"length": -1.0, "tracks": {}}, {"length": 999.0, "tracks": clip.tracks}, {"length": NAN, "tracks": clip.tracks}, {"length": 3.0, "tracks": {}}]:
		assert_eq(Recording.clean(odd), {}, str(odd).left(40))
	var wild := Recording.clean({"length": 3.0, "tracks": {1: PackedFloat32Array([0, 0, 0]), "x": clip.tracks[1], 2: PackedFloat32Array([0, INF, 0, 0, 0, 0]), 3: [1, 2, 3], 4: clip.tracks[2]},
		"shots": [7, [1.0, "a", Vector3.ZERO, Vector3.ONE], [1.0, 1, Vector3(NAN, 0, 0), Vector3.ONE], [0.5, 4, Vector3.ZERO, Vector3.ONE]]})
	assert_eq(wild.tracks.keys(), [4], "only the track that is one")
	assert_eq(wild.shots.size(), 1)


# --- The play of the match --------------------------------------------------

func test_a_knock_out_scores_and_a_run_of_them_scores_more() -> void:
	var highlights := Highlights.new()
	var one := highlights.knock_out(10.0, 1, false, false)
	assert_eq(one, {"score": Highlights.KNOCK_OUT, "text": "Knock-out"})
	var two := highlights.knock_out(12.0, 1, false, false)
	assert_eq(two, {"score": Highlights.KNOCK_OUT * 2.0, "text": "Double knock-out"})
	assert_eq(highlights.knock_out(15.0, 1, false, false).text, "Triple knock-out")
	assert_eq(highlights.knock_out(15.0 + Highlights.RUN_WINDOW + 0.1, 1, false, false).score, Highlights.KNOCK_OUT, "too long after: a new run")
	assert_eq(highlights.knock_out(15.5, 2, false, false).score, Highlights.KNOCK_OUT, "each player has their own")


func test_a_head_shot_and_the_knife_score_a_little_more() -> void:
	var highlights := Highlights.new()
	var plain: float = highlights.knock_out(10.0, 1, false, false).score
	var head := highlights.knock_out(10.0, 2, true, false)
	var knife := highlights.knock_out(10.0, 3, false, true)
	assert_gt(head.score, plain)
	assert_gt(knife.score, head.score)
	assert_eq(head.text, "Head shot")
	assert_eq(knife.text, "Knife knock-out")
	assert_lt(knife.score, Highlights.KNOCK_OUT * 2.0, "but never as much as a second knock-out")
	assert_eq(Highlights.describe(9, true, true), Highlights.RUN_NAMES[-1], "a long run is called the longest there is a name for")


func test_the_best_moment_is_kept_and_an_equal_one_does_not_replace_it() -> void:
	var highlights := Highlights.new()
	var clip := {"length": 4.0, "tracks": {1: PackedFloat32Array([0, 0, 0, 0, 0, 0])}, "shots": []}
	assert_eq(highlights.best, {})
	assert_true(highlights.consider({"score": 100.0, "text": "Knock-out"}, clip, 1, 2))
	assert_false(highlights.consider({"score": 100.0, "text": "Knock-out"}, clip, 3, 4), "the first of equals stays")
	assert_eq(highlights.best.attacker, 1)
	assert_true(highlights.consider({"score": 130.0, "text": "Head shot"}, clip, 3, 4))
	assert_eq(highlights.best.attacker, 3)
	assert_eq(highlights.best.text, "Head shot")
	assert_false(highlights.consider({"score": 999.0, "text": "x"}, {}, 5, 6), "nothing to show, nothing kept")
	highlights.clear()
	assert_eq(highlights.best, {})
	assert_eq(highlights.knock_out(10.0, 1, false, false).score, Highlights.KNOCK_OUT, "and runs start again")


# --- In the arena -----------------------------------------------------------

func test_the_host_records_where_players_are_and_what_they_fire() -> void:
	await _start()
	var own := _add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	match_.state.player(1).weapons.append("blaster")
	for frame in 30:
		match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	match_.record_position(Match.now(), Vector3(0, 0, -9), 2)
	var origin: Vector3 = own.head.global_position
	match_._shot(1, origin, Vector3.FORWARD, 1, "blaster", WeaponStats.AIMED)
	var clip := match_.recording.clip([1, 2], Match.now(), 2.0)
	assert_eq(clip.tracks.keys(), [1, 2])
	assert_eq(clip.shots.size(), 1)
	assert_eq(clip.shots[0][1], 1)


func test_a_knocked_out_player_is_shown_how_from_behind_whoever_did_it() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	for frame in 20:
		match_.record_position(Match.now(), Vector3(frame * 0.1, 0, -9), 2, Vector2(0.3, 0.0))
		match_._physics_process(1.0 / 60.0)
		await wait_physics_frames(1)
	match_.state.player(1).protected_for = 0.0
	watch_signals(match_)
	match_._harm(2, 1, 500.0, Vector3.ZERO, "blaster", true)
	assert_signal_emitted(match_, "replay_shown")
	var shown: Array = get_signal_parameters(match_, "replay_shown")
	assert_eq(shown[1], 2, "about the one who did it")
	assert_eq(shown[2], Match.KNOCK_OUT_TITLE)
	assert_true(shown[0].tracks.has(2))
	assert_lte(shown[0].length, MatchState.RESPAWN_DELAY, "over by the time they are back")
	var view: Node3D = world.replay_view
	assert_true(view.playing)
	assert_eq(view.title, Match.KNOCK_OUT_TITLE)
	assert_gt(view.ghosts(), 0)
	assert_false(world.players.visible, "the real players are out of sight meanwhile")
	assert_true(view.get_node("ReplayCamera").current)
	# The view is behind the player it is about, as they were.
	var pose := Recording.pose(shown[0].tracks[2], 0.0)
	assert_lt(view.get_node("ReplayCamera").global_position.distance_to(pose.at), Watching.BACK + BodyShape.HEIGHT + 3.0)
	# It plays to its end and gives everything back.
	view._process(shown[0].length * 0.5)
	assert_true(view.playing)
	view._process(shown[0].length)
	assert_false(view.playing)
	assert_true(world.players.visible)
	assert_eq(view.ghosts(), 0)
	assert_eq(view.title, "")


func test_knocking_yourself_out_shows_nothing() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	match_.state.player(1).protected_for = 0.0
	watch_signals(match_)
	match_._harm(1, 1, 500.0, Vector3.ZERO, "launcher", false)
	assert_signal_not_emitted(match_, "replay_shown")
	assert_eq(match_.highlights.best, {}, "nor is it anyone's best moment")


func test_the_best_moment_is_shown_to_everyone_when_the_match_ends() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	match_.state.rename(1, "Hunter")
	for frame in 20:
		match_._physics_process(1.0 / 60.0)
		match_.record_position(Match.now(), Vector3(0, 0, -9), 2)
		await wait_physics_frames(1)
	match_.state.player(2).protected_for = 0.0
	match_._harm(1, 2, 500.0, Vector3.ZERO, "blaster", true)
	assert_eq(match_.highlights.best.attacker, 1)
	assert_eq(match_.highlights.best.text, "Head shot")
	world.replay_view.stop()
	watch_signals(match_)
	# The match ends on a knock-out, between frames: shown on the next.
	match_._process(0.01)
	match_.state.end_match()
	match_._process(0.01)
	assert_signal_emit_count(match_, "replay_shown", 1)
	match_._process(0.01)
	assert_signal_emit_count(match_, "replay_shown", 1, "once")
	var shown: Array = get_signal_parameters(match_, "replay_shown")
	assert_eq(shown[1], 1)
	assert_eq(shown[2], "Play of the match: Hunter  ·  Head shot")
	assert_almost_eq(shown[0].length, Highlights.CLIP, 0.001)
	assert_true(world.replay_view.playing)
	# The next match starts with nothing chosen.
	match_._new_match()
	assert_eq(match_.highlights.best, {})


func test_a_match_in_which_nothing_happened_has_no_play_of_the_match() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	watch_signals(match_)
	match_._show_play_of_the_match()
	assert_signal_not_emitted(match_, "replay_shown")


func test_someone_who_has_turned_replays_off_is_shown_none() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	var clip := {"length": 2.0, "tracks": {2: PackedFloat32Array([0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0])}, "shots": []}
	Settings.set_value("replays", false)
	match_.replay_shown.emit(clip, 2, "x")
	assert_false(world.replay_view.playing)
	Settings.set_value("replays", true)
	match_.replay_shown.emit(clip, 2, "x")
	assert_true(world.replay_view.playing)


func test_a_clip_with_nothing_of_the_player_it_is_about_is_not_played() -> void:
	await _start()
	var clip := {"length": 2.0, "tracks": {2: PackedFloat32Array([0, 0, 0, 0, 0, 0])}, "shots": []}
	world.replay_view.play(clip, 7, "x")
	assert_false(world.replay_view.playing)
	world.replay_view.play({"nonsense": true}, 2, "x")
	assert_false(world.replay_view.playing)
	assert_true(world.players.visible)


func test_leaving_in_the_middle_of_a_clip_ends_it() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	var clip := {"length": 2.0, "tracks": {2: PackedFloat32Array([0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0])}, "shots": []}
	world.replay_view.play(clip, 2, "x")
	world.stop()
	assert_false(world.replay_view.playing)
	assert_true(world.players.visible)


func test_the_view_of_someone_watching_waits_for_a_clip_to_end() -> void:
	await _start()
	var own := _add(1, Vector3(0, 0, 9))
	_add(2, Vector3(0, 0, -9))
	own.camera.make_current()
	match_.state.player(1).protected_for = 0.0
	match_._harm(2, 1, 500.0, Vector3.ZERO, "blaster", false)
	match_.record_position(Match.now(), Vector3(0, 0, -9), 2)
	if not world.replay_view.playing:
		world.replay_view.play({"length": 2.0, "tracks": {2: PackedFloat32Array([0, 0, 0, -9, 0, 0])}, "shots": []}, 2, "x")
	world.watch_camera._process(0.1)
	assert_false(world.watch_camera.current, "the clip has the view")
	world.replay_view.stop()
	world.watch_camera._process(0.1)
	assert_true(world.watch_camera.current, "and then it is theirs")


func test_the_hud_says_what_is_being_shown() -> void:
	await _start()
	_add(1, Vector3(0, 0, 9))
	var hud: Control = HUD.new()
	add_child_autofree(hud)
	hud.bind(world)
	hud._process(0.1)
	assert_eq(hud.replay_shown(), "")
	world.replay_view.play({"length": 2.0, "tracks": {2: PackedFloat32Array([0, 0, 0, 0, 0, 0])}, "shots": []}, 2, "How you were knocked out")
	hud._process(0.1)
	assert_eq(hud.replay_shown(), "How you were knocked out")
	world.replay_view.stop()
	hud._process(0.1)
	assert_eq(hud.replay_shown(), "")
