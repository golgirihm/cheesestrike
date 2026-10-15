class_name Routes
extends RefCounted
## The ways across a large map that a computer player can't see for itself:
## the street corners and the doorways of its buildings, each a point, and
## which of them can be walked straight between. A bot that wants to get
## somewhere far off goes from point to point along the shortest way, and
## only walks straight at what it wants once it is near.
##
## A map built by BigMap lists its own as its `routes` metadata:
## {"points": [Vector3], "links": [[from, to]]}. An arena small enough to
## cross at a glance has none, and its bots walk straight.
##
## Kept free of nodes so that the ways can be tested alone.

## Nearer than this, a bot walks straight at what it wants.
const NEAR := 40.0
## How near a point counts as reached, so that the next is made for.
const REACHED := 4.0

var points: Array[Vector3] = []
# For each point, the points it is linked to.
var _links: Array[PackedInt32Array] = []


func _init(data: Dictionary = {}) -> void:
	for point: Variant in data.get("points", []):
		if point is Vector3:
			points.append(point)
	_links.resize(points.size())
	for index in points.size():
		_links[index] = PackedInt32Array()
	for link: Variant in data.get("links", []):
		if link is Array and link.size() == 2:
			connect_points(int(link[0]), int(link[1]))


func is_empty() -> bool:
	return points.is_empty()


## Links points `a` and `b` both ways.
func connect_points(a: int, b: int) -> void:
	if a == b or a < 0 or b < 0 or a >= points.size() or b >= points.size() or _links[a].has(b):
		return
	_links[a].append(b)
	_links[b].append(a)


func linked(a: int) -> PackedInt32Array:
	return _links[a] if a >= 0 and a < _links.size() else PackedInt32Array()


## The point nearest `at`, measured across the ground with a floor's
## difference counting double; -1 with none.
func nearest(at: Vector3) -> int:
	var best := -1
	var least := INF
	for index in points.size():
		var off := points[index] - at
		var distance := Vector3(off.x, off.y * 2.0, off.z).length()
		if distance < least:
			least = distance
			best = index
	return best


## The points to walk through, in order, to get from `from` to `to`: the
## shortest way along the links, from the point nearest one to the point
## nearest the other. Empty when they are near enough to walk straight, or
## there is no way.
func way(from: Vector3, to: Vector3) -> Array[Vector3]:
	var found: Array[Vector3] = []
	if is_empty() or Vector2(to.x - from.x, to.z - from.z).length() <= NEAR:
		return found
	var start := nearest(from)
	var end := nearest(to)
	for index in shortest(start, end):
		found.append(points[index])
	# Already at the first, it is the next that is wanted.
	while found.size() > 1 and Vector2(found[0].x - from.x, found[0].z - from.z).length() <= REACHED:
		found.pop_front()
	return found


## The indices of the points on the shortest way from point `start` to point
## `end`, both included; empty if there is no way.
func shortest(start: int, end: int) -> Array[int]:
	var path: Array[int] = []
	if start < 0 or end < 0 or start >= points.size() or end >= points.size():
		return path
	var cost: Array[float] = []
	var came: Array[int] = []
	cost.resize(points.size())
	came.resize(points.size())
	cost.fill(INF)
	came.fill(-1)
	cost[start] = 0.0
	var open: Array[int] = [start]
	while not open.is_empty():
		# The cheapest guess at the whole way first: few enough points that a
		# plain list will do.
		var pick := 0
		for index in range(1, open.size()):
			if cost[open[index]] + points[open[index]].distance_to(points[end]) < cost[open[pick]] + points[open[pick]].distance_to(points[end]):
				pick = index
		var here: int = open[pick]
		open.remove_at(pick)
		if here == end:
			break
		for next: int in _links[here]:
			var through := cost[here] + points[here].distance_to(points[next])
			if through < cost[next]:
				cost[next] = through
				came[next] = here
				if not open.has(next):
					open.append(next)
	if cost[end] == INF:
		return path
	var at := end
	while at != -1:
		path.push_front(at)
		at = came[at]
	return path


## How far the shortest way between points `start` and `end` is, in metres;
## INF with none.
func distance(start: int, end: int) -> float:
	var path := shortest(start, end)
	if path.is_empty():
		return INF
	var total := 0.0
	for index in range(1, path.size()):
		total += points[path[index - 1]].distance_to(points[path[index]])
	return total
