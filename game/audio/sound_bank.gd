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
	"pistols": "_pistols",
	"carbine": "_carbine",
	"rifle": "_rifle",
	"lmg": "_lmg",
	"bow": "_bow",
	"stick": "_stick",
	"burst": "_burst",
	"hit": "_hit",
	"hit_head": "_hit_head",
	"knock_out": "_knock_out",
	"hurt": "_hurt",
	"knocked_out": "_knocked_out",
	"step": "_step",
	"step_rock": "_step_rock",
	"step_grass": "_step_grass",
	"step_wood": "_step_wood",
	"step_metal": "_step_metal",
	"step_cheese": "_step_cheese",
	"jump": "_jump",
	"land": "_land",
	"slide": "_slide",
	"pad": "_pad",
	"emote": "_emote",
	"pickup": "_pickup",
	"overload": "_overload",
	"power": "_power",
	"objective": "_objective",
	"gnaw": "_gnaw",
	"crumble": "_crumble",
	"reload": "_reload",
	"dry": "_dry",
	"knife": "_knife",
	"throw": "_knife",
	"voice_effort": "_voice_effort",
	"voice_pain": "_voice_pain",
	"voice_taunt": "_voice_taunt",
	"voice_reload": "_voice_reload",
	"voice_out": "_voice_out",
	"hit_pop": "_hit_pop",
	"hit_bell": "_hit_bell",
	"ui_hover": "_ui_hover",
	"ui_press": "_ui_press",
	"ui_back": "_ui_back",
	"ui_error": "_ui_error",
	"ui_count": "_ui_count",
	"music": "_music",
	"music_match": "_music_match",
	"music_tense": "_music_tense",
	"sting_win": "_sting_win",
	"sting_lose": "_sting_lose",
}

## How many takes there are of the sounds heard over and over, so that no
## two in a row need be the same. The rest have one.
const TAKES := {"pistol": 3, "blaster": 3, "shotgun": 3, "grater": 4, "pistols": 3, "carbine": 3, "rifle": 2, "lmg": 4,
	"launcher": 2, "burst": 3, "step": 4, "land": 2, "hurt": 2,
	"step_rock": 4, "step_grass": 4, "step_wood": 4, "step_metal": 4, "step_cheese": 4,
	"voice_effort": 3, "voice_pain": 3}

static var _made := {}
# The take being made: 0 is each sound as written, the rest differ from it.
static var _take := 0


## How many takes there are of the sound called `name`.
static func takes(name: String) -> int:
	return TAKES.get(name, 1)


## The sound called `name`, or null if there is none: its first take, or the
## one asked for.
static func get_sound(name: String, take := 0) -> AudioStreamWAV:
	if not SOUNDS.has(name):
		return null
	take = clampi(take, 0, takes(name) - 1)
	var key := [name, take]
	if not _made.has(key):
		_take = take
		_made[key] = Callable(SoundBank, SOUNDS[name]).call()
		_take = 0
	return _made[key]


## Which take of `name` to play next, when `last` was played before: any
## other, so that the same one is never heard twice running.
static func pick(name: String, last := -1, rng: RandomNumberGenerator = null) -> int:
	var count := takes(name)
	if count <= 1:
		return 0
	var roll := rng.randi() if rng != null else randi()
	if last < 0 or last >= count:
		return roll % count
	return (last + 1 + roll % (count - 1)) % count


## Makes sounds that haven't been made yet, for up to `milliseconds`, so
## that none is made in the middle of play. Returns whether all are made.
static func make_some(milliseconds: int) -> bool:
	var began := Time.get_ticks_msec()
	for name: String in SOUNDS:
		for take in takes(name):
			if _made.has([name, take]):
				continue
			if Time.get_ticks_msec() - began >= milliseconds:
				return false
			get_sound(name, take)
	return true


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
	# Each take has its own noise.
	rng.seed = seed + _take * 7919
	return rng


# `value`, a little more or less for every take but the first: up to `by`
# of it either way, and the same each time that take is made.
static func _varied(value: float, by: float, which: int) -> float:
	if _take == 0:
		return value
	return value * (1.0 + by * sin(_take * 12.9898 + which * 78.233))


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
#   click   the weapon's own works a moment later: the slide, bolt or pump,
#           a tick of metal at `click_at` seconds ringing at `click_note`
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
	# No two takes of a shot are quite alike: a little higher or lower, a
	# little longer or shorter.
	var thump_from := _varied(spec.get("thump_from", 150.0), 0.05, 1)
	var thump_to := _varied(spec.get("thump_to", 50.0), 0.05, 1)
	var blast_decay := _varied(spec.get("blast_decay", 45.0), 0.1, 2)
	var tail_decay := _varied(spec.get("tail_decay", 10.0), 0.1, 3)
	var click: float = spec.get("click", 0.0)
	var click_at: float = spec.get("click_at", 0.08)
	var click_note := _varied(spec.get("click_note", 2600.0), 0.04, 4)
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		# Noise with its high end taken off, more of it for the tail.
		blast = lerpf(blast, noise, spec.get("blast_bright", 0.35))
		tail = lerpf(tail, noise, spec.get("tail_bright", 0.06))
		var hertz := lerpf(thump_to, thump_from, _decay(t, spec.get("thump_fall", 30.0)))
		turn += hertz / RATE
		var sample: float = noise * spec.get("crack", 1.0) * _decay(t, spec.get("crack_decay", 900.0))
		sample += blast * spec.get("blast", 1.0) * _decay(t, blast_decay)
		sample += sin(TAU * turn) * spec.get("thump", 0.8) * _decay(t, spec.get("thump_decay", 28.0))
		# The tail swells for a moment as the first echoes arrive, then dies.
		sample += tail * spec.get("tail", 1.5) * minf(t * 60.0, 1.0) * _decay(t, tail_decay)
		if click > 0.0 and t >= click_at:
			var since := t - click_at
			sample += click * (sin(TAU * click_note * since) * 0.7 + sin(TAU * click_note * 1.48 * since) * 0.3 + noise * 0.25) * _decay(since, 160.0)
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
		"thump": 0.5, "thump_from": 210.0, "thump_to": 90.0, "thump_decay": 45.0, "tail": 0.8, "tail_bright": 0.12, "tail_decay": 18.0, "volume": 0.75,
		"click": 0.22, "click_at": 0.09, "click_note": 3100.0})


# A rifle: a hard crack with weight under it, and a tail that carries.
static func _blaster() -> AudioStreamWAV:
	return _gunshot(1, 0.42, {"crack": 1.3, "crack_decay": 600.0, "blast": 1.2, "blast_bright": 0.4, "blast_decay": 48.0,
		"thump": 0.9, "thump_from": 170.0, "thump_to": 60.0, "thump_decay": 30.0, "tail": 1.6, "tail_bright": 0.08, "tail_decay": 11.0, "volume": 0.85,
		"click": 0.2, "click_at": 0.075, "click_note": 2500.0})


# The grater fires so fast that each shot has to be short, or they smear
# into a hum.
static func _grater() -> AudioStreamWAV:
	return _gunshot(3, 0.15, {"crack": 1.1, "crack_decay": 900.0, "blast": 1.0, "blast_bright": 0.55, "blast_decay": 95.0,
		"thump": 0.5, "thump_from": 240.0, "thump_to": 110.0, "thump_decay": 70.0, "tail": 0.6, "tail_bright": 0.15, "tail_decay": 34.0, "volume": 0.65,
		"click": 0.12, "click_at": 0.045, "click_note": 3400.0})


# A shotgun: a boom more than a crack, and a long roll after it.
static func _shotgun() -> AudioStreamWAV:
	return _gunshot(2, 0.75, {"crack": 0.9, "crack_decay": 450.0, "blast": 1.6, "blast_bright": 0.22, "blast_decay": 24.0,
		"thump": 1.3, "thump_from": 120.0, "thump_to": 42.0, "thump_decay": 16.0, "thump_fall": 20.0, "tail": 2.4, "tail_bright": 0.05, "tail_decay": 6.5, "drive": 2.8, "volume": 0.95,
		"click": 0.3, "click_at": 0.36, "click_note": 1700.0})


# One of a pair of pistols: lighter and sharper than one held in two hands.
static func _pistols() -> AudioStreamWAV:
	return _gunshot(21, 0.24, {"crack": 1.3, "crack_decay": 800.0, "blast": 0.8, "blast_bright": 0.6, "blast_decay": 85.0,
		"thump": 0.4, "thump_from": 250.0, "thump_to": 110.0, "thump_decay": 55.0, "tail": 0.6, "tail_bright": 0.14, "tail_decay": 24.0, "volume": 0.68,
		"click": 0.2, "click_at": 0.07, "click_note": 3300.0})


# The carbine: three quick cracks, so each is tight and dry.
static func _carbine() -> AudioStreamWAV:
	return _gunshot(22, 0.26, {"crack": 1.4, "crack_decay": 750.0, "blast": 1.1, "blast_bright": 0.45, "blast_decay": 70.0,
		"thump": 0.7, "thump_from": 190.0, "thump_to": 75.0, "thump_decay": 42.0, "tail": 1.0, "tail_bright": 0.1, "tail_decay": 17.0, "volume": 0.8,
		"click": 0.18, "click_at": 0.06, "click_note": 2800.0})


# The rifle: one heavy shot that rolls round the arena for a second.
static func _rifle() -> AudioStreamWAV:
	return _gunshot(23, 0.98, {"crack": 1.6, "crack_decay": 380.0, "blast": 1.7, "blast_bright": 0.3, "blast_decay": 26.0,
		"thump": 1.4, "thump_from": 140.0, "thump_to": 38.0, "thump_decay": 13.0, "thump_fall": 18.0, "tail": 2.8, "tail_bright": 0.04, "tail_decay": 4.0, "drive": 3.0, "volume": 0.98,
		"click": 0.32, "click_at": 0.46, "click_note": 1900.0})


# The machine gun: slower and heavier than the grater, with more thump.
static func _lmg() -> AudioStreamWAV:
	return _gunshot(24, 0.2, {"crack": 1.0, "crack_decay": 700.0, "blast": 1.3, "blast_bright": 0.35, "blast_decay": 60.0,
		"thump": 1.0, "thump_from": 150.0, "thump_to": 60.0, "thump_decay": 40.0, "tail": 0.9, "tail_bright": 0.08, "tail_decay": 22.0, "volume": 0.78,
		"click": 0.14, "click_at": 0.055, "click_note": 2300.0})


# A bow: the string's twang and the breath of the stick leaving.
static func _bow() -> AudioStreamWAV:
	var rng := _rng(25)
	var count := int(0.35 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var air := 0.0
	var turn := 0.0
	for i in count:
		var t := float(i) / RATE
		air = lerpf(air, rng.randf_range(-1.0, 1.0), 0.2)
		# The string settles from a high note to its own.
		turn += lerpf(170.0, 260.0, _decay(t, 40.0)) / RATE
		samples[i] = sin(TAU * turn) * _decay(t, 16.0) + sin(TAU * turn * 2.01) * 0.3 * _decay(t, 30.0) + air * 0.5 * _decay(t, 24.0)
	return _stream(_shaped(samples, 0.6))


# A breadstick striking something: a short woody knock.
static func _stick() -> AudioStreamWAV:
	var rng := _rng(26)
	return _make(0.12, func(t: float, _i: int) -> float:
		return rng.randf_range(-1.0, 1.0) * _decay(t, 180.0) + sin(TAU * 620.0 * t) * 0.8 * _decay(t, 60.0) + sin(TAU * 310.0 * t) * 0.5 * _decay(t, 45.0), 0.55)


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


# A footstep on something else. `spec` says what the ground gives back:
#
#   knock      how much of a knock the heel makes, and the notes it falls
#              between (`knock_from` to `knock_to` hertz)
#   dull       how much dull noise is in it, and how fast it dies (`decay`)
#   scuff      how much the sole scuffs after, and how bright (`grit`)
#   ring       how much the ground rings, at `ring_note` hertz, dying at
#              `ring_decay`: a board, or a sheet of metal
#   swell      seconds the sound takes to arrive: none for a hard floor, a
#              moment for something that gives
static func _footstep(seed: int, length: float, spec: Dictionary, volume := 0.6) -> AudioStreamWAV:
	var rng := _rng(seed)
	var count := int(length * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	var dull := 0.0
	var soft := 0.0
	var turn := 0.0
	var sole: float = spec.get("sole", 0.055)
	var decay := _varied(spec.get("decay", 65.0), 0.12, 1)
	var ring_note := _varied(spec.get("ring_note", 200.0), 0.06, 2)
	var knock_from := _varied(spec.get("knock_from", 120.0), 0.08, 3)
	for i in count:
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		dull = lerpf(dull, noise, spec.get("dullness", 0.1))
		soft = lerpf(soft, noise, spec.get("grit", 0.4))
		turn += lerpf(spec.get("knock_to", 55.0), knock_from, _decay(t, 60.0)) / RATE
		var arrive := minf(t / spec.swell, 1.0) if spec.get("swell", 0.0) > 0.0 else 1.0
		var heel: float = (dull * spec.get("dull", 2.6) + sin(TAU * turn) * spec.get("knock", 0.9)) * _decay(t, decay) * arrive
		var scuff: float = (soft - dull) * spec.get("scuff", 1.4) * _decay(t - sole, spec.get("scuff_decay", 80.0)) if t >= sole else 0.0
		var ring: float = (sin(TAU * ring_note * t) + sin(TAU * ring_note * 2.76 * t) * 0.5) * spec.get("ring", 0.0) * _decay(t, spec.get("ring_decay", 30.0))
		samples[i] = tanh(heel + scuff + ring)
	return _stream(_shaped(samples, volume))


# On rock: a harder, higher knock, and more grit under the sole.
static func _step_rock() -> AudioStreamWAV:
	return _footstep(31, 0.17, {"knock": 0.7, "knock_from": 190.0, "knock_to": 90.0, "dull": 2.0, "dullness": 0.16, "decay": 80.0, "scuff": 2.2, "grit": 0.6, "scuff_decay": 60.0})


# On grass and earth: no knock at all, a soft press and a long swish.
static func _step_grass() -> AudioStreamWAV:
	return _footstep(32, 0.22, {"knock": 0.15, "knock_from": 80.0, "knock_to": 45.0, "dull": 1.6, "dullness": 0.05, "decay": 34.0, "swell": 0.012,
		"scuff": 1.9, "grit": 0.3, "scuff_decay": 26.0, "sole": 0.03}, 0.5)


# On wood: a hollow knock, with the board ringing under it.
static func _step_wood() -> AudioStreamWAV:
	return _footstep(33, 0.2, {"knock": 1.1, "knock_from": 230.0, "knock_to": 150.0, "dull": 1.6, "dullness": 0.12, "decay": 55.0, "scuff": 0.6,
		"ring": 0.55, "ring_note": 185.0, "ring_decay": 26.0})


# On metal: a clank, and the sheet ringing on after.
static func _step_metal() -> AudioStreamWAV:
	return _footstep(34, 0.26, {"knock": 0.5, "knock_from": 320.0, "knock_to": 200.0, "dull": 1.2, "dullness": 0.3, "decay": 90.0, "scuff": 0.8, "grit": 0.7,
		"ring": 0.7, "ring_note": 540.0, "ring_decay": 16.0})


# On cheese, and on bread: it gives underfoot, a dull squash with nothing
# sharp in it.
static func _step_cheese() -> AudioStreamWAV:
	return _footstep(35, 0.2, {"knock": 0.8, "knock_from": 95.0, "knock_to": 50.0, "dull": 2.2, "dullness": 0.035, "decay": 30.0, "swell": 0.02,
		"scuff": 0.5, "grit": 0.12, "scuff_decay": 30.0, "sole": 0.07}, 0.55)


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


# An emote: two quick squeaks, the second higher.
static func _emote() -> AudioStreamWAV:
	return _make(0.26, func(t: float, _i: int) -> float:
		var second := t >= 0.13
		var since := t - 0.13 if second else t
		var hertz := (1250.0 if second else 950.0) + 500.0 * since / 0.13
		return sin(TAU * hertz * since) * sin(PI * minf(since / 0.11, 1.0)), 0.4)


static func _hit() -> AudioStreamWAV:
	return _make(0.07, func(t: float, _i: int) -> float:
		return sin(TAU * 1500.0 * t) * _decay(t, 45.0), 0.6)


# A character's voice: a squeak sung through `notes`, each [when it starts,
# how long it lasts, the note it starts on, the note it ends on] in seconds
# and hertz. A squeak is a note with its octave and its twelfth over it,
# and a little wobble.
static func _squeak(length: float, notes: Array, volume := 0.6, breath := 0.0) -> AudioStreamWAV:
	var rng := _rng(40 + notes.size())
	var turn := 0.0
	var lift := _varied(1.0, 0.07, 1)
	return _make(length, func(t: float, _i: int) -> float:
		var sample := 0.0
		for note: Array in notes:
			var since: float = t - note[0]
			if since < 0.0 or since > note[1]:
				continue
			var along: float = since / note[1]
			var hertz: float = lerpf(note[2], note[3], along) * lift * (1.0 + 0.03 * sin(TAU * 28.0 * t))
			turn += hertz / RATE
			# Quick in, and eased away by the end of the note.
			var swell := minf(since * 120.0, 1.0) * (1.0 - along * along)
			sample += (sin(TAU * turn) + sin(TAU * turn * 2.0) * 0.35 + sin(TAU * turn * 3.0) * 0.15) * swell
		return sample * 0.6 + rng.randf_range(-1.0, 1.0) * breath * _decay(t, 20.0), volume)


# Effort, as in a jump or a throw: a quick "hup".
static func _voice_effort() -> AudioStreamWAV:
	return _squeak(0.13, [[0.0, 0.12, 330.0, 470.0]], 0.5, 0.25)


# Pain: a sharp "eek" that falls away.
static func _voice_pain() -> AudioStreamWAV:
	return _squeak(0.2, [[0.0, 0.19, 820.0, 560.0]], 0.6, 0.1)


# A taunt: three notes, up and back, sung at someone.
static func _voice_taunt() -> AudioStreamWAV:
	return _squeak(0.5, [[0.0, 0.14, 560.0, 580.0], [0.16, 0.14, 700.0, 720.0], [0.32, 0.17, 560.0, 500.0]], 0.6)


# "Reloading": two rising chirps, called to whoever is on the same side.
static func _voice_reload() -> AudioStreamWAV:
	return _squeak(0.34, [[0.0, 0.13, 430.0, 520.0], [0.17, 0.16, 520.0, 680.0]], 0.55)


# Knocked out: a squeal that falls all the way down.
static func _voice_out() -> AudioStreamWAV:
	return _squeak(0.55, [[0.0, 0.54, 880.0, 190.0]], 0.65, 0.15)


# The same told another way, for those who choose it: a soft low pop.
static func _hit_pop() -> AudioStreamWAV:
	return _make(0.09, func(t: float, _i: int) -> float:
		return sin(TAU * lerpf(260.0, 520.0, _decay(t, 40.0)) * t) * _decay(t, 38.0), 0.7)


# Or a small bell: a clear note with one above it that rings a little.
static func _hit_bell() -> AudioStreamWAV:
	return _make(0.22, func(t: float, _i: int) -> float:
		return (sin(TAU * 1760.0 * t) * 0.7 + sin(TAU * 2640.0 * t) * 0.3) * _decay(t, 18.0), 0.55)


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


# Mice at the cheese: a run of small dry scrapes.
static func _gnaw() -> AudioStreamWAV:
	var rng := _rng(31)
	var soft := 0.0
	var count := int(0.9 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	for i in count:
		var t := float(i) / RATE
		soft = lerpf(soft, rng.randf_range(-1.0, 1.0), 0.5)
		# Eleven nibbles a second, each a scrape that is over at once.
		samples[i] = soft * _decay(fmod(t, 0.09), 60.0) * minf(t * 8.0, 1.0)
	return _stream(_shaped(samples, 0.5))


# A block of cheese giving way: a soft thud and a patter of crumbs.
static func _crumble() -> AudioStreamWAV:
	var rng := _rng(32)
	var soft := 0.0
	var turn := 0.0
	var count := int(0.7 * RATE)
	var samples := PackedFloat32Array()
	samples.resize(count)
	for i in count:
		var t := float(i) / RATE
		soft = lerpf(soft, rng.randf_range(-1.0, 1.0), 0.18)
		turn += lerpf(55.0, 120.0, _decay(t, 20.0)) / RATE
		var patter := rng.randf_range(-1.0, 1.0) * (0.5 if rng.randf() < 0.02 else 0.0)
		samples[i] = sin(TAU * turn) * _decay(t, 12.0) + soft * 1.4 * _decay(t, 7.0) + patter * _decay(t, 4.0)
	return _stream(_shaped(samples, 0.8))


# Something has happened in the match that everyone is told: two notes.
static func _objective() -> AudioStreamWAV:
	return _make(0.4, func(t: float, _i: int) -> float:
		var note := 587.33 if t < 0.16 else 880.0
		var since := t if t < 0.16 else t - 0.16
		return (sin(TAU * note * t) + 0.4 * sin(TAU * note * 2.0 * t)) * _decay(since, 7.0), 0.6)


# A power-up taken: a fanfare of rising chords, loud enough for everyone.
static func _power() -> AudioStreamWAV:
	var notes := [392.0, 523.25, 659.25, 783.99, 1046.5]
	return _make(0.9, func(t: float, _i: int) -> float:
		var step := mini(int(t / 0.12), 4)
		var note: float = notes[step]
		var since := t - step * 0.12
		var ring := _decay(since, 3.0 if step == 4 else 9.0)
		return (sin(TAU * note * t) + 0.5 * sin(TAU * note * 1.5 * t) + 0.3 * sin(TAU * note * 2.0 * t)) * ring * 0.55, 0.85)


static func _overload() -> AudioStreamWAV:
	return _make(0.7, func(t: float, _i: int) -> float:
		var frequency := lerpf(200.0, 820.0, t / 0.7)
		var wobble := 0.6 + 0.4 * sin(TAU * 18.0 * t)
		return sin(TAU * frequency * t) * wobble * minf(t * 10.0, 1.0) * _decay(maxf(t - 0.5, 0.0), 12.0), 0.6)


## A cheerful loop for the menu: a bass line and a tune, four bars long.
# The pointer coming onto a button: the lightest tick.
static func _ui_hover() -> AudioStreamWAV:
	return _make(0.045, func(t: float, _i: int) -> float:
		return sin(TAU * 1900.0 * t) * _decay(t, 110.0), 0.35)


# A button pressed: a click with a note in it.
static func _ui_press() -> AudioStreamWAV:
	return _make(0.08, func(t: float, _i: int) -> float:
		return (sin(TAU * 880.0 * t) * 0.7 + sin(TAU * 1320.0 * t) * 0.3) * _decay(t, 55.0), 0.5)


# A button that goes back: the same, a fifth lower and falling.
static func _ui_back() -> AudioStreamWAV:
	return _make(0.1, func(t: float, _i: int) -> float:
		return sin(TAU * lerpf(587.0, 440.0, minf(t / 0.1, 1.0)) * t) * _decay(t, 40.0), 0.5)


# Something went wrong: two low buzzes.
static func _ui_error() -> AudioStreamWAV:
	return _make(0.3, func(t: float, _i: int) -> float:
		var since := fmod(t, 0.15)
		var buzz := 1.0 if sin(TAU * 150.0 * t) > 0.0 else -1.0
		return (buzz * 0.5 + sin(TAU * 150.0 * t) * 0.5) * _decay(since, 14.0) * (1.0 if since < 0.11 else 0.0), 0.45)


# One of the last seconds of a wait: a clear short pip.
static func _ui_count() -> AudioStreamWAV:
	return _make(0.12, func(t: float, _i: int) -> float:
		return sin(TAU * 1046.5 * t) * _decay(t, 26.0), 0.5)


# A piece of music from its notes, each [start in beats, length in beats,
# hertz, how loud, whether a soft triangle rather than a bright note], at
# `tempo` beats a minute and `beats` long.
static func _piece(tempo: float, beats: int, notes: Array, loops := true) -> AudioStreamWAV:
	var beat := 60.0 / tempo
	var samples := PackedFloat32Array()
	samples.resize(int(beats * beat * MUSIC_RATE))
	for note: Array in notes:
		_add_note(samples, note[0] * beat, note[1] * beat, note[2], note[3], note[4])
	return _stream(samples, MUSIC_RATE, loops)


# Under a match: slow and sparse, a low pulse with a few held notes over
# it, in a minor key, so that it sits behind the fight.
static func _music_match() -> AudioStreamWAV:
	var notes := []
	var roots := [110.0, 110.0, 87.31, 98.0]
	for bar in 4:
		for step in 2:
			notes.append([bar * 4 + step * 2, 1.6, roots[bar], 0.22, true])
		notes.append([bar * 4 + 1, 2.5, roots[bar] * [3.0, 2.4, 3.0, 2.67][bar], 0.07, false])
	return _piece(96.0, 16, notes)


# Nearly decided: the same key, faster, the pulse on every half beat and a
# figure climbing over it.
static func _music_tense() -> AudioStreamWAV:
	var notes := []
	var roots := [110.0, 110.0, 130.81, 123.47]
	var climb := [440.0, 523.25, 587.33, 659.25]
	for bar in 4:
		for step in 8:
			notes.append([bar * 4 + step * 0.5, 0.4, roots[bar] * (2.0 if step % 4 == 3 else 1.0), 0.24, true])
		for step in 4:
			notes.append([bar * 4 + step, 0.45, climb[(step + bar) % 4], 0.11, false])
	return _piece(144.0, 16, notes)


# Won: a quick run up a major chord to a held note.
static func _sting_win() -> AudioStreamWAV:
	var notes := []
	var run := [523.25, 659.25, 783.99, 1046.5]
	for step in 4:
		notes.append([step * 0.5, 0.5 if step < 3 else 2.2, run[step], 0.22, false])
		notes.append([step * 0.5, 0.5 if step < 3 else 2.2, run[step] / 2.0, 0.16, true])
	return _piece(160.0, 5, notes, false)


# Lost: three notes down a minor chord, the last one low and long.
static func _sting_lose() -> AudioStreamWAV:
	var notes := []
	var fall := [392.0, 311.13, 261.63]
	for step in 3:
		notes.append([step * 0.75, 0.75 if step < 2 else 2.4, fall[step], 0.2, false])
		notes.append([step * 0.75, 0.75 if step < 2 else 2.4, fall[step] / 2.0, 0.2, true])
	return _piece(120.0, 4, notes, false)


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
