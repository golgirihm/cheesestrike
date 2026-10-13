class_name ZoneRules
extends RefCounted
## Zones to stand in, for two ways to play.
##
## Hold the hill: one zone at a time is the hill, and it moves on round the
## arena every so often. A team scores for each second it alone has someone
## standing there.
##
## Three points: three zones, each belonging to whichever team last stood in
## it alone for long enough to take it. Each scores for its owners for as
## long as they keep it.
##
## Kept free of nodes so the rules can be tested alone: the host tells it
## where everyone stands, and adds the points it says were earned.

## How far from a zone's middle counts as in it, across and up.
const RADIUS := 6.0
const HEIGHT := 4.0
## Seconds the hill stays in one place.
const HILL_TIME := 40.0
## Points a second for the team holding the hill.
const HILL_RATE := 1.0
## Seconds a team must stand alone in a point to take it, and points a
## second each point is worth to its owners.
const TAKE_TIME := 4.0
const POINT_RATE := 0.5
const NOBODY := -1
## Both teams are in the zone, so neither has it.
const CONTESTED := -2
const NAMES := ["A", "B", "C", "D", "E"]

## Which zone is the hill, and seconds until it moves.
var hill := 0
var hill_left := HILL_TIME
## For each point: the team that owns it (or NOBODY), the team taking it (or
## NOBODY), and how far along they are, 0 to 1.
var owners: Array[int] = []
var taking: Array[int] = []
var progress: Array[float] = []
# Parts of a point not yet added to each team's score.
var _owed := [0.0, 0.0]


func _init(count := 3) -> void:
	reset(count)


func reset(count: int) -> void:
	hill = 0
	hill_left = HILL_TIME
	_owed = [0.0, 0.0]
	owners.clear()
	taking.clear()
	progress.clear()
	for index in count:
		owners.append(NOBODY)
		taking.append(NOBODY)
		progress.append(0.0)


## Which team alone has someone in play standing in the zone at `spot`:
## a team, NOBODY, or CONTESTED. `standing` maps each player's id to
## {"at": their feet, "team", "alive"}.
static func holder(spot: Vector3, standing: Dictionary) -> int:
	var found := NOBODY
	for player: Dictionary in standing.values():
		if not player.alive or player.team < 0 or not inside(spot, player.at):
			continue
		if found != NOBODY and found != player.team:
			return CONTESTED
		found = player.team
	return found


static func inside(spot: Vector3, feet: Vector3) -> bool:
	return Vector2(feet.x - spot.x, feet.z - spot.z).length() <= RADIUS and feet.y - spot.y >= -0.5 and feet.y - spot.y <= HEIGHT


## Hold the hill, moved on by `delta` seconds, with the hill at one of
## `spots` in turn. Returns {"points": whole points earned by each team,
## Cheddar first, "moved": whether the hill has just moved}.
func update_hill(delta: float, spots: Array, standing: Dictionary) -> Dictionary:
	var result := {"points": [0, 0], "moved": false}
	if spots.is_empty():
		return result
	hill = posmod(hill, spots.size())
	var team := holder(spots[hill], standing)
	if team >= 0:
		_owed[team] += delta * HILL_RATE
	result.points = _whole_points()
	hill_left -= delta
	if hill_left <= 0.0:
		hill_left = HILL_TIME
		if spots.size() > 1:
			hill = (hill + 1) % spots.size()
			result.moved = true
	return result


## Three points, moved on by `delta` seconds, with a point at each of
## `spots`. Returns {"points": whole points earned by each team, "taken":
## [[point, team], …] for each point that has just changed hands}.
func update_points(delta: float, spots: Array, standing: Dictionary) -> Dictionary:
	var result := {"points": [0, 0], "taken": []}
	for index in mini(spots.size(), owners.size()):
		var team := holder(spots[index], standing)
		if team >= 0 and team != owners[index]:
			if taking[index] != team:
				taking[index] = team
				progress[index] = 0.0
			progress[index] += delta / TAKE_TIME
			if progress[index] >= 1.0:
				owners[index] = team
				taking[index] = NOBODY
				progress[index] = 0.0
				result.taken.append([index, team])
		elif team != CONTESTED:
			# Nobody is taking it now: what was done comes undone.
			progress[index] = maxf(progress[index] - delta / TAKE_TIME, 0.0)
			if progress[index] == 0.0:
				taking[index] = NOBODY
		if owners[index] >= 0:
			_owed[owners[index]] += delta * POINT_RATE
	result.points = _whole_points()
	return result


func _whole_points() -> Array:
	var points := [0, 0]
	for team in 2:
		points[team] = floori(_owed[team])
		_owed[team] -= points[team]
	return points


## What the points say for themselves, such as "A Cheddar · B – · C Brie".
func points_text() -> String:
	var parts: Array[String] = []
	for index in owners.size():
		parts.append("%s %s" % [NAMES[index], MatchState.TEAM_NAMES[owners[index]] if owners[index] >= 0 else "–"])
	return " · ".join(parts)


func to_array() -> Array:
	return [hill, hill_left, owners, taking, progress]


static func from_array(data: Variant) -> ZoneRules:
	var rules := ZoneRules.new(0)
	if not data is Array or data.size() != 5 or not data[0] is int or not (data[1] is float or data[1] is int):
		return ZoneRules.new()
	if not data[2] is Array or not data[3] is Array or not data[4] is Array or data[2].size() != data[3].size() or data[2].size() != data[4].size() or data[2].size() > NAMES.size():
		return ZoneRules.new()
	rules.hill = maxi(data[0], 0)
	rules.hill_left = clampf(float(data[1]), 0.0, HILL_TIME) if is_finite(float(data[1])) else HILL_TIME
	for index in data[2].size():
		var owner: Variant = data[2][index]
		var taker: Variant = data[3][index]
		var done: Variant = data[4][index]
		rules.owners.append(owner if owner is int and owner >= NOBODY and owner <= 1 else NOBODY)
		rules.taking.append(taker if taker is int and taker >= NOBODY and taker <= 1 else NOBODY)
		rules.progress.append(clampf(float(done), 0.0, 1.0) if (done is float or done is int) and is_finite(float(done)) else 0.0)
	return rules
