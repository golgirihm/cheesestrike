extends GutTest
## The characters' voices: what triggers each, and who hears it.

const WORLD := preload("res://world/world.tscn")
const OWN := Footsteps.OWN
const MATE := Footsteps.TEAMMATE
const ENEMY := Footsteps.OPPONENT

var world: Node3D


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


func _heard(sound: String) -> int:
	return Sounds.counts.get(sound, 0)


func test_each_thing_a_character_does_has_its_voice() -> void:
	assert_eq(Voices.cue("jump"), Voices.EFFORT)
	assert_eq(Voices.cue("throw"), Voices.EFFORT)
	assert_eq(Voices.cue("strike"), Voices.EFFORT)
	assert_eq(Voices.cue("hurt"), Voices.PAIN)
	assert_eq(Voices.cue("emote"), Voices.TAUNT)
	assert_eq(Voices.cue("reload"), Voices.RELOAD)
	assert_eq(Voices.cue("knocked_out"), Voices.OUT)
	assert_eq(Voices.cue("yawn"), "")
	for event: String in Voices.EVENTS:
		assert_has(Voices.CUES, Voices.EVENTS[event])
	for sound: String in Voices.CUES:
		assert_not_null(SoundBank.get_sound(sound), sound)
		assert_eq(Mix.bus_of(sound), Mix.VOICES, sound)
		assert_has(Voices.LEVELS, sound)
		assert_lt(SoundBank.seconds(SoundBank.get_sound(sound)), 0.6, "%s is short" % sound)
	# What others say is captioned; a grunt of effort is not worth a line.
	for sound: String in [Voices.PAIN, Voices.TAUNT, Voices.RELOAD, Voices.OUT]:
		assert_has(preload("res://ui/captions.gd").TEXTS, sound)


func test_a_reload_is_called_to_teammates_and_nobody_else() -> void:
	assert_true(Voices.heard(Voices.RELOAD, MATE))
	assert_false(Voices.heard(Voices.RELOAD, ENEMY), "an opponent must not be told")
	assert_false(Voices.heard(Voices.RELOAD, OWN))


func test_everyone_hears_effort_and_a_taunt_and_others_hear_pain() -> void:
	for who: String in [OWN, MATE, ENEMY]:
		assert_true(Voices.heard(Voices.EFFORT, who))
		assert_true(Voices.heard(Voices.TAUNT, who))
	for sound: String in [Voices.PAIN, Voices.OUT]:
		assert_true(Voices.heard(sound, MATE))
		assert_true(Voices.heard(sound, ENEMY))
		assert_false(Voices.heard(sound, OWN), "a player's own has a sound already")
	assert_false(Voices.heard("kazoo", ENEMY))


func test_a_players_own_voice_is_quieter_than_anothers() -> void:
	for sound: String in Voices.CUES:
		assert_almost_eq(Voices.level(sound, OWN), Voices.level(sound, ENEMY) + Voices.OWN_DB, 0.001)
		assert_eq(Voices.level(sound, MATE), Voices.level(sound, ENEMY))
	assert_lt(Voices.level(Voices.EFFORT, ENEMY), Voices.level(Voices.PAIN, ENEMY), "a grunt of effort is the quietest")


func test_each_character_has_a_pitch_of_its_own() -> void:
	var pitches := {}
	for id in range(1, 40):
		var pitch := Voices.pitch(id * 7919)
		assert_between(pitch, 0.8, 1.3)
		assert_eq(Voices.pitch(id * 7919), pitch, "the same every time")
		pitches[snappedf(pitch, 0.01)] = true
	assert_gt(pitches.size(), 20, "spread about, not all alike")
	assert_between(Voices.pitch(-3), 0.8, 1.3, "a bot has one too")
	assert_ne(Voices.pitch(1), Voices.pitch(2))


func test_a_character_does_not_squeak_again_at_once_but_is_always_heard_knocked_out() -> void:
	assert_true(Voices.ready(Voices.PAIN, INF))
	assert_false(Voices.ready(Voices.PAIN, 0.1))
	assert_false(Voices.ready(Voices.EFFORT, Voices.GAP - 0.01))
	assert_true(Voices.ready(Voices.EFFORT, Voices.GAP))
	assert_true(Voices.ready(Voices.OUT, 0.0))


func test_voices_differ_from_one_another() -> void:
	var seen := []
	for sound: String in Voices.CUES:
		var data := SoundBank.get_sound(sound).data
		assert_does_not_have(seen, data, sound)
		seen.append(data)
	assert_gt(SoundBank.takes(Voices.PAIN), 1, "pain is heard often: it has takes")
	assert_gt(SoundBank.seconds(SoundBank.get_sound(Voices.OUT)), SoundBank.seconds(SoundBank.get_sound(Voices.PAIN)), "a knock-out is the long squeal")


func test_being_hurt_is_heard_from_others_and_not_from_oneself() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var other := _add(5, MatchState.NO_TEAM, Vector3(4, 0.1, 0))
	var before := _heard(Voices.PAIN)
	world.match_.state.player(5).health = 60
	other._on_match_state_changed()
	assert_eq(_heard(Voices.PAIN), before + 1)
	# Hit again at once: no second squeak.
	world.match_.state.player(5).health = 40
	other._on_match_state_changed()
	assert_eq(_heard(Voices.PAIN), before + 1)
	other._process(Voices.GAP + 0.05)
	world.match_.state.player(5).health = 20
	other._on_match_state_changed()
	assert_eq(_heard(Voices.PAIN), before + 2)
	# Oneself: the thud one already hears, and no squeak.
	world.match_.state.player(1).health = 50
	world.player_node(1)._on_match_state_changed()
	assert_eq(_heard(Voices.PAIN), before + 2)


func test_a_knock_out_is_heard_even_just_after_a_squeak_of_pain() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var other := _add(5, MatchState.NO_TEAM, Vector3(4, 0.1, 0))
	var before := _heard(Voices.OUT)
	world.match_.state.player(5).health = 10
	other._on_match_state_changed()
	world.match_.state.player(5).health = 0
	world.match_.state.player(5).alive = false
	other._on_match_state_changed()
	assert_eq(_heard(Voices.OUT), before + 1)


func test_a_throw_a_strike_and_an_emote_are_heard_from_whoever_makes_them() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var other := _add(5, MatchState.NO_TEAM, Vector3(4, 0.1, 0))
	var effort := _heard(Voices.EFFORT)
	world.match_.throw_shown.emit(5)
	assert_eq(_heard(Voices.EFFORT), effort + 1)
	other._process(Voices.GAP + 0.05)
	world.match_.strike_shown.emit(5)
	assert_eq(_heard(Voices.EFFORT), effort + 2)
	other._process(Voices.GAP + 0.05)
	var taunts := _heard(Voices.TAUNT)
	world.match_.emote_shown.emit(5, Emotes.WAVE)
	assert_eq(_heard(Voices.TAUNT), taunts + 1)
	# At the character, at their own pitch.
	var voice: AudioStreamPlayer3D = world.find_children("*", "AudioStreamPlayer3D", false, false).filter(func(played: AudioStreamPlayer3D) -> bool: return played.stream == SoundBank.get_sound(Voices.TAUNT))[0]
	assert_almost_eq(voice.global_position, other.head.global_position, Vector3.ONE * 0.01)
	assert_almost_eq(voice.pitch_scale, Voices.pitch(5), 0.05)
	assert_eq(String(voice.bus), Mix.VOICES)
	# One's own are heard too.
	world.match_.throw_shown.emit(1)
	assert_eq(_heard(Voices.EFFORT), effort + 3)


func test_a_teammate_is_heard_reloading_and_an_opponent_is_not() -> void:
	await _start(MatchSettings.TEAMS)
	var own_team: int = world.match_.own_record().team
	var other_team := MatchState.BRIE if own_team == MatchState.CHEDDAR else MatchState.CHEDDAR
	var mate := _add(5, own_team, Vector3(4, 0.1, 0))
	var enemy := _add(6, other_team, Vector3(-4, 0.1, 0))
	var before := _heard(Voices.RELOAD)
	enemy.voice("reload")
	assert_eq(_heard(Voices.RELOAD), before, "not a sound from the other side")
	mate.voice("reload")
	assert_eq(_heard(Voices.RELOAD), before + 1)
	world.player_node(1).voice("reload")
	assert_eq(_heard(Voices.RELOAD), before + 1, "nor from oneself")


func test_in_a_free_for_all_nobody_is_heard_reloading() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var other := _add(5, MatchState.NO_TEAM, Vector3(4, 0.1, 0))
	var before := _heard(Voices.RELOAD)
	other.voice("reload")
	assert_eq(_heard(Voices.RELOAD), before)
