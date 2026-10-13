class_name SoundBank
## Every sound in the game, made in code rather than recorded: noise, tones and
## sweeps shaped by envelopes, put together the way the real sound is. A
## gunshot is a crack, a blast, a thump and a tail (see _gunshot); a footstep
## is a heel and then a sole. Each is made once, the first time it is wanted,
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
	"hit_head": "_hit_head",
	"knock_out": "_knock_out",
	"hurt": "_hurt",
	"knocked_out": "_knocked_out",
	"step": "_step",
	"jump": "_jump",
	"land": "_land",
	"slide": "_slide",
	"pad": "_pad",
	"pickup": "_pickup",
	"overload": "_overload",
	"reload": "_reload",
	"dry": "_dry",
	"knife": "_knife",
	"throw": "_knife",
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


# A gunshot, as heard from beside it. A real one is four things on top of
# each other, and `spec` says how much of each this gun has:
#
#   crack   the snap as the shot leaves: a few thousandths of a second of
#           bright noise
#   blast   the muzzle blast: duller noise, over in a tenth of a second
#   thump   the push of air, felt as much as heard: a low note falling from
#           `thump_from` to `thump_to` hertz
#   tail    the sound coming back off everything around: a longer rumble
#
# all pushed hard enough to flatten the loudest part, as a loud bang does
# to the ear.
static func _gunshot(seed: int, length: float, spec: Dictionary) -> AudioStreamWAV:
	var rng := _rng(seed)
	var count := int(length * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var blast := 0.0
	var tail := 0.0
	var turn := 0.0
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		# Noise with its high end taken off, more of it for the tail.
		blast = lerpf(blast, noise, spec.get("blast_bright", 0.35))
		tail = lerpf(tail, noise, spec.get("tail_bright", 0.06))
		var hertz := lerpf(spec.get("thump_to", 50.0), spec.get("thump_from", 150.0), _decay(t, spec.get("thump_fall", 30.0)))
		turn += hertz / RATE
		var sample: float = noise * spec.get("crack", 1.0) * _decay(t, spec.get("crack_decay", 900.0))
		sample += blast * spec.get("blast", 1.0) * _decay(t, spec.get("blast_decay", 45.0))
		sample += sin(TAU * turn) * spec.get("thump", 0.8) * _decay(t, spec.get("thump_decay", 28.0))
		# The tail swells for a moment as the first echoes arrive, then dies.
		sample += tail * spec.get("tail", 1.5) * minf(t * 60.0, 1.0) * _decay(t, spec.get("tail_decay", 10.0))
		samples[i] = tanh(sample * spec.get("drive", 2.2))
	return _stream(_shaped(samples, spec.get("volume", 0.9)))


# `samples` made as loud as `peak`, and faded out at the very end so that it
# doesn't stop with a click.
static func _shaped(samples: PackedFloat32Array, peak: float) -> PackedFloat32Array:
	var loudest := 0.0001
	for sample in samples:
		loudest = maxf(loudest, absf(sample))
	var fade := mini(RATE / 50, samples.size() / 4)
	for i in samples.size():
		samples[i] *= peak / loudest * minf(float(samples.size() - 1 - i) / fade, 1.0)
	return samples


# A pistol: mostly snap, with little behind it.
static func _pistol() -> AudioStreamWAV:
	return _gunshot(9, 0.32, {"crack": 1.2, "crack_decay": 700.0, "blast": 0.9, "blast_bright": 0.5, "blast_decay": 70.0,
		"thump": 0.5, "thump_from": 210.0, "thump_to": 90.0, "thump_decay": 45.0, "tail": 0.8, "tail_bright": 0.12, "tail_decay": 18.0, "volume": 0.75})


# A rifle: a hard crack with weight under it, and a tail that carries.
static func _blaster() -> AudioStreamWAV:
	return _gunshot(1, 0.42, {"crack": 1.3, "crack_decay": 600.0, "blast": 1.2, "blast_bright": 0.4, "blast_decay": 48.0,
		"thump": 0.9, "thump_from": 170.0, "thump_to": 60.0, "thump_decay": 30.0, "tail": 1.6, "tail_bright": 0.08, "tail_decay": 11.0, "volume": 0.85})


# The grater fires so fast that each shot has to be short, or they smear
# into a hum.
static func _grater() -> AudioStreamWAV:
	return _gunshot(3, 0.15, {"crack": 1.1, "crack_decay": 900.0, "blast": 1.0, "blast_bright": 0.55, "blast_decay": 95.0,
		"thump": 0.5, "thump_from": 240.0, "thump_to": 110.0, "thump_decay": 70.0, "tail": 0.6, "tail_bright": 0.15, "tail_decay": 34.0, "volume": 0.65})


# A shotgun: a boom more than a crack, and a long roll after it.
static func _shotgun() -> AudioStreamWAV:
	return _gunshot(2, 0.75, {"crack": 0.9, "crack_decay": 450.0, "blast": 1.6, "blast_bright": 0.22, "blast_decay": 24.0,
		"thump": 1.3, "thump_from": 120.0, "thump_to": 42.0, "thump_decay": 16.0, "thump_fall": 20.0, "tail": 2.4, "tail_bright": 0.05, "tail_decay": 6.5, "drive": 2.8, "volume": 0.95})


# The launcher has no crack at all: a hollow thump from a wide tube.
static func _launcher() -> AudioStreamWAV:
	var rng := _rng(4)
	var count := int(0.4 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var puff := 0.0
	var turn := 0.0
	for i in count:
		var t := float(i) / RATE
		puff = lerpf(puff, rng.randf_range(-1.0, 1.0), 0.12)
		turn += lerpf(55.0, 150.0, _decay(t, 22.0)) / RATE
		# The low note of the charge, the air it pushes out, and the tube ringing.
		samples[i] = tanh((sin(TAU * turn) * _decay(t, 14.0) + puff * 2.2 * _decay(t, 26.0) + sin(TAU * 330.0 * t) * 0.25 * _decay(t, 40.0)) * 1.6)
	return _stream(_shaped(samples, 0.9))


# A burst: a crack, then a deep boom that rolls on.
static func _burst() -> AudioStreamWAV:
	return _gunshot(5, 0.98, {"crack": 1.0, "crack_decay": 300.0, "blast": 1.8, "blast_bright": 0.16, "blast_decay": 14.0,
		"thump": 1.6, "thump_from": 90.0, "thump_to": 30.0, "thump_decay": 7.0, "thump_fall": 9.0, "tail": 3.0, "tail_bright": 0.035, "tail_decay": 3.8, "drive": 3.0, "volume": 0.95})


# A footstep on hard ground: the heel coming down, a dull knock, and a
# moment later the sole, a lighter scuff with grit in it.
static func _step() -> AudioStreamWAV:
	var rng := _rng(7)
	var count := int(0.17 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var dull := 0.0
	var soft := 0.0
	var turn := 0.0
	var sole := 0.055
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		dull = lerpf(dull, noise, 0.1)
		soft = lerpf(soft, noise, 0.4)
		turn += lerpf(55.0, 120.0, _decay(t, 60.0)) / RATE
		var heel := (dull * 2.6 + sin(TAU * turn) * 0.9) * _decay(t, 65.0)
		# What is left of the noise once its low end is taken away: grit.
		var scuff := (soft - dull) * 1.4 * _decay(t - sole, 80.0) if t >= sole else 0.0
		samples[i] = tanh(heel + scuff)
	return _stream(_shaped(samples, 0.6))


# A reload: the magazine out, a new one in, and the bolt run forward. Each
# is a click with the ring of metal in it.
static func _reload() -> AudioStreamWAV:
	var rng := _rng(8)
	# [when, how loud, the note it rings at].
	var clicks := [[0.0, 0.7, 2100.0], [0.2, 1.0, 1500.0], [0.34, 0.8, 2600.0], [0.37, 0.6, 1900.0]]
	return _make(0.5, func(t: float, _i: int) -> float:
		var sample := 0.0
		var noise := rng.randf_range(-1.0, 1.0)
		for click: Array in clicks:
			if t >= click[0]:
				var since: float = t - click[0]
				sample += (noise * _decay(since, 220.0) + sin(TAU * click[2] * since) * 0.5 * _decay(since, 70.0)) * click[1]
		return sample, 0.5)


# The click of a trigger pulled on nothing.
static func _dry() -> AudioStreamWAV:
	var rng := _rng(11)
	return _make(0.06, func(t: float, _i: int) -> float:
		return rng.randf_range(-1.0, 1.0) * _decay(t, 260.0) + sin(TAU * 2400.0 * t) * 0.4 * _decay(t, 90.0), 0.4)


# A knife through the air: a breath of noise that rises and is gone.
static func _knife() -> AudioStreamWAV:
	var rng := _rng(12)
	var count := int(0.16 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var soft := 0.0
	var softer := 0.0
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		# Brighter as the swing speeds up.
		soft = lerpf(soft, noise, lerpf(0.15, 0.7, t / 0.16))
		softer = lerpf(softer, noise, 0.05)
		samples[i] = (soft - softer) * sin(PI * t / 0.16)
	return _stream(_shaped(samples, 0.5))


# Landing from a fall: both feet at once, heavier than a step.
static func _land() -> AudioStreamWAV:
	var rng := _rng(13)
	var count := int(0.22 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var dull := 0.0
	var turn := 0.0
	for i in count:
		var t := float(i) / RATE
		dull = lerpf(dull, rng.randf_range(-1.0, 1.0), 0.07)
		turn += lerpf(45.0, 100.0, _decay(t, 40.0)) / RATE
		samples[i] = tanh((dull * 3.0 + sin(TAU * turn) * 1.2) * _decay(t, 28.0))
	return _stream(_shaped(samples, 0.7))


# Sliding across the ground: a rush of grit that dies away.
static func _slide() -> AudioStreamWAV:
	var rng := _rng(14)
	var count := int(0.7 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var soft := 0.0
	var dull := 0.0
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		soft = lerpf(soft, noise, 0.35)
		dull = lerpf(dull, noise, 0.04)
		samples[i] = (soft - dull) * minf(t * 30.0, 1.0) * _decay(t, 4.5)
	return _stream(_shaped(samples, 0.5))


# A jump pad: a springy note that leaps up.
static func _pad() -> AudioStreamWAV:
	return _make(0.3, func(t: float, _i: int) -> float:
		var hertz := lerpf(180.0, 720.0, sqrt(t / 0.3))
		return (sin(TAU * hertz * t) * 0.7 + sin(TAU * hertz * 2.0 * t) * 0.2) * _decay(t, 7.0), 0.5)


static func _hit() -> AudioStreamWAV:
	return _make(0.07, func(t: float, _i: int) -> float:
		return sin(TAU * 1500.0 * t) * _decay(t, 45.0), 0.6)


# A hit to the head: the same tick, higher, with a second on its heels.
static func _hit_head() -> AudioStreamWAV:
	return _make(0.12, func(t: float, _i: int) -> float:
		var second := sin(TAU * 2900.0 * (t - 0.04)) * _decay(t - 0.04, 40.0) if t >= 0.04 else 0.0
		return (sin(TAU * 2200.0 * t) * _decay(t, 55.0) + second) * 0.7, 0.6)


# Knocking someone out: a low knock under two rising notes.
static func _knock_out() -> AudioStreamWAV:
	return _make(0.3, func(t: float, _i: int) -> float:
		var note := 880.0 if t < 0.09 else 1320.0
		var since := t if t < 0.09 else t - 0.09
		return sin(TAU * note * since) * _decay(since, 14.0) * 0.6 + sin(TAU * 140.0 * t) * _decay(t, 30.0) * 0.6, 0.6)


static func _hurt() -> AudioStreamWAV:
	var rng := _rng(6)
	return _make(0.18, func(t: float, _i: int) -> float:
		return (sin(TAU * 140.0 * t) * 0.7 + rng.randf_range(-1.0, 1.0) * 0.3) * _decay(t, 16.0))


static func _knocked_out() -> AudioStreamWAV:
	return _make(0.45, func(t: float, _i: int) -> float:
		var frequency := lerpf(620.0, 180.0, t / 0.45)
		return signf(sin(TAU * frequency * t)) * 0.5 * _decay(t, 4.0), 0.6)


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
