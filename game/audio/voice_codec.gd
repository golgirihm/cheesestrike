class_name VoiceCodec
## How a player's voice is made small enough to send: one channel, eight
## thousand samples a second, each squeezed into a byte the way telephones
## do it (so that quiet speech keeps its detail and loud speech still
## fits). A fifteenth of a second of talk is 480 bytes.

## Samples a second, as sent.
const RATE := 8000
## Seconds of talk in one packet, and the most bytes one may hold: a packet
## from anyone is checked against it.
const PACKET_SECONDS := 0.06
const MAX_BYTES := 1200
## Talk quieter than this, as the root of the mean square of its samples, is
## silence, and not sent.
const QUIET := 0.008
const _MU := 255.0


## `frames`, stereo at `from_rate` samples a second, as bytes to send.
static func encode(frames: PackedVector2Array, from_rate: float) -> PackedByteArray:
	var bytes := PackedByteArray()
	if frames.is_empty() or from_rate <= 0.0:
		return bytes
	var count := mini(int(frames.size() * RATE / from_rate), MAX_BYTES)
	bytes.resize(count)
	var step := from_rate / RATE
	for index in count:
		# The middle of the stretch of the original this sample stands for.
		var first := int(index * step)
		var last := mini(int((index + 1) * step), frames.size())
		var sum := 0.0
		for at in range(first, maxi(last, first + 1)):
			sum += (frames[at].x + frames[at].y) * 0.5
		bytes[index] = squeeze(sum / maxi(last - first, 1))
	return bytes


## The samples `bytes` hold, from -1 to 1, at RATE a second.
static func decode(bytes: PackedByteArray) -> PackedFloat32Array:
	var samples := PackedFloat32Array()
	samples.resize(bytes.size())
	for index in bytes.size():
		samples[index] = stretch(bytes[index])
	return samples


## One sample, from -1 to 1, as a byte.
static func squeeze(sample: float) -> int:
	var clipped := clampf(sample, -1.0, 1.0)
	var squeezed := signf(clipped) * log(1.0 + _MU * absf(clipped)) / log(1.0 + _MU)
	return clampi(roundi((squeezed + 1.0) * 127.5), 0, 255)


## The sample a byte stands for.
static func stretch(byte: int) -> float:
	var squeezed := byte / 127.5 - 1.0
	return signf(squeezed) * (pow(1.0 + _MU, absf(squeezed)) - 1.0) / _MU


## How loud `frames` are: the root of the mean square of their samples.
static func loudness(frames: PackedVector2Array) -> float:
	if frames.is_empty():
		return 0.0
	var total := 0.0
	for frame in frames:
		var sample := (frame.x + frame.y) * 0.5
		total += sample * sample
	return sqrt(total / frames.size())


## Whether `packet` is one that could have been made here: bytes, some but
## not too many.
static func is_packet(packet: Variant) -> bool:
	return packet is PackedByteArray and not packet.is_empty() and packet.size() <= MAX_BYTES
