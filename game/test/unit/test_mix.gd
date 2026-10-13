extends GutTest
## The mix: which bus each sound goes through, the music and the world
## dipping under gunfire and important cues, and the sound settings.

const WORLD := preload("res://world/world.tscn")
const SETTINGS_MENU := preload("res://ui/settings_menu.gd")

var saved := {}


func before_each() -> void:
	for key: String in Mix.VOLUMES.keys() + [Mix.RANGE_SETTING, Mix.MONO_SETTING, SoundCues.HIT_SETTING]:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _bus_db(bus: String) -> float:
	return AudioServer.get_bus_volume_db(AudioServer.get_bus_index(bus))


func _master_effect(kind: Variant) -> Array:
	var master := AudioServer.get_bus_index(Mix.MASTER)
	for index in AudioServer.get_bus_effect_count(master):
		if is_instance_of(AudioServer.get_bus_effect(master, index), kind):
			return [AudioServer.get_bus_effect(master, index), AudioServer.is_bus_effect_enabled(master, index)]
	return [null, false]


func test_there_is_a_bus_for_each_kind_of_sound_sent_where_it_should_be() -> void:
	for bus: String in [Mix.MUSIC, Mix.EFFECTS, Mix.WEAPONS, Mix.STEPS, Mix.VOICES, Mix.WORLD, Mix.INTERFACE]:
		assert_ne(AudioServer.get_bus_index(bus), -1, bus)
	for bus: String in Mix.SENDS:
		assert_eq(String(AudioServer.get_bus_send(AudioServer.get_bus_index(bus))), Mix.SENDS[bus], bus)
	# Weapons, steps, voices and the world are all effects; the interface is
	# not, so turning effects down leaves hit sounds and announcements.
	for bus: String in [Mix.WEAPONS, Mix.STEPS, Mix.VOICES, Mix.WORLD]:
		assert_eq(Mix.SENDS[bus], Mix.EFFECTS)
	assert_eq(Mix.SENDS[Mix.INTERFACE], Mix.MASTER)
	# Building them again adds nothing.
	var count := AudioServer.bus_count
	Mix.build()
	assert_eq(AudioServer.bus_count, count)


func test_every_sound_has_a_bus() -> void:
	for cue: String in SoundBank.SOUNDS:
		assert_ne(AudioServer.get_bus_index(Mix.bus_of(cue)), -1, cue)
	for weapon: String in Weapons.ids():
		assert_eq(Mix.bus_of(weapon), Mix.WEAPONS, weapon)
	assert_eq(Mix.bus_of("burst"), Mix.WEAPONS)
	assert_eq(Mix.bus_of("reload"), Mix.WEAPONS)
	assert_eq(Mix.bus_of("step"), Mix.STEPS)
	assert_eq(Mix.bus_of("land"), Mix.STEPS)
	assert_eq(Mix.bus_of("hurt"), Mix.VOICES)
	assert_eq(Mix.bus_of("pickup"), Mix.WORLD)
	assert_eq(Mix.bus_of("gnaw"), Mix.WORLD)
	assert_eq(Mix.bus_of("hit"), Mix.INTERFACE)
	assert_eq(Mix.bus_of("objective"), Mix.INTERFACE)
	assert_eq(Mix.bus_of("music"), Mix.MUSIC)
	for cue: String in Mix.BUS_OF:
		assert_has(SoundBank.SOUNDS, cue, "a bus for a sound there is")


func test_a_sound_plays_through_its_bus() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	Sounds.play_at("step", Vector3.ZERO, world)
	Sounds.play_at("blaster", Vector3.ZERO, world)
	var buses: Array = world.find_children("*", "AudioStreamPlayer3D", false, false).map(func(voice: AudioStreamPlayer3D) -> String: return String(voice.bus))
	assert_has(buses, Mix.STEPS)
	assert_has(buses, Mix.WEAPONS)
	Sounds.play("hit")
	assert_true(Sounds._flat_voices.any(func(voice: AudioStreamPlayer) -> bool: return String(voice.bus) == Mix.INTERFACE and voice.stream == SoundBank.get_sound("hit")))


func test_which_sounds_the_music_and_the_world_dip_for() -> void:
	for weapon: String in Weapons.ids():
		assert_eq(Mix.duck_for(weapon), Mix.DUCK_FOR_SHOT, weapon)
	assert_eq(Mix.duck_for("burst"), Mix.DUCK_FOR_SHOT)
	for cue: String in Mix.CUES:
		assert_eq(Mix.duck_for(cue), Mix.DUCK_FOR_CUE, cue)
		assert_has(SoundBank.SOUNDS, cue)
	for cue: String in ["step", "jump", "pickup", "hit", "reload", "music", "kazoo"]:
		assert_eq(Mix.duck_for(cue), 0.0, cue)
	assert_lt(Mix.DUCK_FOR_CUE, Mix.DUCK_FOR_SHOT, "a cue that matters dips them further than a shot")
	assert_does_not_have(Mix.DUCKED, Mix.WEAPONS)
	assert_does_not_have(Mix.DUCKED, Mix.INTERFACE)


func test_a_dip_arrives_fast_holds_and_eases_away() -> void:
	var ducker := Mix.Ducker.new()
	assert_eq(ducker.tick(0.016), 0.0)
	ducker.hit(0.0)
	assert_eq(ducker.tick(0.016), 0.0, "nothing to dip for")
	ducker.hit(Mix.DUCK_FOR_SHOT)
	var after_attack := 0.0
	for step in 4:
		after_attack = ducker.tick(Mix.ATTACK / 4.0)
	assert_almost_eq(after_attack, Mix.DUCK_FOR_SHOT, 0.01, "down within the attack")
	assert_almost_eq(ducker.tick(Mix.HOLD * 0.5), Mix.DUCK_FOR_SHOT, 0.01, "held")
	ducker.tick(Mix.HOLD * 0.6)
	var easing := ducker.tick(Mix.RELEASE * 0.2)
	assert_between(easing, Mix.DUCK_FOR_SHOT + 0.1, -0.1)
	for step in 20:
		ducker.tick(Mix.RELEASE / 10.0)
	assert_eq(ducker.tick(0.016), 0.0, "and back")


func test_a_burst_of_fire_holds_the_dip_and_a_cue_deepens_it() -> void:
	var ducker := Mix.Ducker.new()
	var lowest := 0.0
	var highest_while_firing := -100.0
	for shot in 20:
		ducker.hit(Mix.DUCK_FOR_SHOT)
		for step in 5:
			var level := ducker.tick(0.02)
			lowest = minf(lowest, level)
			if shot > 0:
				highest_while_firing = maxf(highest_while_firing, level)
	assert_almost_eq(lowest, Mix.DUCK_FOR_SHOT, 0.01, "no deeper for many shots than for one")
	assert_almost_eq(highest_while_firing, Mix.DUCK_FOR_SHOT, 0.01, "and it doesn't pump between them")
	ducker.hit(Mix.DUCK_FOR_CUE)
	for step in 5:
		lowest = minf(lowest, ducker.tick(0.02))
	assert_almost_eq(lowest, Mix.DUCK_FOR_CUE, 0.01)
	# A shot during a cue's dip doesn't lift it.
	ducker.hit(Mix.DUCK_FOR_SHOT)
	assert_almost_eq(ducker.tick(0.02), Mix.DUCK_FOR_CUE, 0.01)


func test_gunfire_dips_the_music_and_the_world_and_nothing_else() -> void:
	# Once any dip from before has gone.
	for step in 30:
		Sounds._process(0.1)
	var music := _bus_db(Mix.MUSIC)
	var arena := _bus_db(Mix.WORLD)
	var weapons := _bus_db(Mix.WEAPONS)
	Sounds.play("blaster")
	Sounds._process(Mix.ATTACK)
	assert_almost_eq(Sounds.dip(), Mix.DUCK_FOR_SHOT, 0.01)
	assert_almost_eq(_bus_db(Mix.MUSIC), music + Mix.DUCK_FOR_SHOT, 0.01)
	assert_almost_eq(_bus_db(Mix.WORLD), arena + Mix.DUCK_FOR_SHOT, 0.01)
	assert_almost_eq(_bus_db(Mix.WEAPONS), weapons, 0.01)
	# A volume changed during a dip keeps the dip.
	Settings.set_value("music_volume", 0.3)
	assert_almost_eq(_bus_db(Mix.MUSIC), Mix.volume_db(0.3) + Mix.DUCK_FOR_SHOT, 0.01)
	for step in 30:
		Sounds._process(0.1)
	assert_eq(Sounds.dip(), 0.0)
	assert_almost_eq(_bus_db(Mix.MUSIC), Mix.volume_db(0.3), 0.01)
	assert_almost_eq(_bus_db(Mix.WORLD), arena, 0.01)


func test_each_bus_has_a_volume_that_is_kept_in_bounds_and_takes_effect() -> void:
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	for setting: String in Mix.VOLUMES:
		assert_has(Settings.DEFAULTS, setting)
		assert_eq(Settings.RANGES[setting], [0.0, 1.0], setting)
		assert_not_null(menu.find_child("Setting_" + setting, true, false), "a slider for %s" % setting)
		Settings.set_value(setting, 0.25)
		assert_almost_eq(_bus_db(Mix.VOLUMES[setting]), Mix.volume_db(0.25), 0.01, setting)
		Settings.set_value(setting, 7.0)
		assert_eq(Settings.get_value(setting), 1.0, setting)
		Settings.set_value(setting, 0.0)
		assert_eq(_bus_db(Mix.VOLUMES[setting]), -80.0, "%s at nothing is silent" % setting)
	assert_eq(Mix.VOLUMES.size(), 8, "the master, the music, the effects, and five kinds under them")


func test_the_dynamic_range_presses_the_loud_end_down_more_for_speakers_and_most_at_night() -> void:
	assert_eq(Settings.DEFAULTS[Mix.RANGE_SETTING], "headphones")
	assert_eq(Settings.CHOICES[Mix.RANGE_SETTING], Mix.RANGES.keys())
	assert_lt(Mix.RANGES.speakers.threshold, Mix.RANGES.headphones.threshold)
	assert_lt(Mix.RANGES.night.threshold, Mix.RANGES.speakers.threshold)
	assert_gt(Mix.RANGES.night.ratio, Mix.RANGES.speakers.ratio)
	assert_gt(Mix.RANGES.night.gain, Mix.RANGES.speakers.gain, "and brings the quiet end up")
	Settings.set_value(Mix.RANGE_SETTING, "headphones")
	assert_false(_master_effect(AudioEffectCompressor)[1], "left alone for headphones")
	Settings.set_value(Mix.RANGE_SETTING, "night")
	var pressed := _master_effect(AudioEffectCompressor)
	assert_true(pressed[1])
	assert_eq(pressed[0].ratio, Mix.RANGES.night.ratio)
	assert_eq(pressed[0].threshold, Mix.RANGES.night.threshold)
	Settings.set_value(Mix.RANGE_SETTING, "speakers")
	assert_eq(_master_effect(AudioEffectCompressor)[0].ratio, Mix.RANGES.speakers.ratio)
	# Only ever one of it on the bus, however often it is changed.
	var master := AudioServer.get_bus_index(Mix.MASTER)
	var count := 0
	for index in AudioServer.get_bus_effect_count(master):
		count += 1 if AudioServer.get_bus_effect(master, index) is AudioEffectCompressor else 0
	assert_eq(count, 1)
	Settings.set_value(Mix.RANGE_SETTING, "cinema")
	assert_push_error("can't be cinema")


func test_mono_makes_left_and_right_the_same() -> void:
	assert_eq(Settings.DEFAULTS[Mix.MONO_SETTING], false)
	assert_false(_master_effect(AudioEffectStereoEnhance)[1])
	Settings.set_value(Mix.MONO_SETTING, true)
	var narrow := _master_effect(AudioEffectStereoEnhance)
	assert_true(narrow[1])
	assert_eq(narrow[0].pan_pullout, 0.0)
	Settings.set_value(Mix.MONO_SETTING, false)
	assert_false(_master_effect(AudioEffectStereoEnhance)[1])


func test_the_hit_sound_is_the_players_choice() -> void:
	assert_eq(Settings.CHOICES[SoundCues.HIT_SETTING], SoundCues.HIT_SOUNDS.keys())
	assert_eq(SoundCues.hit_sound(false, false), "hit")
	assert_eq(SoundCues.hit_sound(false, false, "tick"), "hit")
	assert_eq(SoundCues.hit_sound(false, false, "pop"), "hit_pop")
	assert_eq(SoundCues.hit_sound(false, false, "bell"), "hit_bell")
	assert_eq(SoundCues.hit_sound(false, false, "nonsense"), "hit")
	# The head and a knock-out keep their own, whichever is chosen.
	for chosen: String in ["tick", "pop", "bell"]:
		assert_eq(SoundCues.hit_sound(false, true, chosen), "hit_head")
		assert_eq(SoundCues.hit_sound(true, false, chosen), "knock_out")
		assert_not_null(SoundBank.get_sound(SoundCues.hit_sound(false, false, chosen)), chosen)
	for knocked_out: bool in [true, false]:
		for head: bool in [true, false]:
			assert_eq(SoundCues.hit_sound(knocked_out, head, "off"), "", "off is off")
	# The three are told apart: by how long they ring.
	var lengths := ["hit", "hit_pop", "hit_bell"].map(func(cue: String) -> float: return SoundBank.seconds(SoundBank.get_sound(cue)))
	assert_lt(lengths[0], lengths[1])
	assert_lt(lengths[1], lengths[2])


func test_a_hit_plays_what_was_chosen_or_nothing() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	Settings.set_value(SoundCues.HIT_SETTING, "bell")
	var bells: int = Sounds.counts.get("hit_bell", 0)
	world.match_.hit_confirmed.emit(2, 10, false, false)
	assert_eq(Sounds.counts.get("hit_bell", 0), bells + 1)
	Settings.set_value(SoundCues.HIT_SETTING, "off")
	var all := Sounds.counts.duplicate()
	world.match_.hit_confirmed.emit(2, 10, false, true)
	world.match_.hit_confirmed.emit(2, 10, true, false)
	assert_eq(Sounds.counts, all, "nothing at all")


func test_the_settings_screen_has_the_sound_choices() -> void:
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	var range_button: Button = menu.find_child("Setting_" + Mix.RANGE_SETTING, true, false)
	assert_eq(range_button.text, "Sound for: Headphones")
	range_button.pressed.emit()
	assert_eq(Settings.get_value(Mix.RANGE_SETTING), "speakers")
	assert_eq(range_button.text, "Sound for: Speakers")
	var hit_button: Button = menu.find_child("Setting_" + SoundCues.HIT_SETTING, true, false)
	assert_eq(hit_button.text, "Hit sound: Tick")
	for press in SoundCues.HIT_SOUNDS.size():
		hit_button.pressed.emit()
	assert_eq(Settings.get_value(SoundCues.HIT_SETTING), "tick", "round and back")
	assert_not_null(menu.find_child("Setting_" + Mix.MONO_SETTING, true, false))
	for choice: Array in SETTINGS_MENU.SOUND_CHOICES:
		assert_eq(choice[2].keys(), Settings.CHOICES[choice[0]], "%s: a name for every choice" % choice[0])
