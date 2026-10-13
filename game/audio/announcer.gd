class_name Announcer
extends Node
## The announcer: a voice that tells a player how the match stands for them.
## It speaks at the start and end of a match, when the lead changes hands, a
## minute from time, at sudden death, when they knock several out one after
## another, and when a power-up is taken.
##
## What is said is decided here from how the match stands, the same on every
## machine but from each player's own side: one hears "Victory" where
## another hears "Defeat". It is spoken by the device's own speech, where it
## has one, and shown on the screen either way.

## A line is said; `show` is whether it should be put on the screen too, or
## the match already does that itself.
signal said(line: String, show: bool)

const BEGINS := "The match begins"
const VICTORY := "Victory"
const DEFEAT := "Defeat"
const DRAW := "A draw"
const LEAD_TAKEN := "You have taken the lead"
const LEAD_LOST := "You have lost the lead"
const TEAM_LEAD_TAKEN := "Your team has taken the lead"
const TEAM_LEAD_LOST := "Your team has lost the lead"
const MINUTE := "One minute left"
const SUDDEN_DEATH := "Sudden death"
## What is said for knocking out this many, one soon after another.
const STREAKS := {2: "Double knock-out", 3: "Triple knock-out", 4: "Rampage"}
## Seconds after a knock-out in which the next counts as one after another.
const STREAK_WINDOW := 4.5
## The lines the match already puts on the screen itself, as it does when a
## power-up is taken.
const SHOWN_ALREADY := [BEGINS, SUDDEN_DEATH]
## The setting that turns the voice off.
const SETTING := "announcer"

var world: Node3D
## Every line said, oldest first, for tests.
var spoken: Array[String] = []
# How the match stood when it was last looked at.
var _was := {}
# How many the player has knocked out one after another, and when the last
# was.
var _streak := 0
var _last_knock_out := -INF
var _clock := 0.0


func _init(of: Node3D = null) -> void:
	world = of
	name = "Announcer"


func _ready() -> void:
	if world == null:
		return
	world.match_.hit_confirmed.connect(func(_victim: int, _dealt: int, knocked_out: bool, _head: bool) -> void:
		if knocked_out:
			_say(knock_out(_clock)))
	world.match_.power_taken.connect(func(id: int, power: int) -> void:
		_say(power_line(power, multiplayer.multiplayer_peer != null and id == multiplayer.get_unique_id()), false))


func _process(delta: float) -> void:
	_clock += delta
	if world == null or not world.match_.has_state() or multiplayer.multiplayer_peer == null:
		_was = {}
		return
	for line in update(standing(world.match_.state, multiplayer.get_unique_id())):
		_say(line)


## How the match in `state` stands for player `own`: {"playing": the match
## proper is under way, "ended", "won": true, false, or null for a draw or
## for someone only watching, "leading": true, false, or null with nothing
## in it, "teams", "time_left", "sudden": in sudden death}.
static func standing(state: MatchState, own: int) -> Dictionary:
	var record := state.player(own)
	var teams := state.settings.is_teams()
	var side: int = record.team if record != null and teams else own
	var playing := record != null and not record.watching
	var leaders := state._leaders()
	var winners: Array = state.winners
	return {
		"playing": state.phase == MatchState.PLAYING and not state.is_warm_up(),
		"ended": state.phase == MatchState.ENDED,
		"won": (winners == [side] if winners.size() == 1 else null) if playing and not winners.is_empty() else null,
		"leading": (leaders == [side] if leaders.size() == 1 else null) if playing else null,
		"teams": teams,
		"time_left": state.time_left,
		"sudden": state.overtime,
	}


## Told how the match stands now (see standing); returns what there is to
## say about what has changed since it was last told.
func update(now: Dictionary) -> Array[String]:
	var lines: Array[String] = []
	var was := _was
	_was = now
	if was.is_empty():
		# Someone who has only just arrived is told nothing of what went before.
		return lines
	if now.playing and not was.playing:
		lines.append(BEGINS)
	if now.ended and not was.ended:
		lines.append(DRAW if now.won == null else (VICTORY if now.won else DEFEAT))
		return lines
	if not now.playing:
		return lines
	if now.sudden and not was.sudden:
		lines.append(SUDDEN_DEATH)
	elif was.playing and was.time_left > 60.0 and now.time_left <= 60.0 and not now.sudden:
		lines.append(MINUTE)
	if now.leading == true and was.leading != true:
		lines.append(TEAM_LEAD_TAKEN if now.teams else LEAD_TAKEN)
	elif was.leading == true and now.leading != true:
		lines.append(TEAM_LEAD_LOST if now.teams else LEAD_LOST)
	return lines


## The player knocked someone out, `clock` seconds in: what is said for it,
## or "" for the first of a run.
func knock_out(clock: float) -> String:
	_streak = _streak + 1 if clock - _last_knock_out <= STREAK_WINDOW else 1
	_last_knock_out = clock
	return STREAKS.get(mini(_streak, STREAKS.keys().max()), "")


## What is said when the power-up `power` is taken, by the player
## themselves (`mine`) or by someone else.
static func power_line(power: int, mine: bool) -> String:
	if not Powers.is_kind(power):
		return ""
	return "%s!" % Powers.NAMES[power] if mine else "%s is taken" % Powers.NAMES[power]


## Whether the voice is wanted and the device has one.
static func can_speak() -> bool:
	return Settings.get_value(SETTING) and DisplayServer.has_feature(DisplayServer.FEATURE_TEXT_TO_SPEECH) and not DisplayServer.tts_get_voices().is_empty()


## How loud the voice is, from 0 to 100, by the master and the interface
## volumes.
static func voice_volume() -> int:
	return roundi(clampf(Settings.get_value("master_volume") * Settings.get_value("interface_volume"), 0.0, 1.0) * 100.0)


func _say(line: String, show := true) -> void:
	if line.is_empty():
		return
	spoken.append(line)
	if spoken.size() > 40:
		spoken.pop_front()
	said.emit(line, show and not SHOWN_ALREADY.has(line))
	if can_speak() and voice_volume() > 0:
		# What is being said gives way to what there is to say now.
		DisplayServer.tts_stop()
		DisplayServer.tts_speak(line, DisplayServer.tts_get_voices()[0].id, voice_volume(), 0.9, 1.1)
