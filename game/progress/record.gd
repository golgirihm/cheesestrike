class_name Record
## A player's record: everything they have done in every match, added up.
## Matches played and won, knock-outs, what they fired and how much of it
## hit, damage, time played, their best run, and for each weapon the
## knock-outs with it and the time it was in hand.
##
## A record is plain data, kept in the player's own browser, where nothing
## protects it: it is for the player's own eyes, and nothing depends on it
## being true.

const _WHOLE := ["matches", "wins", "draws", "kills", "deaths", "damage", "assists", "shots", "hits", "best_run", "medals"]


## A record with nothing in it.
static func blank() -> Dictionary:
	return {"matches": 0, "wins": 0, "draws": 0, "kills": 0, "deaths": 0, "damage": 0, "assists": 0, "shots": 0, "hits": 0, "best_run": 0,
		"medals": 0, "seconds": 0.0, "weapons": {}, "arenas": {}, "modes": {}}


## `record` with the match in `report` (see MatchReport) added to it.
static func add(record: Dictionary, report: Dictionary) -> Dictionary:
	var added := tidy(record)
	added.matches += 1
	if report.won == true:
		added.wins += 1
	elif report.won == null:
		added.draws += 1
	for key: String in ["kills", "deaths", "damage", "assists", "shots", "hits"]:
		added[key] += report[key]
	added.best_run = maxi(added.best_run, report.best_run)
	added.seconds += report.seconds
	for medal: String in report.medals:
		added.medals += report.medals[medal]
	for weapon: String in report.with:
		_weapon(added, weapon).kills += report.with[weapon]
	for weapon: String in report.held:
		_weapon(added, weapon).seconds += report.held[weapon]
	added.arenas[report.map] = added.arenas.get(report.map, 0) + 1
	added.modes[report.mode] = added.modes.get(report.mode, 0) + 1
	return added


## The share of what was fired that hit, as a whole percentage.
static func accuracy(record: Dictionary) -> int:
	return roundi(minf(float(record.hits) / record.shots, 1.0) * 100.0) if record.shots > 0 else 0


## Knock-outs for each time knocked out, to one decimal place: all of them
## for someone never knocked out.
static func ratio(record: Dictionary) -> float:
	return snappedf(float(record.kills) / maxi(record.deaths, 1), 0.1)


## The weapon most knocked out with, or "" if none.
static func favourite(record: Dictionary) -> String:
	var best := ""
	for weapon: String in record.weapons:
		if record.weapons[weapon].kills > 0 and (best.is_empty() or record.weapons[weapon].kills > record.weapons[best].kills):
			best = weapon
	return best


## How a length of time reads: "3 h 12 min", "12 min", or "under a minute".
static func time_text(seconds: float) -> String:
	var minutes := int(seconds / 60.0)
	if minutes < 1:
		return "under a minute"
	return "%d h %d min" % [minutes / 60, minutes % 60] if minutes >= 60 else "%d min" % minutes


## The record as it reads on its page, a line at a time.
static func lines(record: Dictionary) -> Array[String]:
	var said: Array[String] = [
		"Matches played: %d   ·   won %d   ·   drawn %d" % [record.matches, record.wins, record.draws],
		"Knock-outs: %d   ·   times out: %d   ·   %.1f for each" % [record.kills, record.deaths, ratio(record)],
		"Accuracy: %d%%   ·   damage dealt: %d   ·   assists: %d" % [accuracy(record), record.damage, record.assists],
		"Best run: %d   ·   medals: %d   ·   time played: %s" % [record.best_run, record.medals, time_text(record.seconds)],
	]
	# Each weapon used, most knock-outs first.
	var weapons: Array = record.weapons.keys()
	weapons.sort_custom(func(a: String, b: String) -> bool:
		if record.weapons[a].kills != record.weapons[b].kills:
			return record.weapons[a].kills > record.weapons[b].kills
		return a < b)
	for weapon: String in weapons:
		var stats := Weapons.find(weapon)
		said.append("%s: %d knock-outs   ·   %s in hand" % [stats.display_name if stats != null else Melee.NAME, record.weapons[weapon].kills, time_text(record.weapons[weapon].seconds)])
	return said


## `record` with everything in it made what a record should hold. A blank
## one if it isn't a record at all.
static func tidy(record: Variant) -> Dictionary:
	var tidied := blank()
	if not record is Dictionary:
		return tidied
	for key: String in _WHOLE:
		var value: Variant = record.get(key)
		if value is int or value is float:
			tidied[key] = maxi(int(value), 0)
	var seconds: Variant = record.get("seconds")
	if seconds is float or seconds is int:
		tidied.seconds = maxf(float(seconds), 0.0)
	var weapons: Variant = record.get("weapons")
	if weapons is Dictionary:
		for weapon: Variant in weapons:
			var said: Variant = weapons[weapon]
			if weapon is String and (Weapons.find(weapon) != null or weapon == Melee.ID) and said is Dictionary:
				var kills: Variant = said.get("kills", 0)
				var held: Variant = said.get("seconds", 0.0)
				tidied.weapons[weapon] = {"kills": maxi(int(kills), 0) if kills is int or kills is float else 0, "seconds": maxf(float(held), 0.0) if held is float or held is int else 0.0}
	for key: String in ["arenas", "modes"]:
		var counted: Variant = record.get(key)
		if counted is Dictionary:
			for name: Variant in counted:
				var known: bool = name is String and (Maps.exists(name) if key == "arenas" else MatchSettings.MODES.has(name))
				if known and (counted[name] is int or counted[name] is float):
					tidied[key][name] = maxi(int(counted[name]), 0)
	return tidied


static func _weapon(record: Dictionary, weapon: String) -> Dictionary:
	if not record.weapons.has(weapon):
		record.weapons[weapon] = {"kills": 0, "seconds": 0.0}
	return record.weapons[weapon]
