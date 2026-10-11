class_name SpawnChooser
## Where a player who has been knocked out comes back: the spot farthest from
## everyone else still in play, so they don't reappear in front of whoever just
## got them.


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
