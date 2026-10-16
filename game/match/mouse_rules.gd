class_name MouseRules
extends RefCounted
## The giant mice. A minute into a match they all come in together over the
## edge of the arena, and eat the cheese: each makes for the nearest
## piece low down, eats it bite by bite until it is gone, and moves on to the
## next. So a map that began full of cover has less and less of it as the
## match goes on, and nowhere to hide stays hidden for long.
##
## A mouse that is shot enough runs off for a while, which keeps a player's
## cover standing a little longer at the cost of being heard firing. Mice hurt
## nobody. They eat their way through cheese, climb over what is low, and go
## round what is tall; one that can't get to its cheese tries other cheese.
##
## The host runs them. Everyone is sent where each is and what it is doing,
## and how long until they come, which every machine counts down for itself
## between updates (see MatchState.tick).
##
## Kept free of nodes so the rules can be tested alone: the host gives it the
## cheese as BlockRules has it and the space each block fills, and takes away
## the bites it says were taken.

## Seconds of a match before the mice come, with a countdown to it.
const INVADE_AFTER := 60.0
## How many come, for so many blocks of cheese: from a pair on a small arena
## to a pack on a large map.
const FIRST := 2
const MOST := 8
const BLOCKS_A_MOUSE := 60
## How big a mouse is, in metres: a player is 5.4 tall.
const LENGTH := 12.0
const HEIGHT := 6.0
## Where a shot counts as hitting a mouse: within this of the middle of it.
const HIT_RADIUS := 4.5
## How fast a mouse walks, climbs up to what it is after, and runs when
## scared, in metres a second; and for how long it runs.
const SPEED := 8.0
const CLIMB := 4.0
const FLEE_SPEED := 18.0
const FLEE_TIME := 6.0
## How much a mouse is hurt by before it runs, as a player's health is 100,
## and how fast that wears off, a second.
const BRAVERY := 300.0
const CALM := 20.0
## What a mouse steps up onto rather than going round, in metres above its
## feet; how far ahead it looks for what is in its way; and how long it goes
## without getting nearer its cheese before trying other cheese, and for how
## long it leaves that cheese be.
const STEP := 3.0
const LOOK := 4.0
const GIVE_UP := 8.0
const AVOID_FOR := 30.0
## How near the middle of a mouse must be to a block, across the ground, to
## eat it; and how much it takes a bite, how often. A block lasts five
## seconds of one mouse.
const REACH := LENGTH / 2.0
const BITE := 40
const BITE_EVERY := 0.5
## What a block is worth, as a meal: metres away across the ground, and so
## many more for each metre up it is, or for another mouse having it.
const HEIGHT_COST := 4.0
const TAKEN_COST := 40.0

## What each mouse is doing, as shown.
const WALKING := 0
const EATING := 1
const FLEEING := 2

## Seconds until the mice come; 0 once they have.
var invade_in := INVADE_AFTER
## Whether they have come yet this match.
var arrived := false
## Each mouse: {"at": Vector3 where its feet are, "heading": the way it faces,
## radians round from -Z, "mode", and on the host "target": the block it is
## after, "flee": seconds of running left, "bite": seconds to its next bite,
## "fear": how much it has been hurt by, "side": which way it goes round
## things, 1 or -1, "nearest" and "stalled": how near it has got to its
## cheese and for how long no nearer, "avoid": blocks it gave up on, with
## seconds left before it tries them again}.
var mice: Array[Dictionary] = []
var _rng := RandomNumberGenerator.new()


func _init(seed := 0) -> void:
	_rng.seed = seed


## Back to before a match: no mice, and the countdown from the start.
func reset() -> void:
	invade_in = INVADE_AFTER
	arrived = false
	mice.clear()


## The most mice there are in an arena with `blocks` blocks of cheese.
static func most_for(blocks: int) -> int:
	return clampi(1 + blocks / BLOCKS_A_MOUSE, FIRST, MOST)


## Moves the mice on by `delta` seconds, once the countdown (see
## MatchState.tick) has run out: they come, and each goes to its cheese and
## eats it, or runs. `blocks` is the cheese and `boxes` the space each block
## fills, `edge` the arena's edge across X and Z (see Scenery.bounds), where
## they come in, and `solids` the space each thing that isn't cheese fills,
## which they climb onto or go round.
##
## Returns {"bites": [{"index", "amount"}], the cheese eaten, for the host to
## take away; "came": how many mice came now}.
func tick(delta: float, blocks: BlockRules, boxes: Array, edge: Rect2, solids: Array = []) -> Dictionary:
	var done := {"bites": [], "came": 0}
	if invade_in > 0.0 or blocks.health.is_empty():
		return done
	if not arrived:
		arrived = true
		var count := most_for(blocks.health.size())
		# Spread round the edge, so they come from all sides at once.
		var start := _rng.randf()
		for each in count:
			_come(edge, fposmod(start + (each + _rng.randf_range(0.2, 0.8)) / count, 1.0))
		done.came = count
	for index in mice.size():
		var mouse: Dictionary = mice[index]
		mouse.fear = maxf(mouse.fear - CALM * delta, 0.0)
		for block: int in mouse.avoid.keys():
			mouse.avoid[block] -= delta
			if mouse.avoid[block] <= 0.0:
				mouse.avoid.erase(block)
		var near := _near_solids(mouse.at, solids)
		if mouse.flee > 0.0:
			_run(mouse, delta, edge, near)
			continue
		if not blocks.standing(mouse.target):
			_aim(mouse, _choose(index, blocks, boxes))
		if mouse.target == -1:
			mouse.mode = WALKING
			mouse.at.y = move_toward(mouse.at.y, _ground(mouse.at, near), CLIMB * delta)
			continue
		var box: AABB = boxes[mouse.target]
		var nearest := Vector3(clampf(mouse.at.x, box.position.x, box.end.x), 0.0, clampf(mouse.at.z, box.position.z, box.end.z))
		var off: Vector3 = nearest - Vector3(mouse.at.x, 0.0, mouse.at.z)
		if off.length() > 0.01:
			mouse.heading = atan2(-off.x, -off.z)
		if off.length() > REACH:
			mouse.mode = WALKING
			# A little inside its reach, so it gets there for sure.
			var moved := _step(mouse, off.normalized(), minf(SPEED * delta, off.length() - REACH + 0.1), near)
			mouse.at.y = move_toward(mouse.at.y, _ground(mouse.at, near), CLIMB * delta)
			if off.length() < mouse.nearest - 0.5:
				mouse.nearest = off.length()
				mouse.stalled = 0.0
			else:
				mouse.stalled += delta
			if mouse.stalled > GIVE_UP or not moved and mouse.stalled > GIVE_UP / 2.0:
				# Can't get to it: other cheese, for a while.
				mouse.avoid[mouse.target] = AVOID_FOR
				_aim(mouse, _choose(index, blocks, boxes))
			continue
		# Up, or down, to the bottom of what it is after.
		mouse.at.y = move_toward(mouse.at.y, box.position.y, CLIMB * delta)
		mouse.mode = EATING
		mouse.bite -= delta
		if mouse.bite <= 0.0:
			mouse.bite += BITE_EVERY
			done.bites.append({"index": mouse.target, "amount": BITE})
	return done


## The mouse a shot from `origin` along `direction` hits before it has gone
## `reach`, or -1 for none: the nearest.
func hit(origin: Vector3, direction: Vector3, reach: float) -> int:
	var found := -1
	var nearest := reach
	for index in mice.size():
		var middle: Vector3 = mice[index].at + Vector3.UP * HEIGHT / 2.0
		var along := (middle - origin).dot(direction)
		if along < 0.0 or along > nearest:
			continue
		if (origin + direction * along).distance_to(middle) <= HIT_RADIUS:
			found = index
			nearest = along
	return found


## The mice within `radius` of `at`, as a burst there reaches them.
func near(at: Vector3, radius: float) -> Array[int]:
	var found: Array[int] = []
	for index in mice.size():
		if (mice[index].at + Vector3.UP * HEIGHT / 2.0).distance_to(at) <= radius + HIT_RADIUS:
			found.append(index)
	return found


## Mouse `index` is hurt by `amount`, from `from`. Returns whether that
## scares it off (see scare()): it runs once it has been hurt by BRAVERY,
## less what has worn off.
func hurt(index: int, amount: float, from: Vector3) -> bool:
	if index < 0 or index >= mice.size() or not is_finite(amount) or amount <= 0.0:
		return false
	var mouse: Dictionary = mice[index]
	if mouse.flee > 0.0:
		return false
	mouse.fear += amount
	if mouse.fear < BRAVERY:
		return false
	return scare(index, from)


## Mouse `index` runs off for a while, away from `from`, leaving what it was
## eating. Returns whether it is one that wasn't running already.
func scare(index: int, from: Vector3) -> bool:
	if index < 0 or index >= mice.size():
		return false
	var mouse: Dictionary = mice[index]
	var was_running: bool = mouse.flee > 0.0
	var away: Vector3 = mouse.at - from
	away.y = 0.0
	if away.length() < 0.01:
		away = Vector3.FORWARD.rotated(Vector3.UP, mouse.heading)
	mouse.heading = atan2(-away.x, -away.z)
	mouse.flee = FLEE_TIME
	mouse.mode = FLEEING
	mouse.fear = 0.0
	_aim(mouse, -1)
	return not was_running


# A mouse comes in at `around`, 0 to 1 of the way round the edge, facing in.
func _come(edge: Rect2, around: float) -> void:
	var inside := edge.grow(-LENGTH / 2.0)
	var way := around * 2.0 * (inside.size.x + inside.size.y)
	var at := inside.position
	if way < inside.size.x:
		at += Vector2(way, 0.0)
	elif way < inside.size.x + inside.size.y:
		at += Vector2(inside.size.x, way - inside.size.x)
	elif way < 2.0 * inside.size.x + inside.size.y:
		at += Vector2(2.0 * inside.size.x + inside.size.y - way, inside.size.y)
	else:
		at += Vector2(0.0, 2.0 * (inside.size.x + inside.size.y) - way)
	var toward := inside.get_center() - at
	var mouse := _mouse(Vector3(at.x, 0.0, at.y), atan2(-toward.x, -toward.y), WALKING)
	mouse.side = 1 if _rng.randf() < 0.5 else -1
	mice.append(mouse)


static func _mouse(at: Vector3, heading: float, mode: int) -> Dictionary:
	return {"at": at, "heading": heading, "mode": mode, "target": -1, "flee": 0.0, "bite": BITE_EVERY,
			"fear": 0.0, "side": 1, "nearest": INF, "stalled": 0.0, "avoid": {}}


# The mouse makes for block `block`, from scratch.
static func _aim(mouse: Dictionary, block: int) -> void:
	mouse.target = block
	mouse.nearest = INF
	mouse.stalled = 0.0


# The things that aren't cheese near `at`, worth looking at as it moves.
static func _near_solids(at: Vector3, solids: Array) -> Array:
	var near := []
	var reach := Vector3(LENGTH + LOOK, LENGTH, LENGTH + LOOK)
	var around := AABB(at - reach, reach * 2.0)
	for solid: AABB in solids:
		if solid.intersects(around):
			near.append(solid)
	return near


# The room a mouse with its feet at `at` takes, less what it steps up onto.
static func _body(at: Vector3) -> AABB:
	var half := LENGTH / 4.0
	return AABB(at + Vector3(-half, STEP, -half), Vector3(half * 2.0, HEIGHT - STEP, half * 2.0))


# Whether a mouse at `at` has room there.
static func _room(at: Vector3, solids: Array) -> bool:
	var body := _body(at)
	for solid: AABB in solids:
		if solid.intersects(body):
			return false
	return true


# The top of what a mouse at `at` stands on: the ground, or something low it
# has stepped up onto.
static func _ground(at: Vector3, solids: Array) -> float:
	var half := LENGTH / 4.0
	var under := AABB(Vector3(at.x - half, at.y - STEP, at.z - half), Vector3(half * 2.0, STEP * 2.0, half * 2.0))
	var top := 0.0
	for solid: AABB in solids:
		if solid.intersects(under) and solid.end.y <= at.y + STEP:
			top = maxf(top, solid.end.y)
	return top


# Moves `mouse` `distance` along `way`, or as near that way as is clear,
# going round always on its own side, so it follows what is in its way
# rather than dithering at it; hemmed in on that side, it tries the other.
# Returns whether it could move at all; one already stuck in something moves
# as it likes, to get out.
static func _step(mouse: Dictionary, way: Vector3, distance: float, solids: Array) -> bool:
	if solids.is_empty() or not _room(mouse.at, solids):
		mouse.at += way * distance
		return true
	for side: int in [mouse.side, -mouse.side]:
		for turn: float in [0.0, 30.0, 60.0, 90.0, 120.0, 150.0]:
			var tried := way.rotated(Vector3.UP, deg_to_rad(turn * side))
			if _room(mouse.at + tried * LOOK, solids) and _room(mouse.at + tried * distance, solids):
				mouse.at += tried * distance
				mouse.side = side
				if turn != 0.0:
					mouse.heading = atan2(-tried.x, -tried.z)
				return true
	return false


# The block mouse `index` goes for: the cheapest meal, near, low down, and
# not another mouse's; or -1 with no cheese left.
func _choose(index: int, blocks: BlockRules, boxes: Array) -> int:
	var at: Vector3 = mice[index].at
	var taken := {}
	for other in mice.size():
		if other != index:
			taken[mice[other].target] = true
	var best := -1
	var cheapest := INF
	var avoid: Dictionary = mice[index].avoid
	for block in boxes.size():
		if not blocks.standing(block) or avoid.has(block):
			continue
		var box: AABB = boxes[block]
		var across := Vector2(clampf(at.x, box.position.x, box.end.x) - at.x, clampf(at.z, box.position.z, box.end.z) - at.z).length()
		var cost := across + box.position.y * HEIGHT_COST + (TAKEN_COST if taken.has(block) else 0.0)
		if cost < cheapest:
			cheapest = cost
			best = block
	return best


# A scared mouse runs the way it faces, turning back at the edge and going
# round what is in its way, and comes down to the ground; when it has run far
# enough it goes back to eating.
func _run(mouse: Dictionary, delta: float, edge: Rect2, solids: Array) -> void:
	mouse.flee = maxf(mouse.flee - delta, 0.0)
	mouse.mode = FLEEING if mouse.flee > 0.0 else WALKING
	var way := Vector3.FORWARD.rotated(Vector3.UP, mouse.heading)
	var next: Vector3 = mouse.at + way * FLEE_SPEED * delta
	var inside := edge.grow(-LENGTH / 2.0)
	if not inside.has_point(Vector2(next.x, next.z)):
		# Back the other way along whichever side it ran into.
		if next.x < inside.position.x or next.x > inside.end.x:
			way.x = -way.x
		if next.z < inside.position.y or next.z > inside.end.y:
			way.z = -way.z
		mouse.heading = atan2(-way.x, -way.z)
	if not _step(mouse, way, FLEE_SPEED * delta, solids):
		# Hemmed in: back the way it came.
		mouse.heading = wrapf(mouse.heading + PI, -PI, PI)
	mouse.at.x = clampf(mouse.at.x, inside.position.x, inside.end.x)
	mouse.at.z = clampf(mouse.at.z, inside.position.y, inside.end.y)
	mouse.at.y = move_toward(mouse.at.y, _ground(mouse.at, solids), CLIMB * 2.0 * delta)
	if mouse.flee <= 0.0:
		_aim(mouse, -1)


## What is sent to everyone: the countdown, whether they have come, and each
## mouse's place, heading and what it is doing.
func to_array() -> Array:
	var each := PackedFloat32Array()
	for mouse: Dictionary in mice:
		each.append_array([mouse.at.x, mouse.at.y, mouse.at.z, mouse.heading, mouse.mode])
	return [invade_in, arrived, each]


## The mice `data` describes, with nonsense left out.
static func from_array(data: Variant) -> MouseRules:
	var rules := MouseRules.new()
	if not data is Array or data.size() != 3:
		return rules
	if (data[0] is float or data[0] is int) and is_finite(float(data[0])):
		rules.invade_in = clampf(float(data[0]), 0.0, INVADE_AFTER)
	rules.arrived = data[1] is bool and data[1]
	if data[2] is PackedFloat32Array and data[2].size() % 5 == 0 and data[2].size() <= MOST * 5:
		var each: PackedFloat32Array = data[2]
		for start in range(0, each.size(), 5):
			var at := Vector3(each[start], each[start + 1], each[start + 2])
			if not at.is_finite() or not is_finite(each[start + 3]):
				continue
			rules.mice.append(_mouse(at, each[start + 3], clampi(int(each[start + 4]), WALKING, FLEEING)))
	return rules
