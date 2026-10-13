class_name Mix
## The mix: which bus each sound goes through, how loud each bus is, and how
## the music and the world dip under gunfire and the cues a player must not
## miss.
##
## Weapons, steps, voices and the world all pass through the effects bus, so
## the "Effects" volume still turns them all down together; the interface
## and the music go straight to the master.

const MASTER := "Master"
const MUSIC := "Music"
const EFFECTS := "Effects"
const WEAPONS := "Weapons"
const STEPS := "Steps"
const VOICES := "Voices"
const WORLD := "World"
const INTERFACE := "Interface"
## The buses under the old two, and where each sends its sound.
const SENDS := {WEAPONS: EFFECTS, STEPS: EFFECTS, VOICES: EFFECTS, WORLD: EFFECTS, INTERFACE: MASTER}
## The setting that holds each bus's volume, from 0 to 1.
const VOLUMES := {"master_volume": MASTER, "music_volume": MUSIC, "effects_volume": EFFECTS, "weapons_volume": WEAPONS,
	"steps_volume": STEPS, "voices_volume": VOICES, "world_volume": WORLD, "interface_volume": INTERFACE}

## The kinds of sound that the place they are made in changes, and how each
## kind of place does: how big the room sounds, how much of what comes back
## is heard, and how dull it is (see Acoustics). Each has a bus of its own
## under its kind's, named for both, such as "WeaponsIndoors".
const PLACED := [WEAPONS, STEPS]
const ROOMS := {
	"Indoors": {"room_size": 0.45, "wet": 0.3, "damping": 0.55},
	"Alley": {"room_size": 0.8, "wet": 0.2, "damping": 0.35},
}

## The bus of each sound that isn't a weapon's shot or part of the world.
const BUS_OF := {
	"burst": WEAPONS, "stick": WEAPONS, "reload": WEAPONS, "dry": WEAPONS, "knife": WEAPONS, "throw": WEAPONS,
	"step": STEPS, "jump": STEPS, "land": STEPS, "slide": STEPS,
	"step_rock": STEPS, "step_grass": STEPS, "step_wood": STEPS, "step_metal": STEPS, "step_cheese": STEPS,
	"hurt": VOICES, "knocked_out": VOICES, "emote": VOICES,
	"hit": INTERFACE, "hit_pop": INTERFACE, "hit_bell": INTERFACE, "hit_head": INTERFACE, "knock_out": INTERFACE, "objective": INTERFACE,
	"music": MUSIC,
}

## The buses that dip, how far in decibels under a shot and under a cue that
## matters, and the cues that matter.
const DUCKED := [MUSIC, WORLD]
const DUCK_FOR_SHOT := -4.0
const DUCK_FOR_CUE := -8.0
const CUES := ["objective", "power", "knock_out", "knocked_out", "overload"]
## Seconds a dip takes to arrive, is held for, and takes to go.
const ATTACK := 0.04
const HOLD := 0.25
const RELEASE := 0.7

## How wide the difference between loud and quiet is left: the setting, its
## choices with what each is called, and for each how the loud end is
## pressed down (above `threshold` decibels, by `ratio`) and the whole
## brought up (`gain`).
const RANGE_SETTING := "dynamic_range"
const RANGES := {
	"headphones": {"name": "Headphones", "threshold": 0.0, "ratio": 1.0, "gain": 0.0},
	"speakers": {"name": "Speakers", "threshold": -16.0, "ratio": 3.0, "gain": 2.0},
	"night": {"name": "Night", "threshold": -28.0, "ratio": 8.0, "gain": 6.0},
}
const MONO_SETTING := "mono_sound"


## The bus the sound called `cue` plays through.
static func bus_of(cue: String) -> String:
	if BUS_OF.has(cue):
		return BUS_OF[cue]
	return WEAPONS if Weapons.find(cue) != null else WORLD


## How far, in decibels, the music and the world dip when `cue` plays: 0
## for one they don't dip for.
static func duck_for(cue: String) -> float:
	if CUES.has(cue):
		return DUCK_FOR_CUE
	if cue == "burst" or Weapons.find(cue) != null:
		return DUCK_FOR_SHOT
	return 0.0


## Bus loudness, in decibels, for a setting from 0 (silent) to 1.
static func volume_db(level: float) -> float:
	return -80.0 if level <= 0.001 else linear_to_db(level)


## Makes sure every bus exists and sends its sound where it should.
static func build() -> void:
	for bus: String in SENDS:
		var index := AudioServer.get_bus_index(bus)
		if index == -1:
			AudioServer.add_bus()
			index = AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, bus)
		AudioServer.set_bus_send(index, SENDS[bus])
	for kind: String in PLACED:
		for place: String in ROOMS:
			var name := kind + place
			if AudioServer.get_bus_index(name) != -1:
				continue
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, name)
			AudioServer.set_bus_send(index, kind)
			var room := AudioEffectReverb.new()
			room.room_size = ROOMS[place].room_size
			room.wet = ROOMS[place].wet
			room.damping = ROOMS[place].damping
			room.dry = 1.0
			AudioServer.add_bus_effect(index, room)


## The bus a sound on `bus` goes through when it is made in a place of kind
## `place`: its own, or the one that makes it sound of that place.
static func treated(bus: String, place: String) -> String:
	return bus + place if PLACED.has(bus) and ROOMS.has(place) else bus


## Puts the dynamic range `choice` and whether the sound is `mono` into
## effect on the master bus.
static func apply_master(choice: String, mono: bool) -> void:
	var range: Dictionary = RANGES.get(choice, RANGES.headphones)
	var master := AudioServer.get_bus_index(MASTER)
	var press: AudioEffectCompressor = null
	var narrow: AudioEffectStereoEnhance = null
	for index in AudioServer.get_bus_effect_count(master):
		var effect := AudioServer.get_bus_effect(master, index)
		if effect is AudioEffectCompressor:
			press = effect
		elif effect is AudioEffectStereoEnhance:
			narrow = effect
	if press == null:
		press = AudioEffectCompressor.new()
		AudioServer.add_bus_effect(master, press)
	if narrow == null:
		narrow = AudioEffectStereoEnhance.new()
		AudioServer.add_bus_effect(master, narrow)
	press.threshold = range.threshold
	press.ratio = range.ratio
	press.gain = range.gain
	# Pulled all the way in, left and right are the same.
	narrow.pan_pullout = 0.0 if mono else 1.0
	for index in AudioServer.get_bus_effect_count(master):
		var effect := AudioServer.get_bus_effect(master, index)
		if effect == press:
			AudioServer.set_bus_effect_enabled(master, index, range.ratio > 1.0)
		elif effect == narrow:
			AudioServer.set_bus_effect_enabled(master, index, mono)


## The dip itself: told of each sound that calls for one, and asked each
## frame how far down the dipped buses are.
class Ducker:
	var _level := 0.0
	var _target := 0.0
	var _hold := 0.0

	## A sound that calls for a dip of `decibels` (0 or below) has played.
	func hit(decibels: float) -> void:
		if decibels >= 0.0:
			return
		_target = minf(_target, decibels) if _hold > 0.0 else decibels
		_hold = Mix.HOLD

	## Moves on by `delta` seconds; returns how far down the buses are, in
	## decibels: 0 when there is nothing to dip for.
	func tick(delta: float) -> float:
		if _hold > 0.0:
			_hold -= delta
			_level = move_toward(_level, _target, absf(Mix.DUCK_FOR_CUE) / Mix.ATTACK * delta)
		else:
			_target = 0.0
			_level = move_toward(_level, 0.0, absf(Mix.DUCK_FOR_CUE) / Mix.RELEASE * delta)
		return _level
