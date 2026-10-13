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
## Alone on the practice range: no match, only targets to shoot at. Not
## among the modes a host chooses from.
const PRACTICE := "practice"
const MODES := [FREE_FOR_ALL, TEAMS, CAPTURE, HILL, POINTS, ROUNDS, GUN_GAME]
const MODE_NAMES := {FREE_FOR_ALL: "Free-for-all", TEAMS: "Cheddar vs Brie", CAPTURE: "Capture the wheel", HILL: "Hold the hill",
	POINTS: "Three points", ROUNDS: "Last mouse standing", GUN_GAME: "Gun game", PRACTICE: "Practice"}
## The modes played in two teams.
const TEAM_MODES := [TEAMS, CAPTURE, HILL, POINTS, ROUNDS]
## The modes won on points rather than knock-outs, and the points that win.
const POINT_LIMITS := {CAPTURE: 3, HILL: 100, POINTS: 150, ROUNDS: 4}
## The host's rules: named sets of changes to how a match plays. Each
## changes some of `health`, `speed`, `gravity`, `respawn`, `friendly_fire`
## and `weapons` below from what they usually are.
const STANDARD := "standard"
const RULES := [STANDARD, "instant", "low_gravity", "launchers", "pistols", "frantic", "tough", "friendly"]
const RULE_NAMES := {STANDARD: "Standard", "instant": "Instant knock-out", "low_gravity": "Low gravity", "launchers": "Launchers only",
	"pistols": "Pistols only", "frantic": "Fast and frantic", "tough": "Tough cheese", "friendly": "Friendly fire"}
const PRESETS := {STANDARD: {}, "instant": {"health": 1}, "low_gravity": {"gravity": 0.4}, "launchers": {"weapons": "launchers"},
	"pistols": {"weapons": "pistols"}, "frantic": {"speed": 1.25, "respawn": 1.0}, "tough": {"health": 200}, "friendly": {"friendly_fire": true}}
## The weapons everyone has, and only has, under rules that say so; with
## "all", players have their loadouts and what they find.
const ALL_WEAPONS := "all"
const WEAPON_SETS := {ALL_WEAPONS: [], "launchers": ["launcher"], "pistols": ["pistol"]}
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
## Which of the RULES the host chose, and what they come to: the health a
## player has, how fast they move and how hard gravity pulls (as shares of
## the usual), seconds a knocked-out player waits, whether teammates can
## hurt each other, and which of the WEAPON_SETS everyone has.
var rules := STANDARD
var health := PlayerRecord.MAX_HEALTH
var speed := 1.0
var gravity := 1.0
var respawn := 3.0
var friendly_fire := false
var weapons := ALL_WEAPONS
## The time of day and the weather (see Atmosphere).
var sky := Atmosphere.NOON
## How many computer players the host wants (see Bots), and how good.
var bots := 0
var bot_skill := BotBrain.NORMAL


func is_teams() -> bool:
	return TEAM_MODES.has(mode)


## The settings of a session on the practice range.
static func practice() -> MatchSettings:
	return from_dict({"mode": PRACTICE, "map": Maps.RANGE, "max_players": MIN_PLAYERS})


func is_practice() -> bool:
	return mode == PRACTICE


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
	# Nor of the host's rules.
	for key: String in RULE_KEYS:
		told.erase(key)
	return told


## The settings described by `data`, as made by to_dict, with anything missing
## or out of range set right.
static func from_dict(data: Variant) -> MatchSettings:
	var settings := MatchSettings.new()
	if not data is Dictionary:
		return settings
	if data.get("mode") in MODES or data.get("mode") == PRACTICE:
		settings.mode = data.mode
	settings.kill_limit = _whole(data.get("kill_limit"), settings.kill_limit, 1, 999)
	settings.minutes = _whole(data.get("minutes"), settings.minutes, 1, 60)
	settings.max_players = _whole(data.get("max_players"), settings.max_players, MIN_PLAYERS, MAX_PLAYERS)
	if Maps.exists(data.get("map")):
		settings.map = data.map
	if data.get("rules") in RULES:
		settings.rules = data.rules
	# The rules chosen, and then anything said besides, kept within reason.
	for given: Dictionary in [PRESETS[settings.rules], data]:
		settings.health = _whole(given.get("health"), settings.health, 1, PlayerRecord.MOST_HEALTH)
		settings.speed = _share(given.get("speed"), settings.speed, 0.5, 2.0)
		settings.gravity = _share(given.get("gravity"), settings.gravity, 0.2, 2.0)
		settings.respawn = _share(given.get("respawn"), settings.respawn, 0.5, 15.0)
		if given.get("friendly_fire") is bool:
			settings.friendly_fire = given.friendly_fire
		if given.get("weapons") is String and WEAPON_SETS.has(given.weapons):
			settings.weapons = given.weapons
	if Atmosphere.is_kind(data.get("sky")):
		settings.sky = data.sky
	settings.bots = _whole(data.get("bots"), 0, 0, MAX_PLAYERS - 1)
	if data.get("bot_skill") in BotBrain.SKILLS:
		settings.bot_skill = data.bot_skill
	return settings


func to_dict() -> Dictionary:
	var data := {"mode": mode, "kill_limit": kill_limit, "minutes": minutes, "max_players": max_players, "map": map, "bots": bots, "bot_skill": bot_skill}
	# The rules only where they aren't the usual, as this is sent often.
	var usual := MatchSettings.new()
	for key: String in RULE_KEYS:
		if get(key) != usual.get(key):
			data[key] = get(key)
	return data


## A short description, such as "Free-for-all · first to 20 · 8 min · Cheddar Canyon".
func describe() -> String:
	var said := "%s · first to %d · %d min · %s" % [MODE_NAMES[mode], limit(), minutes, Maps.NAMES[map]]
	return said if rules == STANDARD else "%s · %s" % [said, RULE_NAMES[rules]]


## Whether everyone has the same weapons and no others.
func fixed_weapons() -> bool:
	return weapons != ALL_WEAPONS


const RULE_KEYS := ["rules", "health", "speed", "gravity", "respawn", "friendly_fire", "weapons", "sky"]


static func _share(value: Variant, default: float, low: float, high: float) -> float:
	if not (value is int or value is float) or not is_finite(float(value)):
		return default
	return clampf(float(value), low, high)


static func _whole(value: Variant, default: int, low: int, high: int) -> int:
	if not (value is int or value is float) or is_nan(float(value)):
		return default
	return clampi(int(value), low, high)
