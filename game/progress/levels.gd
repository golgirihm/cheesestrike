class_name Levels
## Levels: experience for playing, for knocking out, for winning and for
## medals, and the level it adds up to, which is shown by a player's name.
##
## Each level takes a little more than the last. The level shown to others
## is the player's own machine's word for it, which nothing checks: it is a
## badge, and nothing in a fight depends on it.

## Experience for each minute played, each knock-out, each assist, each
## medal, and for a win, a draw and finishing a match at all.
const PER_MINUTE := 12
const PER_KNOCK_OUT := 20
const PER_ASSIST := 8
const PER_MEDAL := 25
const FOR_WIN := 120
const FOR_DRAW := 50
const FOR_FINISHING := 30
## The most minutes of one match that earn experience.
const MOST_MINUTES := 30
## The experience the first level takes, and how much more each level takes
## than the one before it.
const FIRST := 300
const STEP := 120
## The highest level there is.
const TOP := 99


## The experience a match earns (see MatchReport), and what for: {"total",
## "parts": [[what for, how much], ...]}.
static func earned(report: Dictionary) -> Dictionary:
	var parts := []
	var minutes := mini(int(report.seconds / 60.0), MOST_MINUTES)
	if minutes > 0:
		parts.append(["Time played", minutes * PER_MINUTE])
	parts.append(["Finishing the match", FOR_FINISHING])
	if report.kills > 0:
		parts.append(["Knock-outs", report.kills * PER_KNOCK_OUT])
	if report.assists > 0:
		parts.append(["Assists", report.assists * PER_ASSIST])
	var medals := 0
	for medal: String in report.medals:
		medals += report.medals[medal]
	if medals > 0:
		parts.append(["Medals", medals * PER_MEDAL])
	if report.won == true:
		parts.append(["Winning", FOR_WIN])
	elif report.won == null:
		parts.append(["A draw", FOR_DRAW])
	var total := 0
	for part: Array in parts:
		total += part[1]
	return {"total": total, "parts": parts}


## The experience it takes to go from `level` to the next.
static func to_next(level: int) -> int:
	return FIRST + STEP * clampi(level - 1, 0, TOP)


## The experience at which `level` is reached: none for the first.
static func needed(level: int) -> int:
	var total := 0
	for below in range(1, clampi(level, 1, TOP)):
		total += to_next(below)
	return total


## The level `xp` experience makes: from 1, up to the top.
static func level(xp: int) -> int:
	var reached := 1
	var left := maxi(xp, 0)
	while reached < TOP and left >= to_next(reached):
		left -= to_next(reached)
		reached += 1
	return reached


## How far `xp` is through its level, from 0 to 1: full at the top.
static func progress(xp: int) -> float:
	var reached := level(xp)
	if reached >= TOP:
		return 1.0
	return clampf(float(maxi(xp, 0) - needed(reached)) / to_next(reached), 0.0, 1.0)


## How `xp` reads: "Level 4   ·   230 of 660 to the next".
static func text(xp: int) -> String:
	var reached := level(xp)
	if reached >= TOP:
		return "Level %d" % reached
	return "Level %d   ·   %d of %d to the next" % [reached, maxi(xp, 0) - needed(reached), to_next(reached)]
