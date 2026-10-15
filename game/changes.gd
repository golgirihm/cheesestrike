class_name Changes
## What changed in each version of the game, newest first, as the credits
## show it. The newest must be the version the project says it is (see
## version()), so a new version can't go out without saying what is new.

const ENTRIES := [
	["0.14.0", [
		"Colours made for colour blindness, or your own for each team and for hits.",
		"Captions say who made a sound and how far off it was, and a ring round the crosshair can show where sounds come from.",
		"One switch for less motion, and a word about flashing on the title screen.",
		"Sensitivity of its own while aiming, through a scope, and up and down, and a way to match another game's.",
		"Aim by turning the phone, and a gamepad's dead zone, response, aim assist and rumble.",
	]],
	["0.13.0", [
		"Play now, and a session list that sorts and filters.",
		"Block a player for good; names with bad language are replaced.",
		"A warning of a poor connection, and a dropped one tried again.",
		"The host is harder to cheat on speed and aim.",
	]],
	["0.12.0", [
		"A profile with a record, levels, challenges, achievements and things to earn.",
		"Medals, a summary at the end of a match, and a fuller scoreboard.",
		"A compass, pings, text chat, a weapon wheel, and a HUD laid out your way.",
	]],
]


## The version the game is, as the project says.
static func version() -> String:
	return String(ProjectSettings.get_setting("application/config/version", "0.0.0"))


## The changes as they read, the newest few versions: "What's new in 0.14.0"
## then a line for each change.
static func lines(most := 2) -> Array[String]:
	var said: Array[String] = []
	for entry: Array in ENTRIES.slice(0, most):
		said.append("%s %s" % ["What's new in" if said.is_empty() else "Before that,", entry[0]])
		for change: String in entry[1]:
			said.append("•  " + change)
	return said
