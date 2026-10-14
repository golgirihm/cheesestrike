class_name StickAim
## How a gamepad's stick turns the view: past a dead zone that the player
## sets, along the response curve they choose, and slowed a little while
## the crosshair is over an opponent. The assist only ever slows the view;
## it never turns it toward anyone.

const DEAD_ZONE := "stick_dead_zone"
const CURVE := "stick_curve"
const ASSIST := "aim_assist"
const RUMBLE := "rumble"
## The curves, and how each is called: how far the stick is pushed (past
## the dead zone, from 0 to 1) is raised to this power.
const CURVES := {"linear": 1.0, "classic": 2.0, "precise": 3.0}
const CURVE_NAMES := {"linear": "Even", "classic": "Gentle at first", "precise": "Finest at first"}
## The view turns this share as fast with the crosshair right over someone,
## and normally again once it is this many times their size away.
const ASSIST_SLOWEST := 0.55
const ASSIST_REACH := 2.0
## How wide a player is taken to be, in metres, seen from far off.
const TARGET_RADIUS := 0.45
## Beyond this, in metres, there is no assist.
const ASSIST_RANGE := 60.0


## `stick` (each way from -1 to 1) once the dead zone `dead` and the curve
## called `curve` have had their way: nothing inside the dead zone, and
## from there out to the edge the whole range again.
static func shaped(stick: Vector2, dead: float, curve: String) -> Vector2:
	var pushed := minf(stick.length(), 1.0)
	if pushed <= dead or dead >= 1.0:
		return Vector2.ZERO
	var past := (pushed - dead) / (1.0 - dead)
	return stick.normalized() * pow(past, CURVES.get(curve, 1.0))


## How far, in radians, a player's body `radius` metres wide looks from
## `distance` metres away: half its angular width.
static func size_at(distance: float, radius := TARGET_RADIUS) -> float:
	return atan2(radius, maxf(distance, 0.1))


## The share of the usual speed the view turns at, with the crosshair `off`
## radians from an opponent who looks `size` radians wide (see size_at): 1
## for none.
static func slowdown(off: float, size: float) -> float:
	if size <= 0.0 or off >= size * ASSIST_REACH:
		return 1.0
	return lerpf(ASSIST_SLOWEST, 1.0, clampf((off - size) / (size * (ASSIST_REACH - 1.0)), 0.0, 1.0)) if off > size else ASSIST_SLOWEST


## The slowdown for someone looking along `forward` from `eye`, given the
## places `targets` of the opponents they can see: the nearest to the
## crosshair counts.
static func slowdown_among(eye: Vector3, forward: Vector3, targets: Array) -> float:
	var slowest := 1.0
	for target: Vector3 in targets:
		var distance := eye.distance_to(target)
		if distance > ASSIST_RANGE or distance < 0.1:
			continue
		slowest = minf(slowest, slowdown(forward.angle_to(target - eye), size_at(distance)))
	return slowest


## How strongly, from 0 to 1, and for how long a gamepad rumbles when its
## player takes `dealt` damage.
static func hurt_rumble(dealt: int) -> Vector2:
	return Vector2(clampf(dealt / 60.0, 0.15, 1.0), 0.12 + clampf(dealt / 400.0, 0.0, 0.2))
