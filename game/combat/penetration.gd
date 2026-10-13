class_name Penetration
## Shots go through thin cover. A shot that meets something no thicker than
## MAX_THICKNESS along its way carries on behind it, doing less damage; it
## stops at anything thicker, and at the second thing it meets either way.
## So how much cover is worth depends on how solid it is.
##
## Worked out from rays cast into the arena by whoever asks, so that the
## host, the shooter's own display and the tests all use the same rule.

## The thickest thing a shot passes through, in metres along its path.
const MAX_THICKNESS := 0.5
## The share of its damage a shot still does once it has.
const DAMAGE_SHARE := 0.6
# How far past the most it could pass through to look back from, for the
# far side of what a shot met.
const _PROBE := 0.02


## The way of a shot from `origin` along `direction` (normalized) of up to
## `max_range`: {"first": how far to the first thing it meets (`max_range` if
## nothing), "reach": how far it gets in all, "through": whether it passed
## through that first thing}. Past "first", it does DAMAGE_SHARE of its damage.
##
## `cast` is a Callable(from, to) giving how far along the segment the arena
## is first met, or INF. It must not count anything the segment starts inside.
static func path(origin: Vector3, direction: Vector3, max_range: float, cast: Callable) -> Dictionary:
	var first: float = minf(cast.call(origin, origin + direction * max_range), max_range)
	var blocked := {"first": first, "reach": first, "through": false}
	if first >= max_range:
		return blocked
	# Look back from just past the most it could go through: from open air,
	# that finds the far side of what it met; from inside it, nothing.
	var entry := origin + direction * first
	var beyond := entry + direction * (MAX_THICKNESS + _PROBE)
	var back: float = cast.call(beyond, entry)
	if back == INF or back >= MAX_THICKNESS + _PROBE:
		return blocked
	var thickness := MAX_THICKNESS + _PROBE - back
	if thickness > MAX_THICKNESS:
		return blocked
	var exit := first + thickness + 0.001
	if exit >= max_range:
		return blocked
	var onward: float = cast.call(origin + direction * exit, origin + direction * max_range)
	return {"first": first, "reach": minf(exit + onward, max_range), "through": true}


## The share of its damage a shot on the way `way` (from path) does to
## something `distance` along it.
static func share(way: Dictionary, distance: float) -> float:
	return DAMAGE_SHARE if way.through and distance > way.first else 1.0
