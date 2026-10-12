class_name BodyShape
## The shape of a player's cheese character, and where a ray first touches it.
## The character everyone sees (player/cheese_character.gd, player/eyes.gd) is
## built from these measurements, and shots are tested against the same ones
## (hit_scan.gd), so a shot hits exactly when it looks like it hits.
##
## Measurements are in the character's own units and grow by SIZE in the
## arena. The body faces -Z and turns with the player; the eyes are on the
## head, so they also tilt with where the player looks. Arms and legs are
## tested where they hang at rest: walking swings them a little, in step with
## a pace that each screen keeps for itself.

## How many times bigger than its measurements a character stands.
const SIZE := 3.0

const HIP_HEIGHT := 0.42
## An upright wedge (a triangle across, standing on its base, made deep).
const WEDGE_SIZE := Vector3(0.78, 1.0, 0.56)
const WEDGE_CENTRE := Vector3(0, HIP_HEIGHT + 0.5, 0)
## Holes, as [centre, radius]: bumps of darker cheese on the wedge's faces.
const HOLES := [
	[Vector3(-0.16, 0.94, -0.27), 0.07], [Vector3(0.12, 1.1, -0.27), 0.05],
	[Vector3(0.05, 0.84, -0.27), 0.04], [Vector3(-0.27, 0.92, 0.05), 0.06],
	[Vector3(0.25, 1.0, 0.12), 0.05],
]
## Limbs hang straight down from their joints, as [joint, length, radius].
const LEGS := [[Vector3(-0.17, HIP_HEIGHT, 0), 0.42, 0.07], [Vector3(0.17, HIP_HEIGHT, 0), 0.42, 0.07]]
const ARMS := [[Vector3(-0.36, HIP_HEIGHT + 0.42, -0.02), 0.34, 0.05], [Vector3(0.36, HIP_HEIGHT + 0.42, -0.02), 0.34, 0.05]]

## Where the player sees from, above their feet.
const HEAD_HEIGHT := 1.55
## The eyes' middle, from the head.
const EYES_OFFSET := Vector3(0, -0.12, -0.3)
const EYE_APART := 0.1
const EYE_RADIUS := 0.075
const PUPIL_FORWARD := 0.055
const PUPIL_RADIUS := 0.035
## Hits on the eyes, or on the wedge's tip level with them or higher, are headshots.
const HEAD_FROM := HEAD_HEIGHT + EYES_OFFSET.y - EYE_RADIUS

## The capsule the player moves as, which holds all of the above.
const CAPSULE_RADIUS := 0.4
const CAPSULE_HEIGHT := 1.8

## The same, as they measure in the arena.
const RADIUS := CAPSULE_RADIUS * SIZE
const HEIGHT := CAPSULE_HEIGHT * SIZE


## The middle of each eye (or, with `forward` = PUPIL_FORWARD, each pupil) in
## the body's measurements, for a head tilted by `pitch`.
static func eye_centres(pitch: float, forward := 0.0) -> Array[Vector3]:
	var tilt := Basis(Vector3.RIGHT, pitch)
	var head := Vector3(0, HEAD_HEIGHT, 0)
	var centres: Array[Vector3] = []
	for side in [-1.0, 1.0]:
		centres.append(head + tilt * (EYES_OFFSET + Vector3(EYE_APART * side, 0, -forward)))
	return centres


## Where a ray from `origin` in direction `direction` (normalized) first touches
## the body of a player standing at `feet`, turned by `yaw` and looking up or
## down by `pitch`, as {distance, head}; or an empty dictionary if it misses. A
## ray that starts inside a part doesn't hit that part.
static func ray_hit(origin: Vector3, direction: Vector3, feet: Vector3, yaw := 0.0, pitch := 0.0) -> Dictionary:
	# Into the body's own measurements, where it stands at the origin facing -Z.
	var unturn := Basis(Vector3.UP, -yaw)
	var local := unturn * (origin - feet) / SIZE
	var along := unturn * direction
	var nearest := INF
	var head := false

	var t := _ray_wedge(local - WEDGE_CENTRE, along)
	if t < nearest:
		nearest = t
		head = local.y + along.y * t >= HEAD_FROM
	for hole: Array in HOLES:
		t = HitScan.ray_sphere(local - hole[0], along, hole[1])
		if t < nearest:
			nearest = t
			head = local.y + along.y * t >= HEAD_FROM
	for limb: Array in LEGS + ARMS:
		var joint: Vector3 = limb[0]
		t = HitScan.ray_capsule(local, along, joint - Vector3(0, limb[1], 0), limb[2], limb[1])
		if t < nearest:
			nearest = t
			head = false
	for eye in eye_centres(pitch):
		t = HitScan.ray_sphere(local - eye, along, EYE_RADIUS)
		if t < nearest:
			nearest = t
			head = true
	for pupil in eye_centres(pitch, PUPIL_FORWARD):
		t = HitScan.ray_sphere(local - pupil, along, PUPIL_RADIUS)
		if t < nearest:
			nearest = t
			head = true
	if nearest == INF:
		return {}
	return {"distance": nearest * SIZE, "head": head}


## The point on the upright line through a player standing at `feet` that is
## nearest `at`: where a burst at `at` is measured to.
static func nearest_middle(feet: Vector3, at: Vector3) -> Vector3:
	return feet + Vector3.UP * clampf(at.y - feet.y, 0.0, HEIGHT)


## How far along the ray `local` + t * `direction` it enters the wedge, measured
## from the wedge's centre, or INF. The wedge is where all of its faces' inner
## sides meet, so the ray is inside it between entering the last face it
## crosses inward and leaving by the first it crosses outward.
static func _ray_wedge(local: Vector3, direction: Vector3) -> float:
	var half := WEDGE_SIZE / 2.0
	# Each face as [outward normal, distance of the face from the centre].
	var slope_right := Vector3(2.0 * half.y, half.x, 0).normalized()
	var slope_left := Vector3(-slope_right.x, slope_right.y, 0)
	var faces := [
		[Vector3.DOWN, half.y], [Vector3.FORWARD, half.z], [Vector3.BACK, half.z],
		[slope_right, slope_right.dot(Vector3(half.x, -half.y, 0))],
		[slope_left, slope_left.dot(Vector3(-half.x, -half.y, 0))],
	]
	var enter := -INF
	var leave := INF
	for face: Array in faces:
		var normal: Vector3 = face[0]
		var towards := normal.dot(direction)
		var room: float = face[1] - normal.dot(local)
		if absf(towards) < 1e-9:
			if room < 0.0:
				return INF  # Alongside the face, outside it.
			continue
		var t := room / towards
		if towards < 0.0:
			enter = maxf(enter, t)
		else:
			leave = minf(leave, t)
	if enter > leave or enter < 0.0:
		return INF
	return enter
