class_name Acoustics
## Space: how a sound is changed by where it is made and where it is heard
## from. A shot indoors rings round the room, one in an alley slaps back
## off the walls on either side, and one in the open just goes. A sound with
## a wall between it and the listener is muffled, and a shot far off loses
## its top and is a dull crack.

const OPEN := "Open"
const ALLEY := "Alley"
const INDOORS := "Indoors"
const PLACES := [OPEN, ALLEY, INDOORS]

## How far overhead something counts as a roof, and how far to the side as a
## wall, in metres.
const ROOF_WITHIN := 14.0
const WALL_WITHIN := 9.0
## How many ways round a place is looked at for walls.
const WAYS := 8
## Indoors has a roof and walls on at least this many of the ways.
const WALLS_FOR_INDOORS := 4

## A sound loses its top with distance: from all of it at FAR_FROM metres
## to only what is under FAR_CUTOFF hertz at FAR_TO.
const CLEAR_CUTOFF := 20500.0
const FAR_FROM := 28.0
const FAR_TO := 95.0
const FAR_CUTOFF := 1700.0
## Behind a wall, only what is under this is heard, and it is this much
## quieter, in decibels.
const MUFFLED_CUTOFF := 850.0
const MUFFLED_DB := -7.0
## How strongly the top is taken off, in decibels: more when muffled.
const FILTER_DB := -24.0
const MUFFLED_FILTER_DB := -40.0
## Height above the feet that a place is looked at from, and the layer of the
## arena's solid things.
const EAR := 1.6
const WORLD_MASK := 1
## Places are looked at once for each square this many metres across.
const CELL := 3.0

static var _known := {}
static var _known_for := 0


## What kind of place it is with a roof overhead or not (`roofed`) and with
## walls near on each of the ways round (`walls`, one for each, going
## round): indoors, an alley, or the open.
static func classify(roofed: bool, walls: Array) -> String:
	var near := walls.count(true)
	if roofed and near >= WALLS_FOR_INDOORS:
		return INDOORS
	# An alley has walls on both sides of some way through it, or a roof
	# with open sides, as under an arch.
	for way in walls.size() / 2:
		if walls[way] and walls[way + walls.size() / 2]:
			return ALLEY
	return ALLEY if roofed else OPEN


## What kind of place `at` is in the arena whose solid things are in `space`.
static func survey(space: PhysicsDirectSpaceState3D, at: Vector3) -> String:
	var ear := at + Vector3.UP * EAR
	var walls := []
	for way in WAYS:
		var along := Vector3.FORWARD.rotated(Vector3.UP, TAU * way / WAYS)
		walls.append(_blocked(space, ear, ear + along * WALL_WITHIN))
	return classify(_blocked(space, ear, ear + Vector3.UP * ROOF_WITHIN), walls)


## The same, remembered for each square of the arena `map`, so that it is
## only worked out once for each.
static func place_at(map: Node3D, at: Vector3) -> String:
	if map == null or not map.is_inside_tree():
		return OPEN
	if _known_for != map.get_instance_id():
		_known.clear()
		_known_for = map.get_instance_id()
	var cell := Vector3i((at / CELL).floor())
	if not _known.has(cell):
		_known[cell] = survey(map.get_world_3d().direct_space_state, at)
	return _known[cell]


## Whether something solid is between a sound at `from` and a listener at
## `to`.
static func muffled(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3) -> bool:
	return _blocked(space, from, to)


## How much of the top of a sound `distance` metres off is heard: the
## frequency in hertz above which it is taken away.
static func cutoff(distance: float, blocked := false) -> float:
	var along := clampf(inverse_lerp(FAR_FROM, FAR_TO, distance), 0.0, 1.0)
	# Evenly by ear, which hears in octaves.
	var far := exp(lerpf(log(CLEAR_CUTOFF), log(FAR_CUTOFF), along)) if along > 0.0 else CLEAR_CUTOFF
	return minf(far, MUFFLED_CUTOFF) if blocked else far


## How a sound on `bus`, made in a place of kind `place` and heard from
## `distance` metres with something in the way or not (`blocked`), is
## played: {"bus": the bus it goes through, "cutoff": hertz, "filter_db",
## "volume_db": added to its own}.
static func treatment(bus: String, place: String, distance: float, blocked: bool) -> Dictionary:
	return {
		"bus": Mix.treated(bus, place),
		"cutoff": cutoff(distance, blocked),
		"filter_db": MUFFLED_FILTER_DB if blocked else FILTER_DB,
		"volume_db": MUFFLED_DB if blocked else 0.0,
	}


# Whether something solid, other than the unseen edge of the arena, is
# between `from` and `to`.
static func _blocked(space: PhysicsDirectSpaceState3D, from: Vector3, to: Vector3) -> bool:
	var hit := space.intersect_ray(PhysicsRayQueryParameters3D.create(from, to, WORLD_MASK))
	return not hit.is_empty() and not String(hit.collider.name).begins_with("Wall")
