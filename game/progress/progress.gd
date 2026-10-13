extends Node
## What a player has earned, kept between visits: their record, their
## experience, and their last matches. It lives in the browser's own
## storage (or a file, in a native build), which costs nothing and can't be
## trusted: anyone can change their own. So nothing here decides anything in
## a match; it is for the player, and for a badge by their name.

## Something here changed.
signal changed

const DEFAULT_PATH := "user://progress.json"
const TEST_PATH := "user://progress_under_test.json"
## How many past matches are kept.
const HISTORY := 20
## The most experience kept: what the top level takes, with room to spare.
const MOST_XP := 10_000_000

## Where it is kept. Tests point their own elsewhere.
var path := DEFAULT_PATH
var record := Record.blank()
var xp := 0
## The last matches played, newest first (see MatchReport).
var history: Array = []
## The achievements earned, by id (see Achievements), and what has been
## done toward the challenges that are set (see Challenges).
var achievements: Array = []
var challenges := {"progress": {}, "done": []}


func _init(at := DEFAULT_PATH) -> void:
	path = at
	# The game's own tests play matches to their end: what those earn is
	# kept apart, and begun afresh each run, so that a player's own record
	# on the same machine is never touched by them.
	if at == DEFAULT_PATH and under_test():
		path = TEST_PATH
		DirAccess.remove_absolute(path)
	load_progress()


## Whether the game is being run by its own tests.
static func under_test() -> bool:
	for argument in OS.get_cmdline_args():
		if argument.contains("gut_cmdln"):
			return true
	return false


## Reads what is kept, taking nothing from it that isn't what it should be.
func load_progress() -> void:
	record = Record.blank()
	xp = 0
	history = []
	achievements = []
	challenges = {"progress": {}, "done": []}
	if not FileAccess.file_exists(path):
		return
	# A file that can't be read is a fresh start, said quietly.
	var reader := JSON.new()
	if reader.parse(FileAccess.get_file_as_string(path)) != OK:
		return
	var kept: Variant = reader.data
	if not kept is Dictionary:
		return
	record = Record.tidy(kept.get("record"))
	var stored: Variant = kept.get("xp")
	xp = clampi(int(stored), 0, MOST_XP) if stored is float or stored is int else 0
	achievements = Achievements.tidy(kept.get("achievements"))
	challenges = Challenges.tidy(kept.get("challenges"), now())
	var matches: Variant = kept.get("history")
	if matches is Array:
		for report: Variant in matches.slice(0, HISTORY):
			var tidied := MatchReport.tidy(report)
			if not tidied.is_empty():
				history.append(tidied)


## The player's level.
func level() -> int:
	return Levels.level(xp)


## The challenges done, by id.
func challenges_done() -> Array:
	return challenges.done


## The moment, in seconds since 1970: what day's challenges are set goes
## by it.
func now() -> int:
	return int(Time.get_unix_time_from_system())


## Adds the match in `report` (see MatchReport) to everything kept. Returns
## what it earned: {"xp": experience, "parts": what for, "from": the level
## before, "to": the level after, "challenges": those it finished,
## "achievements": the ids of those it earned, "unlocked": the things it
## gave, each [kind, id] (see Unlocks)}.
func add_match(report: Dictionary) -> Dictionary:
	var tidied := MatchReport.tidy(report)
	if tidied.is_empty():
		return {"xp": 0, "parts": [], "from": level(), "to": level(), "challenges": [], "achievements": [], "unlocked": []}
	var before := level()
	var earned := Levels.earned(tidied)
	record = Record.add(record, tidied)
	# What the match did toward the challenges, and the feats it was.
	var stood := Challenges.advance(challenges, tidied, tidied.when if tidied.when > 0 else now())
	challenges = stood.state
	var feats := Achievements.newly(achievements, record, tidied)
	achievements.append_array(feats)
	var parts: Array = earned.parts
	if stood.xp > 0:
		parts.append(["Challenges", stood.xp])
	xp = mini(xp + earned.total + stood.xp, MOST_XP)
	history.push_front(tidied)
	while history.size() > HISTORY:
		history.pop_back()
	_save()
	changed.emit()
	var done_now: Array = stood.completed.map(func(challenge: Dictionary) -> String: return challenge.id)
	return {"xp": earned.total + stood.xp, "parts": parts, "from": before, "to": level(), "challenges": stood.completed, "achievements": feats,
		"unlocked": Unlocks.gained(before, level(), feats, done_now)}


## Forgets everything.
func reset() -> void:
	record = Record.blank()
	xp = 0
	history = []
	achievements = []
	challenges = {"progress": {}, "done": []}
	_save()
	changed.emit()


func _save() -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file != null:
		file.store_string(JSON.stringify({"record": record, "xp": xp, "history": history, "achievements": achievements, "challenges": challenges}))
