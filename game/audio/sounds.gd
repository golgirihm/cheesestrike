extends Node
## Plays the game's sounds: effects in the arena, placed where they happen;
## interface sounds; and the music. Browsers only let a page make sound once
## the player has clicked or tapped it, so nothing is heard before that.

## A sound was played, for captions: its name, and where in the arena, or
## null for one that isn't anywhere in particular.
signal played(name: String, at: Variant)

const MUSIC_BUS := "Music"
const EFFECTS_BUS := "Effects"
## Each volume setting, and the bus it sets.
const VOLUME_SETTINGS := {"master_volume": "Master", "music_volume": MUSIC_BUS, "effects_volume": EFFECTS_BUS}
## Most sounds in the arena at once; beyond that the oldest is cut short.
const MAX_VOICES := 24

## How many times each sound has been played, for tests.
var counts := {}


## A pitch a little above or below true, by up to `by`: no two footsteps or
## shots sound quite alike.
static func varied(by := 0.06) -> float:
	return randf_range(1.0 - by, 1.0 + by)

var _music: AudioStreamPlayer
var _voices: Array[AudioStreamPlayer3D] = []
var _flat_voices: Array[AudioStreamPlayer] = []


## Bus loudness, in decibels, for a setting from 0 (silent) to 1.
static func volume_db(level: float) -> float:
	return -80.0 if level <= 0.001 else linear_to_db(level)


## Sets each bus to the player's chosen loudness.
func apply_volumes() -> void:
	for setting: String in VOLUME_SETTINGS:
		var bus := AudioServer.get_bus_index(VOLUME_SETTINGS[setting])
		if bus != -1:
			AudioServer.set_bus_volume_db(bus, volume_db(Settings.get_value(setting)))


func _ready() -> void:
	apply_volumes()
	Settings.changed.connect(func(key: String) -> void:
		if VOLUME_SETTINGS.has(key):
			apply_volumes())
	_music = AudioStreamPlayer.new()
	_music.bus = MUSIC_BUS
	_music.volume_db = -8.0
	add_child(_music)


## Plays `name` at `at` in `world`'s 3D space. `pitch` plays it higher (above
## 1) or lower, so that a sound heard over and over isn't the same each time.
func play_at(name: String, at: Vector3, world: Node3D, volume_db := 0.0, pitch := 1.0) -> void:
	var stream := SoundBank.get_sound(name)
	if stream == null or world == null or not world.is_inside_tree():
		return
	var voice := _free_voice(world)
	voice.stream = stream
	voice.volume_db = volume_db
	voice.pitch_scale = pitch
	voice.global_position = at
	voice.play()
	_count(name, at)


## Plays `name` without a place, as for the player's own footsteps or the
## sound of their hits landing.
func play(name: String, volume_db := 0.0, pitch := 1.0) -> void:
	var stream := SoundBank.get_sound(name)
	if stream == null:
		return
	var voice: AudioStreamPlayer = null
	for flat in _flat_voices:
		if not flat.playing:
			voice = flat
			break
	if voice == null:
		voice = AudioStreamPlayer.new()
		voice.bus = EFFECTS_BUS
		add_child(voice)
		_flat_voices.append(voice)
	voice.stream = stream
	voice.volume_db = volume_db
	voice.pitch_scale = pitch
	voice.play()
	_count(name, null)


func play_music() -> void:
	if not _music.playing:
		_music.stream = SoundBank.get_sound("music")
		_music.play()


func set_music_quiet(quiet: bool) -> void:
	_music.volume_db = -20.0 if quiet else -8.0


func is_music_playing() -> bool:
	return _music.playing


func _count(name: String, at: Variant) -> void:
	counts[name] = counts.get(name, 0) + 1
	played.emit(name, at)


func _free_voice(world: Node3D) -> AudioStreamPlayer3D:
	# Voices go with the arena they were in when it is replaced.
	_voices.assign(_voices.filter(func(voice: Variant) -> bool: return is_instance_valid(voice) and not voice.is_queued_for_deletion()))
	for voice in _voices:
		if not voice.playing and voice.get_parent() == world:
			return voice
	if _voices.size() >= MAX_VOICES:
		var oldest: AudioStreamPlayer3D = _voices.pop_front()
		oldest.stop()
		if oldest.get_parent() == world:
			_voices.append(oldest)
			return oldest
		oldest.queue_free()
	var voice := AudioStreamPlayer3D.new()
	voice.bus = EFFECTS_BUS
	voice.unit_size = 6.0
	voice.max_distance = 60.0
	world.add_child(voice)
	_voices.append(voice)
	return voice
