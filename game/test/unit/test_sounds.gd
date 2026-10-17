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


func test_every_take_of_every_cue_has_a_sound_a_length_and_a_level() -> void:
	for name: String in SoundBank.SOUNDS:
		assert_gte(SoundBank.takes(name), 1, name)
		for take in SoundBank.takes(name):
			var stream := SoundBank.get_sound(name, take)
			assert_not_null(stream, "%s take %d" % [name, take])
			assert_gt(SoundBank.seconds(stream), 0.04, "%s take %d has a length" % [name, take])
			assert_between(SoundBank.peak(stream), 0.1, 1.0, "%s take %d is heard, and doesn't clip" % [name, take])
			# Every take of a sound is as long and about as loud as the first.
			var first := SoundBank.get_sound(name)
			assert_almost_eq(SoundBank.seconds(stream), SoundBank.seconds(first), 0.001, name)
			assert_almost_eq(SoundBank.peak(stream), SoundBank.peak(first), 0.05, name)
	for name: String in SoundBank.TAKES:
		assert_has(SoundBank.SOUNDS, name)
		assert_gt(SoundBank.TAKES[name], 1, name)


func test_what_is_heard_over_and_over_has_several_takes_that_differ() -> void:
	for weapon: String in Weapons.ids():
		if not Weapons.find(weapon).fires_projectile():
			assert_gte(SoundBank.takes(weapon), 2, weapon)
	assert_gte(SoundBank.takes("step"), 3)
	assert_gte(SoundBank.takes("burst"), 2)
	assert_eq(SoundBank.takes("music"), 1)
	assert_eq(SoundBank.takes("kazoo"), 1)
	for name: String in SoundBank.TAKES:
		for take in range(1, SoundBank.takes(name)):
			assert_ne(SoundBank.get_sound(name, take).data, SoundBank.get_sound(name, take - 1).data, "%s takes %d and %d" % [name, take - 1, take])
		assert_same(SoundBank.get_sound(name, 1), SoundBank.get_sound(name, 1), "each made once")
	# A take there isn't is the nearest there is.
	assert_same(SoundBank.get_sound("step", 99), SoundBank.get_sound("step", SoundBank.takes("step") - 1))
	assert_same(SoundBank.get_sound("step", -3), SoundBank.get_sound("step"))


func test_the_same_take_is_never_played_twice_running() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 5
	for name: String in ["step", "grater", "rifle"]:
		var last := -1
		var heard := {}
		for play in 200:
			var take := SoundBank.pick(name, last, rng)
			assert_between(take, 0, SoundBank.takes(name) - 1)
			assert_ne(take, last, name)
			heard[take] = true
			last = take
		assert_eq(heard.size(), SoundBank.takes(name), "every take of %s gets played" % name)
	assert_eq(SoundBank.pick("hit", 0, rng), 0, "one with a single take is always that")
	# In play too.
	var before: int = Sounds.last_take.get("step", -1)
	Sounds.play("step")
	assert_ne(Sounds.last_take["step"], before)
	var then: int = Sounds.last_take["step"]
	Sounds.play("step")
	assert_ne(Sounds.last_take["step"], then)


func test_a_gun_has_its_works_clicking_after_the_shot() -> void:
	# The click is a ring of metal a moment after the bang: with it the
	# sound has more high notes just there than without.
	var with_click: AudioStreamWAV = SoundBank._gunshot(9, 0.4, {"click": 0.5, "click_at": 0.2, "tail": 0.0})
	var without: AudioStreamWAV = SoundBank._gunshot(9, 0.4, {"tail": 0.0})
	assert_gt(_level(with_click, 0.2, 0.22), _level(without, 0.2, 0.22) * 3.0)
	assert_almost_eq(_level(with_click, 0.15, 0.19), _level(without, 0.15, 0.19), 0.02, "and not before")
	for weapon: String in ["pistol", "blaster", "shotgun", "rifle", "carbine", "lmg", "grater", "pistols", "burratas"]:
		assert_not_null(SoundBank.get_sound(weapon), weapon)


func test_sounds_are_made_a_few_at_a_time_until_all_are() -> void:
	SoundBank._made.clear()
	assert_false(SoundBank.make_some(0), "none in no time")
	var rounds := 0
	while not SoundBank.make_some(10) and rounds < 2000:
		rounds += 1
	assert_gt(rounds, 2, "not all at once")
	for name: String in SoundBank.SOUNDS:
		for take in SoundBank.takes(name):
			assert_has(SoundBank._made, [name, take])
	assert_true(SoundBank.make_some(0), "nothing left to make")


func test_effects_are_short_and_the_music_is_a_long_loop() -> void:
	for name: String in SoundBank.SOUNDS:
		var stream := SoundBank.get_sound(name)
		if name.begins_with("music"):
			assert_gt(SoundBank.seconds(stream), 5.0, name)
			assert_eq(stream.loop_mode, AudioStreamWAV.LOOP_FORWARD, name)
			assert_eq(stream.loop_end, stream.data.size() / 2, name)
		elif name.begins_with("sting"):
			assert_between(SoundBank.seconds(stream), 1.0, 2.5, name)
			assert_eq(stream.loop_mode, AudioStreamWAV.LOOP_DISABLED, name)
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
	assert_eq(String(voices[0].bus), Mix.WEAPONS)


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


## How loud `stream` is between `from` and `to` seconds, as the root of the
## mean square of its samples, from 0 to 1.
func _level(stream: AudioStreamWAV, from: float, to: float) -> float:
	var first := int(from * stream.mix_rate)
	var last := mini(int(to * stream.mix_rate), stream.data.size() / 2)
	var total := 0.0
	for i in range(first, last):
		var sample := stream.data.decode_s16(i * 2) / 32767.0
		total += sample * sample
	return sqrt(total / maxi(last - first, 1))


## How much of `stream`'s sound is low: the share of its loudness left once
## each sample is averaged with those just before it, which takes the high
## notes out.
func _lowness(stream: AudioStreamWAV) -> float:
	var smooth := 0.0
	var low := 0.0
	var all := 0.0
	for i in stream.data.size() / 2:
		var sample := stream.data.decode_s16(i * 2) / 32767.0
		smooth = lerpf(smooth, sample, 0.05)
		low += smooth * smooth
		all += sample * sample
	return sqrt(low / all)


func test_a_gunshot_starts_with_a_bang_and_dies_away() -> void:
	for id: String in ["pistol", "blaster", "grater", "shotgun"]:
		var shot := SoundBank.get_sound(id)
		var bang := _level(shot, 0.0, 0.01)
		assert_gt(bang, 0.3, "%s starts loud, with no fade in" % id)
		var length := SoundBank.seconds(shot)
		assert_lt(_level(shot, length * 0.8, length), bang * 0.2, "%s has died away by its end" % id)
		assert_gt(_level(shot, length * 0.4, length * 0.6), 0.005, "%s has a tail, not just a click" % id)


func test_each_gun_sounds_like_its_size() -> void:
	var pistol := SoundBank.get_sound("pistol")
	var blaster := SoundBank.get_sound("blaster")
	var grater := SoundBank.get_sound("grater")
	var shotgun := SoundBank.get_sound("shotgun")
	assert_gt(SoundBank.seconds(shotgun), SoundBank.seconds(blaster), "the shotgun booms longest")
	assert_gt(SoundBank.seconds(blaster), SoundBank.seconds(pistol))
	assert_lt(SoundBank.seconds(grater), Weapons.find("grater").fire_interval * 3.0, "the grater's shots don't pile up into a hum")
	assert_gt(_lowness(shotgun), _lowness(blaster), "the shotgun is the deepest")
	assert_gt(_lowness(blaster), _lowness(pistol), "the pistol the sharpest")


func test_the_launcher_thumps_rather_than_cracks() -> void:
	assert_gt(_lowness(SoundBank.get_sound("launcher")), _lowness(SoundBank.get_sound("shotgun")))
	assert_gt(_lowness(SoundBank.get_sound("burst")), _lowness(SoundBank.get_sound("blaster")))
	assert_gt(SoundBank.seconds(SoundBank.get_sound("burst")), SoundBank.seconds(SoundBank.get_sound("shotgun")), "a burst rolls on")


func test_a_footstep_is_a_heel_and_then_a_sole() -> void:
	var step := SoundBank.get_sound("step")
	var heel := _level(step, 0.0, 0.02)
	var between := _level(step, 0.04, 0.054)
	var sole := _level(step, 0.056, 0.075)
	assert_gt(heel, between * 2.0, "the heel lands and fades")
	assert_gt(sole, between * 1.5, "then the sole comes down")
	assert_gt(heel, sole, "more lightly")
	assert_gt(_lowness(step), _lowness(SoundBank.get_sound("pistol")), "a dull knock, not a click")


func test_a_sound_can_be_played_a_little_higher_or_lower() -> void:
	# An odd pitch, to tell this voice from any still playing from before.
	Sounds.play("hit", 0.0, 1.23)
	var raised := Sounds._flat_voices.filter(func(flat: AudioStreamPlayer) -> bool:
		return flat.stream == SoundBank.get_sound("hit") and is_equal_approx(flat.pitch_scale, 1.23))
	assert_eq(raised.size(), 1)
	for i in 20:
		assert_between(Sounds.varied(0.1), 0.9, 1.1)


func test_a_hit_a_head_hit_and_a_knock_out_each_sound_different() -> void:
	const CUES := preload("res://audio/sound_cues.gd")
	assert_eq(CUES.hit_sound(false, false), "hit")
	assert_eq(CUES.hit_sound(false, true), "hit_head")
	assert_eq(CUES.hit_sound(true, false), "knock_out")
	assert_eq(CUES.hit_sound(true, true), "knock_out")
	var sounds := ["hit", "hit_head", "knock_out"].map(func(name: String) -> PackedByteArray: return SoundBank.get_sound(name).data)
	assert_ne(sounds[0], sounds[1])
	assert_ne(sounds[1], sounds[2])
	assert_gt(SoundBank.seconds(SoundBank.get_sound("knock_out")), SoundBank.seconds(SoundBank.get_sound("hit")), "a knock-out is the one to savour")


func test_the_shooter_hears_the_kind_of_hit_they_landed() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var before := Sounds.counts.duplicate()
	world.match_.hit_confirmed.emit(2, 20, false, false)
	world.match_.hit_confirmed.emit(2, 35, false, true)
	world.match_.hit_confirmed.emit(2, 35, true, true)
	for name: String in ["hit", "hit_head", "knock_out"]:
		assert_eq(Sounds.counts.get(name, 0), before.get(name, 0) + 1, name)
