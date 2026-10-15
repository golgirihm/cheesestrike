class_name MouseRules
extends RefCounted
## The giant mice. A minute into a match they come in over the edge of the
## arena, and more after them, and eat the cheese: each makes for the nearest
## piece low down, eats it bite by bite until it is gone, and moves on to the
## next. So a map that began full of cover has less and less of it as the
## match goes on, and nowhere to hide stays hidden for long.
##
## A mouse that is shot runs off for a while, which keeps a player's cover
## standing a little longer at the cost of being heard firing. Mice hurt
## nobody, and nothing stops them: they walk through whatever is in the way.
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
## How many come at first, how often another follows, and the most there are
## for so many blocks of cheese, from a pair on a small arena to a pack on a
## large map.
const FIRST := 2
const MORE_EVERY := 30.0
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
## after, "flee": seconds of running left, "bite": seconds to its next bite}.
var mice: Array[Dictionary] = []
var _next_in := MORE_EVERY
var _rng := RandomNumberGenerator.new()


func _init(seed := 0) -> void:
	_rng.seed = seed


## Back to before a match: no mice, and the countdown from the start.
func reset() -> void:
	invade_in = INVADE_AFTER
	arrived = false
	mice.clear()
	_next_in = MORE_EVERY


## The most mice there are in an arena with `blocks` blocks of cheese.
static func most_for(blocks: int) -> int:
	return clampi(1 + blocks / BLOCKS_A_MOUSE, FIRST, MOST)


## Moves the mice on by `delta` seconds, once the countdown (see
## MatchState.tick) has run out: they come, more follow, and each goes to
## its cheese and eats it, or runs. `blocks` is the cheese and `boxes` the
## space each block fills, `edge` the arena's edge across X and Z (see
## Scenery.bounds), where they come in.
##
## Returns {"bites": [{"index", "amount"}], the cheese eaten, for the host to
## take away; "came": how many mice came now}.
func tick(delta: float, blocks: BlockRules, boxes: Array, edge: Rect2) -> Dictionary:
	var done := {"bites": [], "came": 0}
	if invade_in > 0.0 or blocks.health.is_empty():
		return done
	if not arrived:
		arrived = true
		for count in FIRST:
			_come(edge)
		done.came = FIRST
	else:
		_next_in -= delta
		if _next_in <= 0.0:
			_next_in = MORE_EVERY
			if mice.size() < most_for(blocks.health.size()):
				_come(edge)
				done.came = 1
	for index in mice.size():
		var mouse: Dictionary = mice[index]
		if mouse.flee > 0.0:
			_run(mouse, delta, edge)
			continue
		if not blocks.standing(mouse.target):
			mouse.target = _choose(index, blocks, boxes)
		if mouse.target == -1:
			mouse.mode = WALKING
			continue
		var box: AABB = boxes[mouse.target]
		var nearest := Vector3(clampf(mouse.at.x, box.position.x, box.end.x), 0.0, clampf(mouse.at.z, box.position.z, box.end.z))
		var off: Vector3 = nearest - Vector3(mouse.at.x, 0.0, mouse.at.z)
		# Up, or down, to the bottom of what it is after.
		mouse.at.y = move_toward(mouse.at.y, box.position.y, CLIMB * delta)
		if off.length() > 0.01:
			mouse.heading = atan2(-off.x, -off.z)
		if off.length() > REACH:
			mouse.mode = WALKING
			mouse.at += off.normalized() * minf(SPEED * delta, off.length() - REACH)
			continue
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
	mouse.target = -1
	return not was_running



# A mouse comes in from somewhere along the edge, facing in.
func _come(edge: Rect2) -> void:
	var inside := edge.grow(-LENGTH / 2.0)
	var along := _rng.randf()
	var side := _rng.randi_range(0, 3)
	var at := Vector2.ZERO
	match side:
		0:
			at = Vector2(lerpf(inside.position.x, inside.end.x, along), inside.position.y)
		1:
			at = Vector2(inside.end.x, lerpf(inside.position.y, inside.end.y, along))
		2:
			at = Vector2(lerpf(inside.position.x, inside.end.x, along), inside.end.y)
		_:
			at = Vector2(inside.position.x, lerpf(inside.position.y, inside.end.y, along))
	var toward := inside.get_center() - at
	mice.append({"at": Vector3(at.x, 0.0, at.y), "heading": atan2(-toward.x, -toward.y), "mode": WALKING, "target": -1, "flee": 0.0, "bite": BITE_EVERY})


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
	for block in boxes.size():
		if not blocks.standing(block):
			continue
		var box: AABB = boxes[block]
		var across := Vector2(clampf(at.x, box.position.x, box.end.x) - at.x, clampf(at.z, box.position.z, box.end.z) - at.z).length()
		var cost := across + box.position.y * HEIGHT_COST + (TAKEN_COST if taken.has(block) else 0.0)
		if cost < cheapest:
			cheapest = cost
			best = block
	return best


# A scared mouse runs the way it faces, turning back at the edge, and comes
# down to the ground; when it has run far enough it goes back to eating.
func _run(mouse: Dictionary, delta: float, edge: Rect2) -> void:
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
		next = mouse.at + way * FLEE_SPEED * delta
	next.x = clampf(next.x, inside.position.x, inside.end.x)
	next.z = clampf(next.z, inside.position.y, inside.end.y)
	next.y = move_toward(mouse.at.y, 0.0, CLIMB * 2.0 * delta)
	mouse.at = next


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
			rules.mice.append({"at": at, "heading": each[start + 3], "mode": clampi(int(each[start + 4]), WALKING, FLEEING), "target": -1, "flee": 0.0, "bite": BITE_EVERY})
	return rules
