class_name VoiceChat
extends Node
## Talking: a player holds a key and their teammates hear them. The voice
## goes over the connections the game already has: from the talker to the
## host, which passes it on to whoever should hear it.
##
## In a match with teams that is the talker's team. Where there are none it
## is everyone, as there is no side to give anything away to. Anyone can be
## muted, on the machine of whoever mutes them, and a setting turns the
## whole thing off.

const SETTING := "voice_chat"
const ACTION := "talk"
const MIC_BUS := "Mic"
## The most packets a second the host passes on from one player.
const PACKETS_A_SECOND := 25
## Seconds after their last packet that a player still counts as talking.
const TALKING_FOR := 0.4

var world: Node3D
## The players this machine's player has muted, by id.
var muted := {}
## How many packets have been sent and heard, for tests.
var sent := 0
var heard := 0
# When each player was last heard, by a clock of this node's own.
var _last_heard := {}
var _clock := 0.0
var _mic: AudioStreamPlayer
var _capture: AudioEffectCapture
var _pending := PackedVector2Array()
# Each talker's player: {id: AudioStreamPlayer}.
var _speakers := {}


func _init(of: Node3D = null) -> void:
	world = of
	name = "VoiceChat"


func _ready() -> void:
	if world != null:
		world.match_.voice_heard.connect(hear)


## Whether the player with record `listener` hears the one with record
## `speaker` talk, in a match with `teams` or without.
static func hears(listener: PlayerRecord, speaker: PlayerRecord, teams: bool) -> bool:
	if listener == null or speaker == null or listener == speaker:
		return false
	if not teams:
		return true
	return listener.team != MatchState.NO_TEAM and listener.team == speaker.team


## Whether the host passes on a packet from a player who has sent `lately`
## packets in the last second.
static func allowed(lately: int) -> bool:
	return lately < PACKETS_A_SECOND


## Mutes player `id`, or lets them be heard again.
func set_muted(id: int, on: bool) -> void:
	if on:
		muted[id] = true
		_drop_speaker(id)
	else:
		muted.erase(id)


func is_muted(id: int) -> bool:
	return muted.has(id)


## The players heard talking just now, by id.
func talking() -> Array[int]:
	var ids: Array[int] = []
	for id: int in _last_heard:
		if _clock - _last_heard[id] <= TALKING_FOR:
			ids.append(id)
	ids.sort()
	return ids


## A packet of player `speaker`'s talk has arrived: it is played, unless
## they are muted or talking is turned off.
func hear(speaker: int, packet: PackedByteArray) -> void:
	if not Settings.get_value(SETTING) or is_muted(speaker) or not VoiceCodec.is_packet(packet):
		return
	heard += 1
	_last_heard[speaker] = _clock
	if not _speakers.has(speaker):
		var player := AudioStreamPlayer.new()
		var stream := AudioStreamGenerator.new()
		stream.mix_rate = VoiceCodec.RATE
		stream.buffer_length = 0.5
		player.stream = stream
		player.bus = Mix.VOICES
		add_child(player)
		player.play()
		_speakers[speaker] = player
	var playback: AudioStreamGeneratorPlayback = _speakers[speaker].get_stream_playback()
	if playback == null:
		return
	for sample in VoiceCodec.decode(packet):
		# Talk that arrives faster than it is played is let go, not queued.
		if playback.get_frames_available() <= 0:
			break
		playback.push_frame(Vector2(sample, sample))


## What was picked up by the microphone, as `frames` at `rate` samples a
## second: sent a packet at a time, unless it is silence.
func speak(frames: PackedVector2Array, rate: float) -> void:
	_pending.append_array(frames)
	var packet := int(VoiceCodec.PACKET_SECONDS * rate)
	while packet > 0 and _pending.size() >= packet:
		var piece := _pending.slice(0, packet)
		_pending = _pending.slice(packet)
		if VoiceCodec.loudness(piece) >= VoiceCodec.QUIET:
			world.match_.say(VoiceCodec.encode(piece, rate))
			sent += 1


## Whether the player may talk just now: talking is wanted, and they are in
## a session with someone else to hear.
func can_talk() -> bool:
	return Settings.get_value(SETTING) and world != null and multiplayer.multiplayer_peer != null and world.match_.own_record() != null and not Net.alone


func _process(delta: float) -> void:
	_clock += delta
	# Whoever has left is forgotten.
	for id: int in _speakers.keys():
		if _clock - _last_heard.get(id, -INF) > 5.0:
			_drop_speaker(id)
	var wanted := can_talk() and Input.is_action_pressed(ACTION)
	if not wanted:
		if _mic != null and _mic.playing:
			_mic.stop()
			_pending.clear()
		return
	_listen()
	if _capture != null and _capture.get_frames_available() > 0:
		speak(_capture.get_buffer(_capture.get_frames_available()), AudioServer.get_mix_rate())


# Starts the microphone, which is when the player is asked to allow it.
func _listen() -> void:
	if _mic == null:
		var bus := AudioServer.get_bus_index(MIC_BUS)
		if bus == -1:
			AudioServer.add_bus()
			bus = AudioServer.bus_count - 1
			AudioServer.set_bus_name(bus, MIC_BUS)
			# Heard by nobody here: only picked up, to be sent.
			AudioServer.set_bus_volume_db(bus, -80.0)
			AudioServer.add_bus_effect(bus, AudioEffectCapture.new())
		_capture = AudioServer.get_bus_effect(bus, 0)
		_mic = AudioStreamPlayer.new()
		_mic.stream = AudioStreamMicrophone.new()
		_mic.bus = MIC_BUS
		add_child(_mic)
	if not _mic.playing:
		_capture.clear_buffer()
		_mic.play()


func _drop_speaker(id: int) -> void:
	if _speakers.has(id):
		_speakers[id].queue_free()
		_speakers.erase(id)
