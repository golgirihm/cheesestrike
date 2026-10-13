class_name MatchSummary
## What the end of a match shows: who came first, second and third, and what
## this player did in it: how much of what they fired hit, the damage they
## dealt, their best run and their favourite weapon, and the medals they
## earned.

const PLACES := ["1st", "2nd", "3rd"]


## The three who did best, best first: [{"place", "name", "kills", "id"}].
## Most knock-outs, then fewest times knocked out, then by name; those only
## watching aren't placed.
static func podium(state: MatchState) -> Array[Dictionary]:
	var players: Array[PlayerRecord] = []
	for record: PlayerRecord in state.players.values():
		if not record.watching:
			players.append(record)
	players.sort_custom(func(a: PlayerRecord, b: PlayerRecord) -> bool:
		if a.kills != b.kills:
			return a.kills > b.kills
		if a.deaths != b.deaths:
			return a.deaths < b.deaths
		return a.name.naturalnocasecmp_to(b.name) < 0)
	var placed: Array[Dictionary] = []
	for index in mini(players.size(), PLACES.size()):
		placed.append({"place": PLACES[index], "name": players[index].name, "kills": players[index].kills, "id": players[index].id})
	return placed


## The podium as it reads, a line for each place.
static func podium_lines(state: MatchState) -> Array[String]:
	var lines: Array[String] = []
	for stood: Dictionary in podium(state):
		lines.append("%s   %s   ·   %d" % [stood.place, stood.name, stood.kills])
	return lines


## What a player whose stats are `line` (see Stats) did, as it reads.
static func numbers(line: Dictionary) -> Array[String]:
	var said: Array[String] = [
		"Accuracy %d%%" % Stats.accuracy(line),
		"Damage %d" % line.damage,
		"Best run %d" % line.best,
	]
	if line.assists > 0:
		said.append("Assists %d" % line.assists)
	var favourite := Stats.favourite(line)
	if not favourite.is_empty():
		var weapon := Weapons.find(favourite)
		said.append("Favourite: %s" % (weapon.display_name if weapon != null else Melee.NAME))
	return said


## Everything the end of the match says to player `own`, a line at a time:
## the podium, their numbers, and their medals if they earned any.
static func lines(state: MatchState, own: int, stats: Stats, medals: Medals) -> Array[String]:
	var said := podium_lines(state)
	var record := state.player(own)
	if record != null and not record.watching and stats != null:
		said.append("   ·   ".join(numbers(stats.of(own))))
	if medals != null and not medals.summary().is_empty():
		said.append("Medals: %s" % medals.summary())
	return said
