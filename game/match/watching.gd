class_name Watching
## Watching a match rather than playing in it. A player can join to watch, or
## turn to watching part way through: they are not in the arena, can't be hit
## and can't score, and see the match by following any player or flying
## free. A player who has been knocked out watches too until they come back:
## a teammate, or in a match without teams whoever knocked them out.
##
## The rules of who can be watched, kept free of nodes.

## How far behind and above a watched player's head the view sits.
const BACK := 9.0
const UP := 2.5
## How fast a free view flies, in metres a second.
const FLY_SPEED := 16.0


## Who the player `own` can watch just now, in an order that doesn't change
## as they are stepped through: everyone in play, or, for a player who is
## only knocked out in a match with teams, their own team.
static func choices(state: MatchState, own: int) -> Array[int]:
	var mine := state.player(own)
	var found: Array[int] = []
	for record: PlayerRecord in state.players.values():
		if record.id == own or not record.alive or record.watching:
			continue
		if mine != null and not mine.watching and state.settings.is_teams() and record.team != mine.team:
			continue
		found.append(record.id)
	found.sort()
	return found


## The player after (or with `step` -1, before) `current` among `among`,
## coming round at the end; the first if `current` isn't one of them, and 0
## if there is nobody.
static func next(among: Array[int], current: int, step := 1) -> int:
	if among.is_empty():
		return 0
	var at := among.find(current)
	return among[0] if at == -1 else among[posmod(at + step, among.size())]


## Who a player just knocked out is shown first: whoever knocked them out,
## if that is someone they can watch, or else the first of those they can.
static func first_for(state: MatchState, own: int) -> int:
	var among := choices(state, own)
	var mine := state.player(own)
	if mine != null and among.has(mine.last_attacker):
		return mine.last_attacker
	return among[0] if not among.is_empty() else 0


## Where the view sits to watch a player whose head is at `head`, facing
## `yaw` and looking up by `pitch`: behind and above them, looking the way
## they look.
static func chase(head: Vector3, yaw: float, pitch: float) -> Transform3D:
	var facing := Basis.from_euler(Vector3(clampf(pitch, -1.2, 1.2), yaw, 0.0))
	return Transform3D(facing, head + facing * Vector3(0.0, UP, BACK))


## What a watcher is told: who they are watching and how to change it.
static func text(state: MatchState, own: int, target: int, free: bool, touch := false) -> String:
	var mine := state.player(own)
	if mine == null or (mine.alive and not mine.watching):
		return ""
	if not mine.watching:
		return "Watching %s" % state.name_of(target) if target != 0 else ""
	if free:
		return "Flying free   ·   %s to follow a player" % ("Swap" if touch else "Space")
	if target == 0:
		return "Nobody to watch   ·   %s to fly free" % ("Swap" if touch else "Space")
	return "Watching %s   ·   %s for the next   ·   %s to fly free" % [state.name_of(target), "FIRE" if touch else "click", "Swap" if touch else "Space"]
