class_name Challenges
## Challenges: a few each day and each week, and a long one for each weapon.
## Knock-outs with the shotgun, a win on Curd City, three medals.
##
## Which are set on a day is worked out from the day itself, so everyone has
## the same ones and nothing has to be sent. What a player has done toward
## them is kept with the rest of what they have earned (see Progress).

const DAILY := "day"
const WEEKLY := "week"
const LONG := "long"
## How many are set each day and each week.
const EACH_DAY := 3
const EACH_WEEK := 2
## The experience each is worth.
const REWARDS := {DAILY: 150, WEEKLY: 500, LONG: 1000}
## A week's challenge asks this many times what a day's does.
const WEEK_TIMES := 5
## The knock-outs a weapon's long challenge asks for.
const LONG_KILLS := 100
const DAY_SECONDS := 86400
## The kinds of thing a challenge can ask for, with how each reads; %d is
## how many and %s what with, or where.
const KINDS := {
	"kills": "Knock out %d players",
	"kills_with": "Knock out %d players with the %s",
	"wins_on": "Win %d matches on %s",
	"play_mode": "Play %d matches of %s",
	"medals": "Earn %d medals",
	"damage": "Deal %d damage",
	"assists": "Help with %d knock-outs",
}
## How many of each kind a day's challenge asks for.
const DAY_GOALS := {"kills": 15, "kills_with": 6, "wins_on": 1, "play_mode": 2, "medals": 3, "damage": 1500, "assists": 4}


## The day `unix` (seconds since 1970) falls on, counted from 1970, and the
## week.
static func day_of(unix: int) -> int:
	return unix / DAY_SECONDS


static func week_of(unix: int) -> int:
	return day_of(unix) / 7


## Every challenge a day or a week could set: {"kind", "what"}.
static func pool() -> Array:
	var all := []
	for weapon: String in Weapons.ids():
		all.append({"kind": "kills_with", "what": weapon})
	for map: String in Maps.ORDER:
		all.append({"kind": "wins_on", "what": map})
	for mode: String in MatchSettings.MODES:
		if mode != MatchSettings.PRACTICE:
			all.append({"kind": "play_mode", "what": mode})
	for kind: String in ["kills", "medals", "damage", "assists"]:
		all.append({"kind": kind, "what": ""})
	return all


## The challenges set at `unix`: the day's, the week's, and the long ones.
## Each is {"id", "period", "kind", "what", "goal", "reward", "text"}.
static func active(unix: int) -> Array[Dictionary]:
	var set: Array[Dictionary] = []
	for chosen: Dictionary in _draw(day_of(unix) * 2 + 1, EACH_DAY):
		set.append(_made(DAILY, day_of(unix), chosen, DAY_GOALS[chosen.kind]))
	for chosen: Dictionary in _draw(week_of(unix) * 2, EACH_WEEK):
		set.append(_made(WEEKLY, week_of(unix), chosen, DAY_GOALS[chosen.kind] * WEEK_TIMES))
	for weapon: String in Weapons.ids():
		set.append(_made(LONG, 0, {"kind": "kills_with", "what": weapon}, LONG_KILLS))
	return set


## How much the match in `report` (see MatchReport) does toward a challenge
## of `kind` asking for `what`.
static func counted(kind: String, what: String, report: Dictionary) -> int:
	match kind:
		"kills":
			return report.kills
		"kills_with":
			return report.with.get(what, 0)
		"wins_on":
			return 1 if report.won == true and report.map == what else 0
		"play_mode":
			return 1 if report.mode == what else 0
		"medals":
			var total := 0
			for medal: String in report.medals:
				total += report.medals[medal]
			return total
		"damage":
			return report.damage
		"assists":
			return report.assists
	return 0


## What has been done toward the challenges, kept as {"progress": {id: how
## much}, "done": [ids]}, after the match in `report`, which ended at
## `unix`. Returns {"state": the new, "completed": the challenges it
## finished, "xp": what they are worth together}.
static func advance(state: Dictionary, report: Dictionary, unix: int) -> Dictionary:
	var kept := tidy(state, unix)
	var completed: Array[Dictionary] = []
	var xp := 0
	for challenge: Dictionary in active(unix):
		if kept.done.has(challenge.id):
			continue
		var so_far: int = kept.progress.get(challenge.id, 0) + counted(challenge.kind, challenge.what, report)
		if so_far >= challenge.goal:
			kept.done.append(challenge.id)
			kept.progress.erase(challenge.id)
			completed.append(challenge)
			xp += challenge.reward
		elif so_far > 0:
			kept.progress[challenge.id] = so_far
	return {"state": kept, "completed": completed, "xp": xp}


## `state` with nothing in it but what belongs: what is kept of challenges
## that are still set at `unix`, or are long ones.
static func tidy(state: Variant, unix: int) -> Dictionary:
	var kept := {"progress": {}, "done": []}
	if not state is Dictionary:
		return kept
	var ids := {}
	for challenge: Dictionary in active(unix):
		ids[challenge.id] = challenge.goal
	var progress: Variant = state.get("progress")
	if progress is Dictionary:
		for id: Variant in progress:
			if ids.has(id) and (progress[id] is int or progress[id] is float) and progress[id] > 0:
				kept.progress[id] = mini(int(progress[id]), ids[id])
	var done: Variant = state.get("done")
	if done is Array:
		for id: Variant in done:
			# A long one stays done; a day's or a week's is forgotten with it.
			if id is String and (ids.has(id) or id.begins_with(LONG + ":")) and not kept.done.has(id):
				kept.done.append(id)
	return kept


## How each challenge set at `unix` stands in `state`, as it reads: "Knock
## out 6 players with the Swiss Shotgun   ·   2 of 6   ·   150 experience".
static func lines(state: Dictionary, unix: int) -> Array[String]:
	var kept := tidy(state, unix)
	var said: Array[String] = []
	var headings := {DAILY: "Today", WEEKLY: "This week", LONG: "For each weapon"}
	var last := ""
	for challenge: Dictionary in active(unix):
		if challenge.period != last:
			last = challenge.period
			said.append("%s:" % headings[last])
		var stands := "done" if kept.done.has(challenge.id) else "%d of %d" % [kept.progress.get(challenge.id, 0), challenge.goal]
		said.append("   %s   ·   %s   ·   %d experience" % [challenge.text, stands, challenge.reward])
	return said


## What the challenge with `id` asks, as it reads, or "" if it isn't one.
static func text_of(id: String) -> String:
	var parts := id.split(":")
	if parts.size() < 3 or not KINDS.has(parts[-2]):
		return ""
	var period := parts[0]
	var goal: int = LONG_KILLS if period == LONG else DAY_GOALS[parts[-2]] * (WEEK_TIMES if period == WEEKLY else 1)
	return _text(parts[-2], parts[-1], goal)


static func _made(period: String, number: int, chosen: Dictionary, goal: int) -> Dictionary:
	var id := "%s:%s:%s" % [period, chosen.kind, chosen.what] if period == LONG else "%s:%d:%s:%s" % [period, number, chosen.kind, chosen.what]
	return {"id": id, "period": period, "kind": chosen.kind, "what": chosen.what, "goal": goal, "reward": REWARDS[period], "text": _text(chosen.kind, chosen.what, goal)}


static func _text(kind: String, what: String, goal: int) -> String:
	var named := what
	if Weapons.find(what) != null:
		named = Weapons.find(what).display_name
	elif Maps.NAMES.has(what):
		named = Maps.NAMES[what]
	elif MatchSettings.MODE_NAMES.has(what):
		named = MatchSettings.MODE_NAMES[what]
	var said: String = KINDS[kind]
	return said % ([goal, named] if said.contains("%s") else [goal])


# `count` different challenges from the pool, chosen by `seed`: the same
# for everyone on the same day.
static func _draw(seed: int, count: int) -> Array:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var all := pool()
	var drawn := []
	var kinds := {}
	while drawn.size() < count and not all.is_empty():
		var chosen: Dictionary = all.pop_at(rng.randi() % all.size())
		# No two of a kind on the same day, so that a day has variety.
		if not kinds.has(chosen.kind):
			kinds[chosen.kind] = true
			drawn.append(chosen)
	return drawn
