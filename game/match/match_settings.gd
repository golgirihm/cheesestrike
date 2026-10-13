class_name MatchSettings
extends RefCounted
## How the host has set up a session's matches. Chosen on the menu before
## hosting, shown in the session list, and sent to everyone with the match
## state. Anything out of range is brought back into it, as settings arrive from
## other machines.

const FREE_FOR_ALL := "ffa"
const TEAMS := "teams"
## Two bases, two wheels of cheese to carry off (see CaptureRules).
const CAPTURE := "ctf"
## One zone that moves; and three that are kept (see ZoneRules).
const HILL := "hill"
const POINTS := "points"
## Rounds with one life each.
const ROUNDS := "rounds"
## Every knock-out moves a player to the next weapon (see GunGame).
const GUN_GAME := "gun"
const MODES := [FREE_FOR_ALL, TEAMS, CAPTURE, HILL, POINTS, ROUNDS, GUN_GAME]
const MODE_NAMES := {FREE_FOR_ALL: "Free-for-all", TEAMS: "Cheddar vs Brie", CAPTURE: "Capture the wheel", HILL: "Hold the hill",
	POINTS: "Three points", ROUNDS: "Last mouse standing", GUN_GAME: "Gun game"}
## The modes played in two teams.
const TEAM_MODES := [TEAMS, CAPTURE, HILL, POINTS, ROUNDS]
## The modes won on points rather than knock-outs, and the points that win.
const POINT_LIMITS := {CAPTURE: 3, HILL: 100, POINTS: 150, ROUNDS: 4}
const KILL_LIMITS := [5, 10, 20, 30, 50]
const MINUTES := [3, 5, 8, 10, 15]
const MIN_PLAYERS := 2
const MAX_PLAYERS := 16

var mode := FREE_FOR_ALL
## Knock-outs that win: by one player, or by one team.
var kill_limit := 20
var minutes := 8
var max_players := MAX_PLAYERS
## Which of the Maps to play on.
var map := Maps.DEFAULT
## How many computer players the host wants (see Bots), and how good.
var bots := 0
var bot_skill := BotBrain.NORMAL


func is_teams() -> bool:
	return TEAM_MODES.has(mode)


## Whether teams score points for what they do, rather than knock-outs.
func by_points() -> bool:
	return POINT_LIMITS.has(mode)


## The score that wins: points, knock-outs, or every weapon of the gun game.
func limit() -> int:
	if mode == GUN_GAME:
		return GunGame.LADDER.size()
	return POINT_LIMITS.get(mode, kill_limit)


## The settings as the session list is told them. The server that keeps the
## list knows only the first two modes, so the others are listed as
## whichever of those is nearest: teams or not.
func advertised() -> Dictionary:
	var told := to_dict()
	told.mode = TEAMS if is_teams() else FREE_FOR_ALL
	# Nor does it know of bots.
	told.erase("bots")
	told.erase("bot_skill")
	return told


## The settings described by `data`, as made by to_dict, with anything missing
## or out of range set right.
static func from_dict(data: Variant) -> MatchSettings:
	var settings := MatchSettings.new()
	if not data is Dictionary:
		return settings
	if data.get("mode") in MODES:
		settings.mode = data.mode
	settings.kill_limit = _whole(data.get("kill_limit"), settings.kill_limit, 1, 999)
	settings.minutes = _whole(data.get("minutes"), settings.minutes, 1, 60)
	settings.max_players = _whole(data.get("max_players"), settings.max_players, MIN_PLAYERS, MAX_PLAYERS)
	if Maps.exists(data.get("map")):
		settings.map = data.map
	settings.bots = _whole(data.get("bots"), 0, 0, MAX_PLAYERS - 1)
	if data.get("bot_skill") in BotBrain.SKILLS:
		settings.bot_skill = data.bot_skill
	return settings


func to_dict() -> Dictionary:
	return {"mode": mode, "kill_limit": kill_limit, "minutes": minutes, "max_players": max_players, "map": map, "bots": bots, "bot_skill": bot_skill}


## A short description, such as "Free-for-all · first to 20 · 8 min · Cheddar Canyon".
func describe() -> String:
	return "%s · first to %d · %d min · %s" % [MODE_NAMES[mode], limit(), minutes, Maps.NAMES[map]]


static func _whole(value: Variant, default: int, low: int, high: int) -> int:
	if not (value is int or value is float) or is_nan(float(value)):
		return default
	return clampi(int(value), low, high)
