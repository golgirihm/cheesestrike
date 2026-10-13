class_name Stats
extends RefCounted
## What each player has done in a match beyond knocking others out: the
## damage they dealt, the knock-outs they helped with, what they did for the
## mode's objective, how much of what they fired hit, their best run of
## knock-outs, and what with. The host keeps these and sends them to
## everyone every so often, apart from the state it sends many times a
## second, as nothing in play depends on them.

## Seconds before a knock-out in which hurting the one knocked out counts as
## helping with it.
const ASSIST_WINDOW := 10.0
## What taking the other team's wheel, and bringing it home, are worth to
## the one who did it, in the objective column.
const TAKE_POINTS := 2.0
const CAPTURE_POINTS := 10.0
## The most players a set of stats sent by the host can be for.
const MOST := 64

# Player id -> {"damage", "assists", "objective", "shots", "hits", "streak",
# "best", "with": {weapon: knock-outs}}.
var _lines := {}
# Victim id -> {attacker id: when they last hurt them}.
var _hurt_by := {}
## Whether anything has changed since it was last sent.
var changed := false


## A new line, with nothing done.
static func blank() -> Dictionary:
	return {"damage": 0, "assists": 0, "objective": 0.0, "shots": 0, "hits": 0, "streak": 0, "best": 0, "with": {}}


## What player `id` has done: a copy of their line.
func of(id: int) -> Dictionary:
	return (_lines[id] as Dictionary).duplicate(true) if _lines.has(id) else blank()


## Player `id` fired, threw or struck once.
func shot(id: int) -> void:
	_line(id).shots += 1
	changed = true


## `attacker` hurt `victim` for `dealt` with `weapon` at `clock` seconds,
## knocking them out or not.
func hit(attacker: int, victim: int, dealt: int, weapon: String, knocked_out: bool, clock: float) -> void:
	if attacker == victim or dealt <= 0:
		return
	var line := _line(attacker)
	line.damage += dealt
	line.hits += 1
	if not _hurt_by.has(victim):
		_hurt_by[victim] = {}
	_hurt_by[victim][attacker] = clock
	if knocked_out:
		line.streak += 1
		line.best = maxi(line.best, line.streak)
		line.with[weapon] = line.with.get(weapon, 0) + 1
		# Everyone else who hurt them lately helped.
		for helper: int in _hurt_by[victim]:
			if helper != attacker and clock - _hurt_by[victim][helper] <= ASSIST_WINDOW:
				_line(helper).assists += 1
	changed = true


## Player `id` was knocked out: their run ends, and nobody is helping to
## knock them out any more.
func knocked_out(id: int) -> void:
	_line(id).streak = 0
	_hurt_by.erase(id)
	changed = true


## Player `id` did `amount` for the mode's objective.
func objective(id: int, amount: float) -> void:
	if amount > 0.0:
		_line(id).objective += amount
		changed = true


## Forgets a player who has left, or everyone, as when a match begins.
func forget(id: int) -> void:
	_lines.erase(id)
	_hurt_by.erase(id)
	changed = true


func clear() -> void:
	_lines.clear()
	_hurt_by.clear()
	changed = true


## The share of what `line` fired that hit, as a whole percentage. A weapon
## whose shot can hit more than once doesn't take it over a hundred.
static func accuracy(line: Dictionary) -> int:
	return roundi(minf(float(line.hits) / line.shots, 1.0) * 100.0) if line.shots > 0 else 0


## The weapon `line` knocked the most out with, or "" if none.
static func favourite(line: Dictionary) -> String:
	var best := ""
	for weapon: String in line.with:
		if best.is_empty() or line.with[weapon] > line.with[best]:
			best = weapon
	return best


## Everything, as it is sent: [[id, damage, assists, objective, shots, hits,
## streak, best, {weapon: knock-outs}], ...].
func to_array() -> Array:
	var sent := []
	for id: int in _lines:
		var line: Dictionary = _lines[id]
		sent.append([id, line.damage, line.assists, roundi(line.objective), line.shots, line.hits, line.streak, line.best, line.with])
	return sent


## The stats in `sent`, as to_array gives them, with anything that isn't
## what it should be left out.
static func from_array(sent: Variant) -> Stats:
	var stats := Stats.new()
	if not sent is Array:
		return stats
	for entry: Variant in (sent as Array).slice(0, MOST):
		if not entry is Array or entry.size() != 9 or not entry[8] is Dictionary:
			continue
		var whole := true
		for index in 8:
			whole = whole and entry[index] is int and (index == 0 or entry[index] >= 0)
		if not whole:
			continue
		var line := blank()
		line.damage = entry[1]
		line.assists = entry[2]
		line.objective = float(entry[3])
		line.shots = entry[4]
		line.hits = entry[5]
		line.streak = entry[6]
		line.best = entry[7]
		for weapon: Variant in entry[8]:
			if weapon is String and entry[8][weapon] is int and (Weapons.find(weapon) != null or weapon == Melee.ID):
				line.with[weapon] = entry[8][weapon]
		stats._lines[entry[0]] = line
	return stats


func _line(id: int) -> Dictionary:
	if not _lines.has(id):
		_lines[id] = blank()
	return _lines[id]
