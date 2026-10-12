class_name SoundBank
## Every sound in the game, made in code rather than recorded: noise, tones and
## sweeps shaped by envelopes. Each is made once, the first time it is wanted,
## and always comes out the same.

const RATE := 22050
const MUSIC_RATE := 16000

## Every sound by name, with what makes it.
const SOUNDS := {
	"pistol": "_pistol",
	"blaster": "_blaster",
	"shotgun": "_shotgun",
	"grater": "_grater",
	"launcher": "_launcher",
	"burst": "_burst",
	"hit": "_hit",
	"hurt": "_hurt",
	"knocked_out": "_knocked_out",
	"step": "_step",
	"jump": "_jump",
	"pickup": "_pickup",
	"overload": "_overload",
	"reload": "_reload",
	"music": "_music",
}

static var _made := {}


## The sound called `name`, or null if there is none.
static func get_sound(name: String) -> AudioStreamWAV:
	if not SOUNDS.has(name):
		return null
	if not _made.has(name):
		_made[name] = Callable(SoundBank, SOUNDS[name]).call()
	return _made[name]


## The loudest sample in `stream`, from 0 to 1.
static func peak(stream: AudioStreamWAV) -> float:
	var loudest := 0
	var data := stream.data
	for i in range(0, data.size() - 1, 2):
		loudest = maxi(loudest, absi(data.decode_s16(i)))
	return loudest / 32767.0


static func seconds(stream: AudioStreamWAV) -> float:
	return stream.data.size() / 2.0 / stream.mix_rate


# Turns samples from -1 to 1 into a stream.
static func _stream(samples: PackedFloat32Array, rate := RATE, loops := false) -> AudioStreamWAV:
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		data.encode_s16(i * 2, int(clampf(samples[i], -1.0, 1.0) * 32767.0))
	var stream := AudioStreamWAV.new()
	stream.format = AudioStreamWAV.FORMAT_16_BITS
	stream.mix_rate = rate
	stream.stereo = false
	stream.data = data
	if loops:
		stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
		stream.loop_begin = 0
		stream.loop_end = samples.size()
	return stream


static func _rng(seed: int) -> RandomNumberGenerator:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	return rng


# A sound of `length` seconds, each sample from `shape`, a Callable(t, i)
# giving the sample at time t, then faded in and out by a hair to avoid clicks.
static func _make(length: float, shape: Callable, volume := 0.8) -> AudioStreamWAV:
	var count := int(length * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	for i in count:
		samples[i] = shape.call(float(i) / RATE, i) * volume
	var edge := mini(64, count / 4)
	for i in edge:
		samples[i] *= float(i) / edge
		samples[count - 1 - i] *= float(i) / edge
	return _stream(samples)


static func _decay(t: float, speed: float) -> float:
	return exp(-t * speed)


static func _blaster() -> AudioStreamWAV:
	var rng := _rng(1)
	return _make(0.16, func(t: float, _i: int) -> float:
		var tone := signf(sin(TAU * (900.0 - 2500.0 * t) * t)) * 0.4
		return (tone + rng.randf_range(-1.0, 1.0) * 0.6) * _decay(t, 28.0))


static func _pistol() -> AudioStreamWAV:
	var rng := _rng(9)
	return _make(0.12, func(t: float, _i: int) -> float:
		return rng.randf_range(-1.0, 1.0) * _decay(t, 40.0), 0.7)


static func _shotgun() -> AudioStreamWAV:
	var rng := _rng(2)
	var low := 0.0
	return _make(0.4, func(t: float, _i: int) -> float:
		low = lerpf(low, rng.randf_range(-1.0, 1.0), 0.25)
		return (low * 1.6 + sin(TAU * 70.0 * t) * 0.5) * _decay(t, 9.0), 0.9)


static func _grater() -> AudioStreamWAV:
	var rng := _rng(3)
	return _make(0.06, func(t: float, _i: int) -> float:
		return (rng.randf_range(-1.0, 1.0) * 0.7 + signf(sin(TAU * 2200.0 * t)) * 0.3) * _decay(t, 60.0), 0.6)


static func _launcher() -> AudioStreamWAV:
	var rng := _rng(4)
	return _make(0.3, func(t: float, _i: int) -> float:
		var frequency := lerpf(180.0, 60.0, t / 0.3)
		return (sin(TAU * frequency * t) + rng.randf_range(-1.0, 1.0) * 0.25) * _decay(t, 10.0))


static func _burst() -> AudioStreamWAV:
	var rng := _rng(5)
	var low := 0.0
	return _make(0.9, func(t: float, _i: int) -> float:
		low = lerpf(low, rng.randf_range(-1.0, 1.0), 0.08)
		return (low * 3.0 + sin(TAU * 45.0 * t) * 0.6) * _decay(t, 4.5), 0.9)


static func _hit() -> AudioStreamWAV:
	return _make(0.07, func(t: float, _i: int) -> float:
		return sin(TAU * 1500.0 * t) * _decay(t, 45.0), 0.6)


static func _hurt() -> AudioStreamWAV:
	var rng := _rng(6)
	return _make(0.18, func(t: float, _i: int) -> float:
		return (sin(TAU * 140.0 * t) * 0.7 + rng.randf_range(-1.0, 1.0) * 0.3) * _decay(t, 16.0))


static func _knocked_out() -> AudioStreamWAV:
	return _make(0.45, func(t: float, _i: int) -> float:
		var frequency := lerpf(620.0, 180.0, t / 0.45)
		return signf(sin(TAU * frequency * t)) * 0.5 * _decay(t, 4.0), 0.6)


static func _step() -> AudioStreamWAV:
	var rng := _rng(7)
	var low := 0.0
	return _make(0.07, func(t: float, _i: int) -> float:
		low = lerpf(low, rng.randf_range(-1.0, 1.0), 0.3)
		return low * 2.0 * _decay(t, 50.0), 0.5)


static func _jump() -> AudioStreamWAV:
	return _make(0.12, func(t: float, _i: int) -> float:
		return sin(TAU * lerpf(300.0, 600.0, t / 0.12) * t) * _decay(t, 15.0), 0.4)


static func _pickup() -> AudioStreamWAV:
	var notes := [523.25, 659.25, 783.99]
	return _make(0.3, func(t: float, _i: int) -> float:
		var note: float = notes[mini(int(t / 0.1), 2)]
		return sin(TAU * note * t) * _decay(fmod(t, 0.1), 12.0), 0.5)


static func _overload() -> AudioStreamWAV:
	return _make(0.7, func(t: float, _i: int) -> float:
		var frequency := lerpf(200.0, 820.0, t / 0.7)
		var wobble := 0.6 + 0.4 * sin(TAU * 18.0 * t)
		return sin(TAU * frequency * t) * wobble * minf(t * 10.0, 1.0) * _decay(maxf(t - 0.5, 0.0), 12.0), 0.6)


static func _reload() -> AudioStreamWAV:
	var rng := _rng(8)
	return _make(0.3, func(t: float, _i: int) -> float:
		var click := 0.0
		for at in [0.0, 0.2]:
			if t >= at:
				click += rng.randf_range(-1.0, 1.0) * _decay(t - at, 70.0)
		return click, 0.5)


## A cheerful loop for the menu: a bass line and a tune, four bars long.
static func _music() -> AudioStreamWAV:
	var beat := 60.0 / 120.0
	# Each note: [start in beats, length in beats, frequency].
	var bass := []
	var roots := [130.81, 174.61, 196.0, 130.81]
	for bar in 4:
		for step in 4:
			bass.append([bar * 4 + step, 0.9, roots[bar] if step % 2 == 0 else roots[bar] * 1.5])
	var tune := []
	var melody := [523.25, 587.33, 659.25, 783.99, 659.25, 587.33, 523.25, 440.0,
		698.46, 659.25, 587.33, 523.25, 587.33, 659.25, 587.33, 523.25]
	for i in melody.size():
		tune.append([i, 0.8, melody[i]])
	var count := int(16 * beat * MUSIC_RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	for note: Array in bass:
		_add_note(samples, note[0] * beat, note[1] * beat, note[2], 0.28, true)
	for note: Array in tune:
		_add_note(samples, note[0] * beat, note[1] * beat, note[2], 0.18, false)
	return _stream(samples, MUSIC_RATE, true)


static func _add_note(samples: PackedFloat32Array, start: float, length: float, frequency: float, volume: float, triangle: bool) -> void:
	var first := int(start * MUSIC_RATE)
	var last := mini(int((start + length) * MUSIC_RATE), samples.size())
	for i in range(first, last):
		var t := float(i - first) / MUSIC_RATE
		var phase := fmod(frequency * t, 1.0)
		var wave := (4.0 * absf(phase - 0.5) - 1.0) if triangle else sin(TAU * phase) * 0.7 + sin(TAU * phase * 2.0) * 0.3
		var envelope := minf(t * 40.0, 1.0) * exp(-t * 2.5)
		samples[i] += wave * envelope * volume
