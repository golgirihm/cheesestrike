class_name MatchSettings
extends RefCounted
## How the host has set up a session's matches. Chosen on the menu before
## hosting, shown in the session list, and sent to everyone with the match
## state. Anything out of range is brought back into it, as settings arrive from
## other machines.

const FREE_FOR_ALL := "ffa"
const TEAMS := "teams"
const MODES := [FREE_FOR_ALL, TEAMS]
const MODE_NAMES := {FREE_FOR_ALL: "Free-for-all", TEAMS: "Cheddar vs Brie"}
const KILL_LIMITS := [5, 10, 20, 30, 50]
const MINUTES := [3, 5, 8, 10, 15]
const MIN_PLAYERS := 2
const MAX_PLAYERS := 16

var mode := FREE_FOR_ALL
## Knock-outs that win: by one player, or by one team.
var kill_limit := 20
var minutes := 8
var max_players := MAX_PLAYERS


func is_teams() -> bool:
	return mode == TEAMS


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
	return settings


func to_dict() -> Dictionary:
	return {"mode": mode, "kill_limit": kill_limit, "minutes": minutes, "max_players": max_players}


## A short description for the session list, such as "Free-for-all · first to 20".
func describe() -> String:
	return "%s · first to %d · %d min" % [MODE_NAMES[mode], kill_limit, minutes]


static func _whole(value: Variant, default: int, low: int, high: int) -> int:
	if not (value is int or value is float) or is_nan(float(value)):
		return default
	return clampi(int(value), low, high)
