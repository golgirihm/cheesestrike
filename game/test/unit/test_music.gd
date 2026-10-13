extends GutTest
## Music that follows the match, and the sounds the interface makes.

const WORLD := preload("res://world/world.tscn")
const MAIN := preload("res://main.tscn")

var saved := {}


func before_each() -> void:
	saved["seen_help"] = Settings.get_value("seen_help")
	Settings.set_value("seen_help", true)


func after_each() -> void:
	Settings.set_value("seen_help", saved["seen_help"])
	Sounds.set_music_quiet(false)
	Sounds.set_piece(MusicDirector.MENU)


func _standing(changed := {}) -> Dictionary:
	var standing := {"playing": true, "ended": false, "won": null, "leading": null, "teams": false, "time_left": 300.0, "sudden": false}
	standing.merge(changed, true)
	return standing


func _state(mode: String, limit := 20) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict({"mode": mode, "kill_limit": limit}))
	for id in [1, 2]:
		state.add_player(id)
	state.start_match()
	return state


func test_the_menu_has_its_theme_and_a_match_a_quieter_piece() -> void:
	assert_eq(MusicDirector.piece(false, {}, false), MusicDirector.MENU)
	assert_eq(MusicDirector.piece(false, _standing(), true), MusicDirector.MENU, "whatever the match, on the menu")
	assert_eq(MusicDirector.piece(true, {}, false), MusicDirector.MENU, "in a session with no match yet")
	assert_eq(MusicDirector.piece(true, _standing(), false), MusicDirector.MATCH)
	assert_eq(MusicDirector.piece(true, _standing({"playing": false}), false), MusicDirector.MATCH, "in the warm-up")


func test_the_music_builds_in_the_last_minute_at_match_point_and_in_sudden_death() -> void:
	assert_eq(MusicDirector.piece(true, _standing({"time_left": MusicDirector.LAST + 0.5}), false), MusicDirector.MATCH)
	assert_eq(MusicDirector.piece(true, _standing({"time_left": MusicDirector.LAST}), false), MusicDirector.TENSE)
	assert_eq(MusicDirector.piece(true, _standing({"time_left": 5.0}), false), MusicDirector.TENSE)
	assert_eq(MusicDirector.piece(true, _standing(), true), MusicDirector.TENSE, "match point")
	assert_eq(MusicDirector.piece(true, _standing({"sudden": true}), false), MusicDirector.TENSE)
	# Not once it is over, nor before it has begun.
	assert_eq(MusicDirector.piece(true, _standing({"playing": false, "ended": true, "time_left": 0.0}), true), MusicDirector.MATCH)
	assert_eq(MusicDirector.piece(true, _standing({"playing": false, "time_left": 10.0}), false), MusicDirector.MATCH)


func test_match_point_is_a_score_from_winning_or_the_last_tenth_of_a_long_one() -> void:
	var state := _state(MatchSettings.FREE_FOR_ALL, 20)
	assert_false(MusicDirector.match_point(state))
	state.player(1).kills = 17
	assert_false(MusicDirector.match_point(state))
	state.player(1).kills = 18
	assert_true(MusicDirector.match_point(state), "two from twenty")
	state = _state(MatchSettings.FREE_FOR_ALL, 5)
	state.player(2).kills = 3
	assert_false(MusicDirector.match_point(state))
	state.player(2).kills = 4
	assert_true(MusicDirector.match_point(state), "one from five")
	# By the team in a team match.
	state = _state(MatchSettings.TEAMS, 10)
	state.team_kills[MatchState.BRIE] = 9
	assert_true(MusicDirector.match_point(state))
	# Not in a match of one knock-out, which is all match point, nor before
	# it begins or after it ends.
	assert_false(MusicDirector.match_point(_state(MatchSettings.FREE_FOR_ALL, 1)))
	state.end_match()
	assert_false(MusicDirector.match_point(state))


func test_match_point_in_a_mode_played_for_points() -> void:
	for mode: String in MatchSettings.MODES:
		var state := _state(mode)
		if not state.settings.by_points():
			continue
		var limit := state.settings.limit()
		state.team_points[MatchState.CHEDDAR] = limit - maxi(1, limit / 10) - 1
		assert_false(MusicDirector.match_point(state), mode)
		state.team_points[MatchState.CHEDDAR] = limit - 1
		assert_true(MusicDirector.match_point(state), mode)


func test_the_end_has_a_sting_for_winning_and_one_for_losing() -> void:
	assert_eq(MusicDirector.sting(true), MusicDirector.WIN)
	assert_eq(MusicDirector.sting(false), MusicDirector.LOSE)
	assert_eq(MusicDirector.sting(null), "", "none for a draw, or for someone watching")
	for sound: String in [MusicDirector.WIN, MusicDirector.LOSE]:
		assert_not_null(SoundBank.get_sound(sound))
		assert_eq(Mix.bus_of(sound), Mix.MUSIC)
	assert_ne(SoundBank.get_sound(MusicDirector.WIN).data, SoundBank.get_sound(MusicDirector.LOSE).data)


func test_every_piece_is_a_loop_of_its_own_on_the_music_bus() -> void:
	var seen := []
	for piece: String in MusicDirector.PIECES:
		var stream := SoundBank.get_sound(piece)
		assert_not_null(stream, piece)
		assert_eq(stream.loop_mode, AudioStreamWAV.LOOP_FORWARD, piece)
		assert_eq(Mix.bus_of(piece), Mix.MUSIC, piece)
		assert_does_not_have(seen, stream.data, piece)
		seen.append(stream.data)
	# The piece that drives is the fastest: its loop of sixteen beats is the
	# shortest.
	assert_lt(SoundBank.seconds(SoundBank.get_sound(MusicDirector.TENSE)), SoundBank.seconds(SoundBank.get_sound(MusicDirector.MENU)))
	assert_gt(SoundBank.seconds(SoundBank.get_sound(MusicDirector.MATCH)), SoundBank.seconds(SoundBank.get_sound(MusicDirector.MENU)), "and the one under a match the slowest")


func test_one_piece_gives_way_to_another_without_a_gap() -> void:
	Sounds.play_music()
	Sounds.set_piece(MusicDirector.MENU)
	for step in 30:
		Sounds._process(0.1)
	assert_eq(Sounds.piece(), MusicDirector.MENU)
	var menu_player: AudioStreamPlayer = Sounds._music
	Sounds.set_piece(MusicDirector.TENSE)
	assert_eq(Sounds.piece(), MusicDirector.TENSE)
	assert_ne(Sounds._music, menu_player, "on a player of its own")
	assert_eq(Sounds._music.stream, SoundBank.get_sound(MusicDirector.TENSE))
	assert_true(Sounds._music.playing)
	assert_true(menu_player.playing, "the old piece is still sounding")
	assert_lt(Sounds._music.volume_db, -50.0, "and the new one comes in from nothing")
	Sounds._process(Sounds.MUSIC_FADE / 2.0)
	assert_between(Sounds._music.volume_db, -50.0, -9.0)
	assert_lt(menu_player.volume_db, -9.0)
	for step in 20:
		Sounds._process(0.1)
	assert_almost_eq(Sounds._music.volume_db, -8.0, 0.01)
	assert_false(menu_player.playing, "the old one has gone")
	# Asked for what is already playing, nothing changes.
	var playing: AudioStreamPlayer = Sounds._music
	Sounds.set_piece(MusicDirector.TENSE)
	assert_eq(Sounds._music, playing)
	Sounds.set_piece("kazoo")
	assert_eq(Sounds.piece(), MusicDirector.TENSE, "nor for a piece there isn't")


func test_the_music_follows_a_match_and_stings_once_at_its_end() -> void:
	Sounds.play_music()
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	await wait_seconds(0.4)
	assert_eq(Sounds.piece(), MusicDirector.MENU)
	world.start(MatchSettings.from_dict({"map": "box", "kill_limit": 5}))
	Sounds.set_music_quiet(true)
	world.match_.add_player(2)
	world.match_.state.start_match()
	await wait_seconds(0.4)
	assert_eq(Sounds.piece(), MusicDirector.MATCH)
	world.match_.state.player(2).kills = 4
	await wait_seconds(0.4)
	assert_eq(Sounds.piece(), MusicDirector.TENSE, "match point")
	var lost: int = Sounds.counts.get(MusicDirector.LOSE, 0)
	world.match_.state.player(2).kills = 5
	world.match_.state.end_match()
	await wait_seconds(0.8)
	assert_eq(Sounds.counts.get(MusicDirector.LOSE, 0), lost + 1, "once")
	assert_eq(Sounds.piece(), MusicDirector.MATCH)
	world.stop()
	Sounds.set_music_quiet(false)
	await wait_seconds(0.4)
	assert_eq(Sounds.piece(), MusicDirector.MENU, "back on the menu")


func test_every_button_in_the_game_makes_a_sound() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(3)
	var buttons := main.find_children("*", "BaseButton", true, false)
	assert_gt(buttons.size(), 30)
	for button: BaseButton in buttons:
		assert_true(UiSounds.wired(button), "%s" % button.get_path())
	# One made later has them too.
	var late := Button.new()
	late.name = "Late"
	main.add_child(late)
	assert_true(UiSounds.wired(late))
	var presses: int = Sounds.counts.get(UiSounds.PRESS, 0)
	late.pressed.emit()
	assert_eq(Sounds.counts.get(UiSounds.PRESS, 0), presses + 1)
	var hovers: int = Sounds.counts.get(UiSounds.HOVER, 0)
	late.mouse_entered.emit()
	assert_eq(Sounds.counts.get(UiSounds.HOVER, 0), hovers + 1)
	late.disabled = true
	late.mouse_entered.emit()
	assert_eq(Sounds.counts.get(UiSounds.HOVER, 0), hovers + 1, "not one that can't be pressed")
	# Wired again, it still makes one sound a press.
	UiSounds.wire(late)
	late.pressed.emit()
	assert_eq(Sounds.counts.get(UiSounds.PRESS, 0), presses + 2)


func test_a_button_that_goes_back_sounds_different() -> void:
	for name: String in ["MenuLeaveButton", "ResumeButton", "EditorDone", "DisconnectedButton", "CloseButton"]:
		var button := Button.new()
		button.name = name
		assert_eq(UiSounds.press_sound(button), UiSounds.BACK, name)
		button.free()
	for name: String in ["HostButton", "JoinButton", "Setting_quality", "MapButton"]:
		var button := Button.new()
		button.name = name
		assert_eq(UiSounds.press_sound(button), UiSounds.PRESS, name)
		button.free()
	for sound: String in UiSounds.CUES:
		assert_not_null(SoundBank.get_sound(sound), sound)
		assert_eq(Mix.bus_of(sound), Mix.INTERFACE, sound)
		assert_lt(SoundBank.seconds(SoundBank.get_sound(sound)), 0.35, "%s is short" % sound)
	assert_ne(SoundBank.get_sound(UiSounds.PRESS).data, SoundBank.get_sound(UiSounds.BACK).data)


func test_the_last_three_seconds_of_a_wait_are_counted() -> void:
	assert_false(UiSounds.counts(5.0, 4.9))
	assert_false(UiSounds.counts(3.5, 3.1), "not until a whole second is reached")
	assert_true(UiSounds.counts(3.01, 2.99), "three")
	assert_false(UiSounds.counts(2.99, 2.5))
	assert_true(UiSounds.counts(2.01, 1.99), "two")
	assert_true(UiSounds.counts(1.01, 0.99), "one")
	assert_false(UiSounds.counts(0.5, 0.0), "and nothing at nought")
	assert_false(UiSounds.counts(0.0, 3.0), "a wait beginning is not counted")
	assert_false(UiSounds.counts(2.0, 2.0))
	# A whole wait, a frame at a time, is counted three times.
	var pips := 0
	var left := 10.0
	while left > 0.0:
		var next := left - 1.0 / 60.0
		pips += 1 if UiSounds.counts(left, next) else 0
		left = next
	assert_eq(pips, UiSounds.COUNTED)


func test_a_session_that_fails_buzzes() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	await wait_process_frames(2)
	var errors: int = Sounds.counts.get(UiSounds.ERROR, 0)
	main._on_session_failed("No such session")
	assert_eq(Sounds.counts.get(UiSounds.ERROR, 0), errors + 1)
	assert_eq(Mix.duck_for(UiSounds.ERROR), Mix.DUCK_FOR_CUE, "and is not buried under the music")
