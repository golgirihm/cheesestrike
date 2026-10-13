class_name MatchReport
## What one player did in one match, as it is kept: for their record, their
## level, their challenges and the list of their past matches.
##
## A report is plain data: {"when": seconds since 1970, "map", "mode",
## "won": true, false or null for a draw, "place", "players", "kills",
## "deaths", "damage", "assists", "shots", "hits", "best_run", "seconds":
## how long they played, "medals": {medal: how many}, "with": {weapon:
## knock-outs}, "held": {weapon: seconds in hand}}.

## The longest a match is taken to have lasted, in seconds: longer is a
## clock gone wrong.
const LONGEST := 3600.0
const _WHOLE := ["when", "place", "players", "kills", "deaths", "damage", "assists", "shots", "hits", "best_run"]


## A report with nothing in it.
static func blank() -> Dictionary:
	return {"when": 0, "map": Maps.DEFAULT, "mode": MatchSettings.FREE_FOR_ALL, "won": null, "place": 0, "players": 0, "kills": 0, "deaths": 0,
		"damage": 0, "assists": 0, "shots": 0, "hits": 0, "best_run": 0, "seconds": 0.0, "medals": {}, "with": {}, "held": {}}


## The report of the match in `state` for player `own`, whose stats (see
## Stats) are `line`, who earned `medals` ({medal: how many}), had each
## weapon in hand for `held` ({weapon: seconds}), and played for `seconds`,
## ending at `when`.
static func build(state: MatchState, own: int, line: Dictionary, medals: Dictionary, held: Dictionary, seconds: float, when: int) -> Dictionary:
	var record := state.player(own)
	var report := blank()
	report.when = when
	report.map = state.settings.map
	report.mode = state.settings.mode
	report.players = state.playing_count()
	report.seconds = seconds
	report.medals = medals.duplicate()
	report.held = held.duplicate()
	if record == null:
		return tidy(report)
	report.kills = record.kills
	report.deaths = record.deaths
	var side: int = record.team if state.settings.is_teams() else own
	if not state.winners.is_empty():
		report.won = state.winners == [side] if state.winners.size() == 1 else null
	var podium := MatchSummary.podium(state)
	for index in podium.size():
		if podium[index].id == own:
			report.place = index + 1
	for key: String in ["damage", "assists", "shots", "hits"]:
		report[key] = line.get(key, 0)
	report.best_run = line.get("best", 0)
	report.with = (line.get("with", {}) as Dictionary).duplicate()
	return tidy(report)


## Whether a match in `state` is one that counts: a real match, not the
## practice range, and one in which this player played rather than watched.
static func counts(state: MatchState, own: int) -> bool:
	var record := state.player(own)
	return record != null and not record.watching and not state.settings.is_practice() and state.playing_count() >= 2


## `report` with everything in it made what a report should hold: numbers
## whole and not below nothing, an arena and a mode there are, and only
## medals and weapons there are. {} if it isn't a report at all.
static func tidy(report: Variant) -> Dictionary:
	if not report is Dictionary:
		return {}
	var tidied := blank()
	for key: String in _WHOLE:
		var value: Variant = report.get(key)
		if value is int or value is float:
			tidied[key] = maxi(int(value), 0)
	if Maps.exists(report.get("map")):
		tidied.map = report.map
	if MatchSettings.MODES.has(report.get("mode")):
		tidied.mode = report.mode
	tidied.won = report.get("won") if report.get("won") is bool else null
	var seconds: Variant = report.get("seconds")
	tidied.seconds = clampf(float(seconds), 0.0, LONGEST) if seconds is float or seconds is int else 0.0
	tidied.hits = mini(tidied.hits, 100000)
	for medal: Variant in report.get("medals", {}) if report.get("medals") is Dictionary else {}:
		var count: Variant = report.medals[medal]
		if Medals.ALL.has(medal) and (count is int or count is float) and count > 0:
			tidied.medals[medal] = int(count)
	for key: String in ["with", "held"]:
		var said: Variant = report.get(key)
		if not said is Dictionary:
			continue
		for weapon: Variant in said:
			var amount: Variant = said[weapon]
			if weapon is String and (Weapons.find(weapon) != null or weapon == Melee.ID) and (amount is int or amount is float) and amount > 0:
				tidied[key][weapon] = int(amount) if key == "with" else minf(float(amount), LONGEST)
	return tidied


## How a report reads in a line, as in the list of past matches: "Won ·
## Curd City · Cheddar vs Brie · 12 knock-outs, 4 times out".
static func line(report: Dictionary) -> String:
	var result := "Drew" if report.won == null else ("Won" if report.won else "Lost")
	return "%s  ·  %s  ·  %s  ·  %d knock-outs, %d times out" % [result, Maps.NAMES[report.map], MatchSettings.MODE_NAMES[report.mode], report.kills, report.deaths]
