extends GutTest

const WORLD := preload("res://world/world.tscn")


func test_every_sound_is_made_and_is_audible_without_clipping() -> void:
	for name: String in SoundBank.SOUNDS:
		var stream := SoundBank.get_sound(name)
		assert_not_null(stream, name)
		assert_eq(stream.format, AudioStreamWAV.FORMAT_16_BITS, name)
		assert_false(stream.stereo, name)
		var loudest := SoundBank.peak(stream)
		assert_gt(loudest, 0.1, "%s is too quiet" % name)
		assert_lte(loudest, 1.0, name)


func test_effects_are_short_and_the_music_is_a_long_loop() -> void:
	for name: String in SoundBank.SOUNDS:
		var stream := SoundBank.get_sound(name)
		if name == "music":
			assert_gt(SoundBank.seconds(stream), 5.0)
			assert_eq(stream.loop_mode, AudioStreamWAV.LOOP_FORWARD)
			assert_eq(stream.loop_end, stream.data.size() / 2)
		else:
			assert_between(SoundBank.seconds(stream), 0.04, 1.0, name)
			assert_eq(stream.loop_mode, AudioStreamWAV.LOOP_DISABLED, name)


func test_a_sound_is_made_once_and_is_always_the_same() -> void:
	assert_same(SoundBank.get_sound("hit"), SoundBank.get_sound("hit"))
	var again: AudioStreamWAV = SoundBank._blaster()
	assert_eq(again.data, SoundBank.get_sound("blaster").data)


func test_there_is_a_sound_for_every_weapon() -> void:
	for id: String in Weapons.ids():
		assert_not_null(SoundBank.get_sound(id), id)


func test_an_unknown_sound_is_nothing() -> void:
	assert_null(SoundBank.get_sound("kazoo"))
	Sounds.play("kazoo")
	assert_false(Sounds.counts.has("kazoo"))


func test_sounds_start_and_are_counted() -> void:
	var before: int = Sounds.counts.get("hit", 0)
	Sounds.play("hit")
	assert_eq(Sounds.counts.get("hit", 0), before + 1)


func test_arena_sounds_play_where_they_happen() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	watch_signals(Sounds)
	Sounds.play_at("burst", Vector3(3, 1, 2), world)
	assert_signal_emitted_with_parameters(Sounds, "played", ["burst", Vector3(3, 1, 2)])
	var voices := world.find_children("*", "AudioStreamPlayer3D", false, false)
	assert_eq(voices.size(), 1)
	assert_eq(voices[0].global_position, Vector3(3, 1, 2))
	assert_eq(voices[0].bus, Sounds.EFFECTS_BUS)


func test_the_arena_never_has_too_many_sounds_at_once() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	for i in Sounds.MAX_VOICES + 10:
		Sounds.play_at("step", Vector3(i, 0, 0), world)
	assert_lte(world.find_children("*", "AudioStreamPlayer3D", false, false).size(), Sounds.MAX_VOICES)


func test_the_buses_exist() -> void:
	assert_ne(AudioServer.get_bus_index(Sounds.MUSIC_BUS), -1)
	assert_ne(AudioServer.get_bus_index(Sounds.EFFECTS_BUS), -1)


func test_the_music_plays_and_quietens_in_a_session() -> void:
	Sounds.play_music()
	assert_true(Sounds.is_music_playing())
	Sounds.set_music_quiet(true)
	assert_lt(Sounds._music.volume_db, -10.0)
	Sounds.set_music_quiet(false)
