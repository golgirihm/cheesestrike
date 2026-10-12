class_name BodyShape
## The shape of a player's cheese character, and where a ray first touches it.
## The character everyone sees (player/cheese_character.gd) is built from
## these measurements, and shots are tested against the same ones
## (hit_scan.gd), so a shot hits when it looks like it hits.
##
## Only the wedge can be hit. The face and the holes are drawn flat on its
## surface, and the arms and legs are thin sticks that shots pass through.
##
## Measurements are in the character's own units and grow by SIZE in the
## arena. The body faces -Z and turns with the player.

## How many times bigger than its measurements a character stands.
const SIZE := 3.0

const HIP_HEIGHT := 0.42
## An upright wedge (a triangle across, standing on its base, made deep).
const WEDGE_SIZE := Vector3(0.78, 1.0, 0.56)
const WEDGE_CENTRE := Vector3(0, HIP_HEIGHT + 0.5, 0)
## How far the wedge's edges are rounded off where it is drawn. Shots are
## tested against the wedge with sharp edges, which is that hair bigger.
const WEDGE_ROUNDING := 0.025

## The faces of the wedge that holes are drawn on.
enum { FRONT, BACK, RIGHT, LEFT }
## Holes, as [face, across, up, radius]. On the front and back, `across` and
## `up` are from the middle of the wedge; on the sloping sides, `across` is
## from the middle toward the back and `up` is the share of the way up the
## slope.
const HOLES := [
	[FRONT, 0.0, 0.27, 0.04], [FRONT, -0.21, -0.31, 0.06], [FRONT, 0.2, -0.36, 0.04],
	[FRONT, 0.24, -0.22, 0.028], [FRONT, -0.03, -0.38, 0.03],
	[BACK, 0.04, 0.2, 0.05], [BACK, -0.12, -0.04, 0.06], [BACK, 0.15, -0.26, 0.07],
	[BACK, -0.2, -0.36, 0.04], [BACK, 0.0, -0.38, 0.03],
	[RIGHT, 0.1, 0.2, 0.06], [RIGHT, -0.12, 0.42, 0.05], [RIGHT, 0.06, 0.7, 0.04],
	[LEFT, -0.08, 0.25, 0.05], [LEFT, 0.12, 0.5, 0.06], [LEFT, -0.1, 0.72, 0.035],
]

## Arms and legs are sticks this thick.
const LIMB_RADIUS := 0.02
## Each leg hangs straight down from its hip, as [hip, length].
const LEGS := [[Vector3(-0.15, HIP_HEIGHT, 0), HIP_HEIGHT], [Vector3(0.15, HIP_HEIGHT, 0), HIP_HEIGHT]]
## Each arm starts on the slope of the wedge, as [shoulder, length]: the left
## hangs free, and the right reaches to the weapon.
const SHOULDER_HEIGHT := HIP_HEIGHT + 0.42
const ARMS := [[Vector3(-0.226, SHOULDER_HEIGHT, 0), 0.34], [Vector3(0.226, SHOULDER_HEIGHT, 0), 0.34]]

## Where the player sees from, above their feet: level with the eyes drawn
## on the front of the wedge, so that a shot fired straight ahead at a player
## standing level with the shooter meets their face.
const HEAD_HEIGHT := 0.9
## The eyes: dots drawn this far either side of the middle of the face.
const EYE_APART := 0.075
const EYE_RADIUS := 0.028
## Hits on the wedge from just under the eyes up to its tip are headshots.
const HEAD_FROM := HEAD_HEIGHT - EYE_RADIUS

## The capsule the player moves as, which holds all of the above.
const CAPSULE_RADIUS := 0.4
const CAPSULE_HEIGHT := 1.8

## The same, as they measure in the arena.
const RADIUS := CAPSULE_RADIUS * SIZE
const HEIGHT := CAPSULE_HEIGHT * SIZE


## The middle of each eye, on the front of the wedge, in the body's
## measurements.
static func eye_centres() -> Array[Vector3]:
	var centres: Array[Vector3] = []
	for side in [-1.0, 1.0]:
		centres.append(Vector3(EYE_APART * side, HEAD_HEIGHT, -WEDGE_SIZE.z / 2.0))
	return centres


## Where hole `hole` (one of HOLES) is drawn, in the body's measurements, as
## {centre, normal, radius}: `normal` is the way the face it lies on looks.
static func hole_place(hole: Array) -> Dictionary:
	var half := WEDGE_SIZE / 2.0
	var centre := Vector3.ZERO
	var normal := Vector3.ZERO
	match hole[0]:
		FRONT, BACK:
			normal = Vector3.FORWARD if hole[0] == FRONT else Vector3.BACK
			centre = Vector3(hole[1], hole[2], normal.z * half.z)
		_:
			var side := 1.0 if hole[0] == RIGHT else -1.0
			normal = Vector3(2.0 * half.y * side, half.x, 0).normalized()
			# From the foot of the slope to the tip.
			centre = Vector3(half.x * side, -half.y, hole[1]).lerp(Vector3(0, half.y, hole[1]), hole[2])
	return {"centre": WEDGE_CENTRE + centre, "normal": normal, "radius": hole[3]}


## Where a ray from `origin` in direction `direction` (normalized) first touches
## the wedge of a player standing at `feet` and turned by `yaw`, as {distance,
## head}; or an empty dictionary if it misses. A ray that starts inside the
## wedge doesn't hit it.
static func ray_hit(origin: Vector3, direction: Vector3, feet: Vector3, yaw := 0.0) -> Dictionary:
	# Into the body's own measurements, where it stands at the origin facing -Z.
	var unturn := Basis(Vector3.UP, -yaw)
	var local := unturn * (origin - feet) / SIZE
	var along := unturn * direction
	var t := _ray_wedge(local - WEDGE_CENTRE, along)
	if t == INF:
		return {}
	return {"distance": t * SIZE, "head": local.y + along.y * t >= HEAD_FROM}


## The point up the middle of a player standing at `feet` that is nearest
## `at`: where a burst at `at` is measured to. The middle runs up the core of
## their capsule, a radius in from its ends, so it is never down at the floor
## a burst lies on.
static func nearest_middle(feet: Vector3, at: Vector3) -> Vector3:
	return feet + Vector3.UP * clampf(at.y - feet.y, RADIUS, HEIGHT - RADIUS)


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
