extends GutTest
## Talking: how a voice is packed to send, who hears whom, what the host
## passes on, and muting.

const WORLD := preload("res://world/world.tscn")

var world: Node3D
var saved: bool


func before_each() -> void:
	saved = Settings.get_value(VoiceChat.SETTING)
	Settings.set_value(VoiceChat.SETTING, true)


func after_each() -> void:
	Settings.set_value(VoiceChat.SETTING, saved)


func _start(mode: String) -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	world.start(MatchSettings.from_dict({"map": "box", "mode": mode}))
	await wait_physics_frames(2)


# A note of `hertz` for `seconds`, as a microphone would give it.
func _tone(hertz: float, seconds: float, rate := 44100.0, level := 0.4) -> PackedVector2Array:
	var frames := PackedVector2Array()
	frames.resize(int(seconds * rate))
	for index in frames.size():
		var sample := sin(TAU * hertz * index / rate) * level
		frames[index] = Vector2(sample, sample)
	return frames


func _record(team: int) -> PlayerRecord:
	var record := PlayerRecord.new()
	record.team = team
	return record


func test_a_voice_is_packed_small_and_comes_back_much_as_it_was() -> void:
	var talk := _tone(300.0, VoiceCodec.PACKET_SECONDS)
	var packet := VoiceCodec.encode(talk, 44100.0)
	assert_eq(packet.size(), int(VoiceCodec.PACKET_SECONDS * VoiceCodec.RATE), "a byte for each of eight thousand samples a second")
	assert_true(VoiceCodec.is_packet(packet))
	var back := VoiceCodec.decode(packet)
	assert_eq(back.size(), packet.size())
	# The same note at the same loudness: compare with the note itself at
	# the rate it was sent at.
	var worst := 0.0
	for index in back.size():
		worst = maxf(worst, absf(back[index] - sin(TAU * 300.0 * (index + 0.5) / VoiceCodec.RATE) * 0.4))
	assert_lt(worst, 0.06)


func test_quiet_speech_keeps_its_detail() -> void:
	# Squeezing gives the quiet end more of the byte than the loud end.
	for sample: float in [0.0, 0.001, 0.01, 0.1, 0.5, 1.0, -0.01, -0.5, -1.0]:
		var back := VoiceCodec.stretch(VoiceCodec.squeeze(sample))
		assert_almost_eq(back, sample, maxf(absf(sample) * 0.04, 0.0003), "%s" % sample)
	assert_ne(VoiceCodec.squeeze(0.002), VoiceCodec.squeeze(0.004), "told apart even this quiet")
	assert_eq(VoiceCodec.squeeze(5.0), VoiceCodec.squeeze(1.0), "and too loud is only as loud as can be")
	assert_between(VoiceCodec.squeeze(-9.0), 0, 255)


func test_what_is_not_a_packet_is_known() -> void:
	assert_false(VoiceCodec.is_packet(PackedByteArray()))
	assert_false(VoiceCodec.is_packet("hello"))
	assert_false(VoiceCodec.is_packet(null))
	assert_false(VoiceCodec.is_packet([1, 2, 3]))
	var long := PackedByteArray()
	long.resize(VoiceCodec.MAX_BYTES + 1)
	assert_false(VoiceCodec.is_packet(long))
	long.resize(VoiceCodec.MAX_BYTES)
	assert_true(VoiceCodec.is_packet(long))
	assert_eq(VoiceCodec.encode(PackedVector2Array(), 44100.0).size(), 0)
	assert_eq(VoiceCodec.encode(_tone(300.0, 0.1), 0.0).size(), 0)
	assert_lte(VoiceCodec.encode(_tone(300.0, 5.0), 44100.0).size(), VoiceCodec.MAX_BYTES, "never more than a packet holds")


func test_silence_is_told_from_speech() -> void:
	assert_eq(VoiceCodec.loudness(PackedVector2Array()), 0.0)
	assert_lt(VoiceCodec.loudness(_tone(300.0, 0.06, 44100.0, 0.002)), VoiceCodec.QUIET)
	assert_gt(VoiceCodec.loudness(_tone(300.0, 0.06, 44100.0, 0.1)), VoiceCodec.QUIET)


func test_teammates_hear_each_other_and_opponents_do_not() -> void:
	var cheddar := _record(MatchState.CHEDDAR)
	assert_true(VoiceChat.hears(cheddar, _record(MatchState.CHEDDAR), true))
	assert_false(VoiceChat.hears(cheddar, _record(MatchState.BRIE), true), "nothing is given away to the other side")
	assert_false(VoiceChat.hears(cheddar, cheddar, true), "nor does anyone hear themselves")
	assert_false(VoiceChat.hears(null, cheddar, true))
	assert_false(VoiceChat.hears(cheddar, null, true))
	assert_false(VoiceChat.hears(_record(MatchState.NO_TEAM), _record(MatchState.NO_TEAM), true), "someone on no team in a team match")


func test_without_teams_everyone_hears_everyone() -> void:
	var one := _record(MatchState.NO_TEAM)
	assert_true(VoiceChat.hears(one, _record(MatchState.NO_TEAM), false))
	assert_false(VoiceChat.hears(one, one, false))


func test_the_host_passes_on_only_so_many_packets_a_second() -> void:
	assert_true(VoiceChat.allowed(0))
	assert_true(VoiceChat.allowed(VoiceChat.PACKETS_A_SECOND - 1))
	assert_false(VoiceChat.allowed(VoiceChat.PACKETS_A_SECOND))
	# Enough for talk sent as it is made, with room to spare.
	assert_gt(VoiceChat.PACKETS_A_SECOND, 1.0 / VoiceCodec.PACKET_SECONDS)


func test_talk_is_sent_a_packet_at_a_time_and_silence_is_not_sent() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var chat: VoiceChat = world.voice_chat
	chat.speak(_tone(300.0, VoiceCodec.PACKET_SECONDS * 0.5), 44100.0)
	assert_eq(chat.sent, 0, "not until there is a packet's worth")
	chat.speak(_tone(300.0, VoiceCodec.PACKET_SECONDS * 2.5), 44100.0)
	assert_eq(chat.sent, 3)
	chat.speak(_tone(300.0, VoiceCodec.PACKET_SECONDS * 3.0, 44100.0, 0.001), 44100.0)
	assert_eq(chat.sent, 3, "silence stays here")


func test_the_host_passes_talk_to_who_should_hear_and_nobody_else() -> void:
	await _start(MatchSettings.TEAMS)
	var match_: Match = world.match_
	var own_team: int = match_.own_record().team
	var other_team := MatchState.BRIE if own_team == MatchState.CHEDDAR else MatchState.CHEDDAR
	match_.add_player(5)
	match_.state.player(5).team = own_team
	match_.add_player(6)
	match_.state.player(6).team = other_team
	watch_signals(match_)
	var packet := VoiceCodec.encode(_tone(300.0, VoiceCodec.PACKET_SECONDS), 44100.0)
	# A teammate talks: the host, on their team, hears.
	match_._pass_on_talk(5, packet)
	assert_signal_emit_count(match_, "voice_heard", 1)
	assert_signal_emitted_with_parameters(match_, "voice_heard", [5, packet])
	# An opponent talks: the host doesn't.
	match_._pass_on_talk(6, packet)
	assert_signal_emit_count(match_, "voice_heard", 1)
	# The host's own talk isn't played back to them.
	match_.say(packet)
	assert_signal_emit_count(match_, "voice_heard", 1)
	# Nor anything from someone who isn't in the session, or that isn't talk.
	match_._pass_on_talk(77, packet)
	match_._pass_on_talk(5, "hello")
	match_._pass_on_talk(5, PackedByteArray())
	assert_signal_emit_count(match_, "voice_heard", 1)


func test_a_flood_of_packets_from_one_player_is_cut_off_and_let_back() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var match_: Match = world.match_
	match_.add_player(5)
	watch_signals(match_)
	var packet := VoiceCodec.encode(_tone(300.0, VoiceCodec.PACKET_SECONDS), 44100.0)
	for sending in 200:
		match_._pass_on_talk(5, packet)
	assert_signal_emit_count(match_, "voice_heard", VoiceChat.PACKETS_A_SECOND)
	# A second on, they are heard again.
	match_._clock += 1.1
	match_._pass_on_talk(5, packet)
	assert_signal_emit_count(match_, "voice_heard", VoiceChat.PACKETS_A_SECOND + 1)


func test_talk_that_arrives_is_played_and_its_talker_named() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var chat: VoiceChat = world.voice_chat
	world.match_.add_player(5)
	var packet := VoiceCodec.encode(_tone(300.0, VoiceCodec.PACKET_SECONDS), 44100.0)
	assert_eq(chat.talking(), [])
	world.match_.voice_heard.emit(5, packet)
	assert_eq(chat.heard, 1)
	assert_eq(chat.talking(), [5])
	var player: AudioStreamPlayer = chat._speakers[5]
	assert_true(player.playing)
	assert_eq(String(player.bus), Mix.VOICES, "under the voices volume")
	assert_eq((player.stream as AudioStreamGenerator).mix_rate, float(VoiceCodec.RATE))
	# A moment after their last packet they are no longer talking.
	chat._process(VoiceChat.TALKING_FOR + 0.05)
	assert_eq(chat.talking(), [])
	# What isn't talk is not played.
	chat.hear(5, PackedByteArray())
	assert_eq(chat.heard, 1)


func test_a_muted_player_is_not_heard_until_unmuted() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var chat: VoiceChat = world.voice_chat
	var packet := VoiceCodec.encode(_tone(300.0, VoiceCodec.PACKET_SECONDS), 44100.0)
	chat.hear(5, packet)
	chat.set_muted(5, true)
	assert_true(chat.is_muted(5))
	assert_false(chat._speakers.has(5), "and what they were saying is cut off")
	chat.hear(5, packet)
	chat.hear(6, packet)
	assert_eq(chat.heard, 2, "the one from before, and the other player's")
	assert_eq(chat.talking(), [5, 6].filter(func(id: int) -> bool: return id != 5 or chat._clock - chat._last_heard[5] <= VoiceChat.TALKING_FOR))
	chat.set_muted(5, false)
	assert_false(chat.is_muted(5))
	chat.hear(5, packet)
	assert_eq(chat.heard, 3)


func test_with_talking_turned_off_nothing_is_heard_or_sent() -> void:
	await _start(MatchSettings.FREE_FOR_ALL)
	var chat: VoiceChat = world.voice_chat
	assert_eq(Settings.DEFAULTS[VoiceChat.SETTING], true)
	Settings.set_value(VoiceChat.SETTING, false)
	chat.hear(5, VoiceCodec.encode(_tone(300.0, VoiceCodec.PACKET_SECONDS), 44100.0))
	assert_eq(chat.heard, 0)
	assert_false(chat.can_talk())
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	assert_not_null(menu.find_child("Setting_" + VoiceChat.SETTING, true, false))


func test_there_is_a_key_to_hold_that_can_be_changed() -> void:
	assert_true(InputMap.has_action(VoiceChat.ACTION))
	assert_has(Controls.REBINDABLE, VoiceChat.ACTION)
	# Its key is nobody else's.
	var key: int = (InputMap.action_get_events(VoiceChat.ACTION)[0] as InputEventKey).physical_keycode
	for action: String in Controls.REBINDABLE:
		if action == VoiceChat.ACTION:
			continue
		for event in InputMap.action_get_events(action):
			if event is InputEventKey:
				assert_ne(event.physical_keycode, key, action)


func test_nobody_talks_alone_or_out_of_a_session() -> void:
	world = WORLD.instantiate()
	add_child_autofree(world)
	assert_false(world.voice_chat.can_talk(), "on the menu")
