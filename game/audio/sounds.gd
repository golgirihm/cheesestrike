extends Node
## Plays the game's sounds: effects in the arena, placed where they happen;
## interface sounds; and the music. Browsers only let a page make sound once
## the player has clicked or tapped it, so nothing is heard before that.

## A sound was played, for captions: its name, and where in the arena, or
## null for one that isn't anywhere in particular.
signal played(name: String, at: Variant)

const MUSIC_BUS := "Music"
const EFFECTS_BUS := "Effects"
## Most sounds in the arena at once; beyond that the oldest is cut short.
const MAX_VOICES := 24

## How many times each sound has been played, for tests.
var counts := {}

var _music: AudioStreamPlayer
var _voices: Array[AudioStreamPlayer3D] = []
var _flat_voices: Array[AudioStreamPlayer] = []


func _ready() -> void:
	_music = AudioStreamPlayer.new()
	_music.bus = MUSIC_BUS
	_music.volume_db = -8.0
	add_child(_music)


## Plays `name` at `at` in `world`'s 3D space.
func play_at(name: String, at: Vector3, world: Node3D, volume_db := 0.0) -> void:
	var stream := SoundBank.get_sound(name)
	if stream == null or world == null or not world.is_inside_tree():
		return
	var voice := _free_voice(world)
	voice.stream = stream
	voice.volume_db = volume_db
	voice.global_position = at
	voice.play()
	_count(name, at)


## Plays `name` without a place, as for the player's own footsteps or the
## sound of their hits landing.
func play(name: String, volume_db := 0.0) -> void:
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
