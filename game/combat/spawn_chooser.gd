class_name SpawnChooser
## Where a player who has been knocked out comes back. The further from
## anyone who could hurt them the better, so they don't reappear in front of
## whoever just got them; but not somewhere an enemy nearby is looking, nor
## where there has just been fighting, and with teams, nearer their own
## side than not.
##
## Kept free of nodes: the host tells it who is where and what can be seen.

## An enemy this near, with a spot this close to the middle of their view
## (the cosine of the angle off it) and nothing in the way, is watching it.
const WATCH_RANGE := 25.0
const WATCH_CONE := 0.8
## What a spot loses for being watched, for being where there was fighting
## (falling to nothing FIGHT_REACH from it), and gains for a teammate being
## by (falling to nothing ALLY_REACH from one): in metres further from, or
## nearer to, the nearest enemy that it is as good as.
const WATCHED := 20.0
const FOUGHT := 12.0
const FIGHT_REACH := 10.0
const ALLIED := 8.0
const ALLY_REACH := 30.0
## Further from the nearest enemy than this is no better.
const FAR_ENOUGH := 60.0


## The index in `spots` whose nearest player in `others` is farthest away. With
## nobody else about, any spot will do, so one is picked at random.
static func farthest(spots: Array[Vector3], others: Array[Vector3], rng := RandomNumberGenerator.new()) -> int:
	if spots.is_empty():
		return -1
	if others.is_empty():
		return rng.randi_range(0, spots.size() - 1)
	var best := 0
	var best_gap := -1.0
	for i in spots.size():
		var gap := INF
		for other in others:
			gap = minf(gap, spots[i].distance_to(other))
		if gap > best_gap:
			best_gap = gap
			best = i
	return best


## The index of the best of `spots` to come back at.
##
## `enemies`: each {"at": where they stand, "yaw": which way they face}.
## `allies`: where the returning player's teammates stand.
## `fights`: where there has lately been fighting.
## `sees`: Callable(from, to) telling whether nothing stands between two
## points, or an empty Callable if nothing ever does.
##
## With nobody else about and nothing to go by, one is picked at random.
static func best(spots: Array[Vector3], enemies: Array, allies: Array, fights: Array, sees := Callable(), rng := RandomNumberGenerator.new()) -> int:
	if spots.is_empty():
		return -1
	if enemies.is_empty() and allies.is_empty() and fights.is_empty():
		return rng.randi_range(0, spots.size() - 1)
	var chosen := 0
	var most := -INF
	for index in spots.size():
		var worth := score(spots[index], enemies, allies, fights, sees)
		if worth > most:
			most = worth
			chosen = index
	return chosen


## How good `spot` is to come back at: higher is better.
static func score(spot: Vector3, enemies: Array, allies: Array, fights: Array, sees := Callable()) -> float:
	var gap := FAR_ENOUGH
	for enemy: Dictionary in enemies:
		gap = minf(gap, spot.distance_to(enemy.at))
	var worth := gap
	if watched(spot, enemies, sees):
		worth -= WATCHED
	var fought := 0.0
	for fight: Vector3 in fights:
		fought = maxf(fought, 1.0 - spot.distance_to(fight) / FIGHT_REACH)
	worth -= FOUGHT * fought
	var friendly := 0.0
	for ally: Vector3 in allies:
		friendly = maxf(friendly, 1.0 - spot.distance_to(ally) / ALLY_REACH)
	worth += ALLIED * friendly
	return worth


## Whether any of `enemies` is near `spot`, facing it, with nothing between.
static func watched(spot: Vector3, enemies: Array, sees := Callable()) -> bool:
	for enemy: Dictionary in enemies:
		var to: Vector3 = spot - enemy.at
		to.y = 0.0
		var distance := to.length()
		if distance > WATCH_RANGE or distance < 0.01:
			continue
		var facing := Vector3(-sin(enemy.yaw), 0.0, -cos(enemy.yaw))
		if facing.dot(to / distance) < WATCH_CONE:
			continue
		if sees.is_valid() and not sees.call(enemy.at, spot):
			continue
		return true
	return false


## `spots` as each team starts a match on them: [Cheddar's, Brie's], each
## the half of them nearer that team's base (`bases`, Cheddar's first),
## nearest first.
static func by_side(spots: Array[Vector3], bases: Array) -> Array:
	var sides := [[], []]
	var order := spots.duplicate()
	# Those plainest for one side or the other are dealt out first.
	order.sort_custom(func(a: Vector3, b: Vector3) -> bool:
		return absf(a.distance_to(bases[0]) - a.distance_to(bases[1])) > absf(b.distance_to(bases[0]) - b.distance_to(bases[1])))
	var half := ceili(spots.size() / 2.0)
	for spot: Vector3 in order:
		var side := 0 if spot.distance_to(bases[0]) <= spot.distance_to(bases[1]) else 1
		if sides[side].size() >= half:
			side = 1 - side
		sides[side].append(spot)
	for side in 2:
		sides[side].sort_custom(func(a: Vector3, b: Vector3) -> bool: return a.distance_to(bases[side]) < b.distance_to(bases[side]))
	return sides
