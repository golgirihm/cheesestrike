class_name Achievements
## Achievements: one-off feats, each with a picture and a line. Earned once
## and kept (see Progress).
##
## Each is decided from the player's record as it stands after a match and
## from that match's own report, so that whether one is earned is a plain
## question of numbers.

## Every achievement: what it is called, the line that says how it was
## earned, and its picture (see Icons).
const ALL := {
	"first_blood": {"name": "First Crumb", "line": "Knock someone out.", "icon": "medal_head"},
	"first_win": {"name": "Top of the Board", "line": "Win a match.", "icon": "credits"},
	"century": {"name": "Mouser", "line": "Knock out a hundred players.", "icon": "medal_streak"},
	"veteran": {"name": "Well Aged", "line": "Play fifty matches.", "icon": "learn"},
	"flawless": {"name": "Untouched Rind", "line": "Win a match with five knock-outs and without being knocked out.", "icon": "armour"},
	"hat_trick": {"name": "Three in a Row", "line": "Knock out three players one after another.", "icon": "medal_triple"},
	"marksman": {"name": "Sharpshooter", "line": "Finish a match having hit with six shots in ten, of twenty or more.", "icon": "medal_long"},
	"long_arm": {"name": "From the Next County", "line": "Knock someone out from very far away.", "icon": "rifle"},
	"comeback_kid": {"name": "Back from the Board", "line": "Knock someone out after being knocked out three times running.", "icon": "medal_comeback"},
	"grudge": {"name": "No Hard Feelings", "line": "Take revenge on whoever last knocked you out.", "icon": "medal_revenge"},
	"globetrotter": {"name": "Tour of the Dairy", "line": "Play a match on every arena.", "icon": "ping"},
	"all_rounder": {"name": "Every Flavour", "line": "Play a match of every mode.", "icon": "practice"},
	"arsenal": {"name": "The Whole Board", "line": "Knock someone out with every weapon.", "icon": "ammo"},
	"marathon": {"name": "Long Maturing", "line": "Play for ten hours in all.", "icon": "health"},
}
const MARKSMAN_SHARE := 0.6
const MARKSMAN_SHOTS := 20
const MARATHON_SECONDS := 36000.0


## What the achievement with `id` is called, or "" if there is none.
static func name_of(id: String) -> String:
	return ALL[id].name if ALL.has(id) else ""


## Whether the achievement with `id` is earned by a player whose record
## (see Record) is `record` after the match in `report` (see MatchReport).
static func earned(id: String, record: Dictionary, report: Dictionary) -> bool:
	match id:
		"first_blood":
			return record.kills >= 1
		"first_win":
			return record.wins >= 1
		"century":
			return record.kills >= 100
		"veteran":
			return record.matches >= 50
		"flawless":
			return report.won == true and report.deaths == 0 and report.kills >= 5
		"hat_trick":
			return report.medals.get(Medals.TRIPLE, 0) > 0
		"marksman":
			return report.shots >= MARKSMAN_SHOTS and float(report.hits) / report.shots >= MARKSMAN_SHARE
		"long_arm":
			return report.medals.get(Medals.LONG_SHOT, 0) > 0
		"comeback_kid":
			return report.medals.get(Medals.COMEBACK, 0) > 0
		"grudge":
			return report.medals.get(Medals.REVENGE, 0) > 0
		"globetrotter":
			return Maps.ORDER.all(func(map: String) -> bool: return record.arenas.get(map, 0) > 0)
		"all_rounder":
			return MatchSettings.MODES.all(func(mode: String) -> bool: return mode == MatchSettings.PRACTICE or record.modes.get(mode, 0) > 0)
		"arsenal":
			return Weapons.ids().all(func(weapon: String) -> bool: return record.weapons.has(weapon) and record.weapons[weapon].kills > 0)
		"marathon":
			return record.seconds >= MARATHON_SECONDS
	return false


## The achievements newly earned after the match in `report`, by a player
## whose record is now `record` and who already `had` some.
static func newly(had: Array, record: Dictionary, report: Dictionary) -> Array[String]:
	var new: Array[String] = []
	for id: String in ALL:
		if not had.has(id) and earned(id, record, report):
			new.append(id)
	return new


## `had` with nothing in it that isn't an achievement, and each once.
static func tidy(had: Variant) -> Array:
	var kept := []
	if had is Array:
		for id: Variant in had:
			if id is String and ALL.has(id) and not kept.has(id):
				kept.append(id)
	return kept


## Every achievement as it reads on its page: earned or not, its name and
## its line.
static func lines(had: Array) -> Array[String]:
	var said: Array[String] = ["%d of %d earned" % [tidy(had).size(), ALL.size()]]
	for id: String in ALL:
		said.append("%s %s   ·   %s" % ["[x]" if had.has(id) else "[ ]", ALL[id].name, ALL[id].line])
	return said
