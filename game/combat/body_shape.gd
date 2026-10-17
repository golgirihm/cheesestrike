class_name BodyShape
## The shape of a player's soldier, and where a ray first touches it. Shots
## are tested against these measurements (hit_scan.gd), which are the
## soldier's (player/soldier_character.gd), measured round its mesh posed
## standing and kneeling, with its eyes at HEAD_HEIGHT, so a shot hits when
## it looks like it hits.
##
## What can be hit is a few boxes: the head (cap and all), the torso and the
## two legs. The arms are thin enough that shots pass them by.
##
## Measurements are in the character's own units and grow by SIZE in the
## arena. The body faces -Z and turns with the player.

## How many times bigger than its measurements a character stands.
const SIZE := 3.0

const HIP_HEIGHT := 0.43
## Each leg hangs straight down from its hip, as [hip, length]. It bends at
## the knee halfway down: the thigh and the shin are each half its length.
const LEGS := [[Vector3(-0.08, HIP_HEIGHT, 0), HIP_HEIGHT], [Vector3(0.08, HIP_HEIGHT, 0), HIP_HEIGHT]]
const THIGH := HIP_HEIGHT / 2.0
## How wide and deep a leg is, boot and all: deep enough to hold the legs
## of a soldier standing with one foot ahead of the other.
const LEG_WIDTH := 0.13
const LEG_DEPTH := 0.28

## From the hips to the neck, across the shoulders and the vest.
const TORSO_SIZE := Vector3(0.24, 0.37, 0.21)
const TORSO_CENTRE := Vector3(0, HIP_HEIGHT + TORSO_SIZE.y / 2.0, 0)
## The head, from the chin to the top of the cap.
const HEAD_SIZE := Vector3(0.15, 0.18, 0.18)
const HEAD_CENTRE := Vector3(0, 0.89, 0)
## The middle of the chest: where to aim at a player, or put something on them.
const CHEST := TORSO_CENTRE
## How far the edges of each part are rounded off where it is drawn. Shots
## are tested against the parts with sharp edges, which are that hair bigger.
const ROUNDING := 0.025

## Arms are sleeves this thick, which shots pass by.
const LIMB_RADIUS := 0.035
## Each arm hangs from a shoulder, as [shoulder, length]: the left hangs
## free, and the right reaches to the weapon.
const SHOULDER_HEIGHT := HIP_HEIGHT + 0.33
const ARMS := [[Vector3(-0.145, SHOULDER_HEIGHT, 0), 0.3], [Vector3(0.145, SHOULDER_HEIGHT, 0), 0.3]]

## Where the player sees from, above their feet: level with the eyes drawn
## on the front of the head, so that a shot fired straight ahead at a player
## standing level with the shooter meets their face.
const HEAD_HEIGHT := 0.9
## The eyes: dots drawn this far either side of the middle of the face.
const EYE_APART := 0.03
const EYE_RADIUS := 0.015
## Hits on the head, from the chin up, are headshots.
const HEAD_FROM := HEAD_CENTRE.y - HEAD_SIZE.y / 2.0

## How far a player who crouches drops: their knees bend, and everything
## above the legs, with the eyes they see from, comes down by this much, as
## far as the realistic soldier's eyes come down when it kneels.
const CROUCH_DROP := 0.27

## How far a player who leans tips to one side, pivoting on their feet, in
## radians: enough to put their eyes most of a metre out past a corner.
const LEAN_ANGLE := 0.3

## The capsule the player moves as, which holds all of the above.
const CAPSULE_RADIUS := 0.4
const CAPSULE_HEIGHT := 1.8

## The same, as they measure in the arena.
const RADIUS := CAPSULE_RADIUS * SIZE
const HEIGHT := CAPSULE_HEIGHT * SIZE


## The middle of each eye, on the front of the head, in the body's
## measurements.
static func eye_centres() -> Array[Vector3]:
	var centres: Array[Vector3] = []
	for side in [-1.0, 1.0]:
		centres.append(Vector3(EYE_APART * side, HEAD_HEIGHT, -HEAD_SIZE.z / 2.0))
	return centres


## How far forward each thigh swings, in radians, for a player crouched by
## `crouch`, with the shin swinging back by as much again: just enough to
## bring the hips down by the drop with the feet still on the ground.
static func knee_bend(crouch := 0.0) -> float:
	return acos((HIP_HEIGHT - CROUCH_DROP * clampf(crouch, 0.0, 1.0)) / HIP_HEIGHT)


## The parts that can be hit, for a player crouched by `crouch`, in the
## body's measurements before they are turned or lean: [box, whether it is
## the head] for each.
static func parts(crouch := 0.0) -> Array:
	var drop := Vector3.DOWN * CROUCH_DROP * clampf(crouch, 0.0, 1.0)
	var result := [
		[AABB(HEAD_CENTRE - HEAD_SIZE / 2.0 + drop, HEAD_SIZE), true],
		[AABB(TORSO_CENTRE - TORSO_SIZE / 2.0 + drop, TORSO_SIZE), false],
	]
	# A bent leg reaches from the hip down to the ground, its knee out in
	# front.
	var knee := THIGH * sin(knee_bend(crouch))
	for leg: Array in LEGS:
		var hip: Vector3 = leg[0] + drop
		var corner := Vector3(hip.x - LEG_WIDTH / 2.0, 0.0, -LEG_DEPTH / 2.0 - knee)
		result.append([AABB(corner, Vector3(LEG_WIDTH, hip.y, LEG_DEPTH + knee)), false])
	return result


## Where a ray from `origin` in direction `direction` (normalized) first touches
## a player standing at `feet` and turned by `yaw`, as {distance, head}; or an
## empty dictionary if it misses. `crouch` is how far down they are, from 0
## standing to 1 crouched, and `lean` how far they lean, from -1 to their left
## to 1 to their right. A ray doesn't hit a part it starts inside.
static func ray_hit(origin: Vector3, direction: Vector3, feet: Vector3, yaw := 0.0, crouch := 0.0, lean := 0.0) -> Dictionary:
	# Into the body's own measurements, where it stands at the origin facing
	# -Z, upright.
	var unturn := lean_basis(lean).inverse() * Basis(Vector3.UP, -yaw)
	var local := unturn * (origin - feet) / SIZE
	var along := unturn * direction
	var nearest := INF
	var head := false
	for part: Array in parts(crouch):
		var t := _ray_box(local, along, part[0])
		if t < nearest:
			nearest = t
			head = part[1]
	if nearest == INF:
		return {}
	return {"distance": nearest * SIZE, "head": head}


## How a player leaning by `lean` (-1 to their left, 1 to their right) is
## tipped, pivoting on their feet.
static func lean_basis(lean: float) -> Basis:
	return Basis(Vector3.BACK, -LEAN_ANGLE * clampf(lean, -1.0, 1.0))


## Where a player crouched by `crouch` and leaning by `lean` sees from, from
## their feet, in the body's measurements and before they are turned.
static func head_offset(crouch := 0.0, lean := 0.0) -> Vector3:
	return lean_basis(lean) * Vector3(0, head_height(crouch), 0)


## How high above their feet a player crouched by `crouch` sees from, in the
## body's measurements.
static func head_height(crouch := 0.0) -> float:
	return HEAD_HEIGHT - CROUCH_DROP * clampf(crouch, 0.0, 1.0)


## The point up the middle of a player standing at `feet` that is nearest
## `at`: where a burst at `at` is measured to. The middle runs up the core of
## their capsule, a radius in from its ends, so it is never down at the floor
## a burst lies on.
static func nearest_middle(feet: Vector3, at: Vector3) -> Vector3:
	return feet + Vector3.UP * clampf(at.y - feet.y, RADIUS, HEIGHT - RADIUS)


## How far along the ray `local` + t * `direction` it enters `box`, or INF.
## The ray is inside the box between entering the last of its three slabs it
## crosses into and leaving the first it crosses out of.
static func _ray_box(local: Vector3, direction: Vector3, box: AABB) -> float:
	var enter := -INF
	var leave := INF
	for axis in 3:
		var low := box.position[axis]
		var high := box.end[axis]
		if absf(direction[axis]) < 1e-9:
			if local[axis] < low or local[axis] > high:
				return INF  # Alongside the slab, outside it.
			continue
		var t1 := (low - local[axis]) / direction[axis]
		var t2 := (high - local[axis]) / direction[axis]
		enter = maxf(enter, minf(t1, t2))
		leave = minf(leave, maxf(t1, t2))
	if enter > leave or enter < 0.0:
		return INF
	return enter
