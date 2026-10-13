extends Node
## Plays the game's sounds: effects in the arena, placed where they happen;
## interface sounds; and the music. Browsers only let a page make sound once
## the player has clicked or tapped it, so nothing is heard before that.

## A sound was played, for captions: its name, and where in the arena, or
## null for one that isn't anywhere in particular.
signal played(name: String, at: Variant)

const MUSIC_BUS := Mix.MUSIC
const EFFECTS_BUS := Mix.EFFECTS
## Each volume setting, and the bus it sets.
const VOLUME_SETTINGS := Mix.VOLUMES
## Most sounds in the arena at once; beyond that the oldest is cut short.
const MAX_VOICES := 24

## How many times each sound has been played, for tests.
var counts := {}


## A pitch a little above or below true, by up to `by`: no two footsteps or
## shots sound quite alike.
static func varied(by := 0.06) -> float:
	return randf_range(1.0 - by, 1.0 + by)

## The take of each sound that was played last.
var last_take := {}
var _ducker := Mix.Ducker.new()
var _dip := 0.0
var _music: AudioStreamPlayer
var _voices: Array[AudioStreamPlayer3D] = []
var _flat_voices: Array[AudioStreamPlayer] = []


## Bus loudness, in decibels, for a setting from 0 (silent) to 1.
static func volume_db(level: float) -> float:
	return Mix.volume_db(level)


## Sets each bus to the player's chosen loudness, less what it is dipped by.
func apply_volumes() -> void:
	for setting: String in VOLUME_SETTINGS:
		var name: String = VOLUME_SETTINGS[setting]
		var bus := AudioServer.get_bus_index(name)
		if bus != -1:
			AudioServer.set_bus_volume_db(bus, volume_db(Settings.get_value(setting)) + (_dip if Mix.DUCKED.has(name) else 0.0))


## How far, in decibels, the music and the world are dipped just now.
func dip() -> float:
	return _dip


func _ready() -> void:
	Mix.build()
	apply_volumes()
	Mix.apply_master(Settings.get_value(Mix.RANGE_SETTING), Settings.get_value(Mix.MONO_SETTING))
	Settings.changed.connect(func(key: String) -> void:
		if VOLUME_SETTINGS.has(key):
			apply_volumes()
		elif key == Mix.RANGE_SETTING or key == Mix.MONO_SETTING:
			Mix.apply_master(Settings.get_value(Mix.RANGE_SETTING), Settings.get_value(Mix.MONO_SETTING)))
	_music = AudioStreamPlayer.new()
	_music.bus = MUSIC_BUS
	_music.volume_db = -8.0
	add_child(_music)


## Plays `name` at `at` in `world`'s 3D space. `pitch` plays it higher (above
## 1) or lower, so that a sound heard over and over isn't the same each time.
func play_at(name: String, at: Vector3, world: Node3D, volume_db := 0.0, pitch := 1.0) -> void:
	var stream := _next_take(name)
	if stream == null or world == null or not world.is_inside_tree():
		return
	var voice := _free_voice(world)
	voice.bus = Mix.bus_of(name)
	voice.stream = stream
	voice.volume_db = volume_db
	voice.pitch_scale = pitch
	voice.global_position = at
	voice.play()
	_count(name, at)


## Plays `name` without a place, as for the player's own footsteps or the
## sound of their hits landing.
func play(name: String, volume_db := 0.0, pitch := 1.0) -> void:
	var stream := _next_take(name)
	if stream == null:
		return
	var voice: AudioStreamPlayer = null
	for flat in _flat_voices:
		if not flat.playing:
			voice = flat
			break
	if voice == null:
		voice = AudioStreamPlayer.new()
		add_child(voice)
		_flat_voices.append(voice)
	voice.bus = Mix.bus_of(name)
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
	_ducker.hit(Mix.duck_for(name))
	played.emit(name, at)


# A take of `name` other than the one played last.
func _next_take(name: String) -> AudioStreamWAV:
	var take := SoundBank.pick(name, last_take.get(name, -1))
	var stream := SoundBank.get_sound(name, take)
	if stream != null:
		last_take[name] = take
	return stream


func _process(delta: float) -> void:
	var dip := _ducker.tick(delta)
	if not is_equal_approx(dip, _dip):
		_dip = dip
		apply_volumes()


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
	voice.unit_size = 6.0
	voice.max_distance = 60.0
	world.add_child(voice)
	_voices.append(voice)
	return voice
