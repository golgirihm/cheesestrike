class_name MatchKeeper
extends Node
## Keeps count of this machine's player through a match, and when it ends
## hands what they did to what is kept between visits (see Progress): once,
## and only for a match that counts.

## A match that counted has been added to the player's record: what it
## earned (see Progress.add_match).
signal kept(earned: Dictionary)

var world: Node3D
## What the last match earned, or {} if it didn't count or isn't over.
var last := {}
# How long this player has been in the match, and how long each weapon has
# been in their hands.
var _seconds := 0.0
var _held := {}
var _was_over := false


func _init(of: Node3D = null) -> void:
	world = of
	name = "MatchKeeper"


## What the end of the match adds to what it says, for the last match: the
## experience earned and the level it brought.
func lines() -> Array[String]:
	if last.is_empty():
		return []
	var said: Array[String] = ["+%d experience   ·   Level %d" % [last.xp, last.to]]
	if last.to > last.from:
		said[0] = "+%d experience   ·   Level %d reached!" % [last.xp, last.to]
	for challenge: Dictionary in last.get("challenges", []):
		said.append("Challenge done: %s" % challenge.text)
	for feat: String in last.get("achievements", []):
		said.append("Achievement: %s" % Achievements.name_of(feat))
	for thing: Array in last.get("unlocked", []):
		said.append("Unlocked   ·   %s" % Unlocks.gained_text(thing))
	return said


func _process(delta: float) -> void:
	var state: MatchState = world.match_.state if world != null and world.match_.has_state() and multiplayer.multiplayer_peer != null else null
	if state == null:
		_begin()
		_was_over = false
		return
	var own := multiplayer.get_unique_id()
	var over := state.phase == MatchState.ENDED
	if over and not _was_over:
		_finish(state, own)
	elif not over and _was_over:
		# The next match.
		_begin()
	_was_over = over
	if not state.is_live():
		return
	var record := state.player(own)
	if record == null or record.watching:
		return
	_seconds += delta
	if record.alive:
		_held[record.weapon] = _held.get(record.weapon, 0.0) + delta


func _begin() -> void:
	_seconds = 0.0
	_held = {}
	last = {}


# The match is over: if it counted, it is added to what is kept.
func _finish(state: MatchState, own: int) -> void:
	if not MatchReport.counts(state, own):
		return
	var hud: Node = get_tree().get_first_node_in_group("combat_hud")
	var medals: Dictionary = hud.medals.earned if hud != null else {}
	var report := MatchReport.build(state, own, world.match_.stats.of(own), medals, _held, _seconds, int(Time.get_unix_time_from_system()))
	last = Progress.add_match(report)
	kept.emit(last)
	# Their level is by their name, for everyone to see.
	if last.to != last.from:
		world.match_.wear(Looks.chosen())
