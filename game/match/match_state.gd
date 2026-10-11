class_name MatchState
extends RefCounted
## Everything about the game that the host decides, as opposed to where each
## player is, which players decide for themselves. The host changes its copy
## and sends the whole of it to everyone whenever it changes (see Match); the
## others replace theirs with what arrives.
##
## A match runs until someone (or, in teams, a team) reaches the kill limit or
## the time runs out. Then everyone sees the result for INTERMISSION seconds,
## during which nobody can be hurt, and a new match begins with scores at zero.
##
## Kept free of nodes and networking so the rules can be tested on their own.

## Seconds a knocked-out player waits before coming back.
const RESPAWN_DELAY := 3.0
## Seconds a player who has come back can't be hurt, unless they fire first.
const PROTECTION := 2.0
## Seconds between one match ending and the next beginning.
const INTERMISSION := 10.0

const PLAYING := "playing"
const ENDED := "ended"

const NO_TEAM := -1
const CHEDDAR := 0
const BRIE := 1
const TEAM_NAMES := ["Cheddar", "Brie"]

var settings := MatchSettings.new()
## Player id -> PlayerRecord.
var players := {}
var phase := PLAYING
## Seconds left in the match while playing, or in the break while ended.
var time_left := float(settings.minutes * 60)
## Knock-outs by each team, Cheddar first, in a team match.
var team_kills := [0, 0]
## Who won the last match: player ids, or teams in a team match. More than one
## is a draw.
var winners: Array[int] = []
## For each of the arena's pickup pads, in order, seconds until its pickup is
## back: 0 when it is there to collect.
var pickups: Array[float] = []


func _init(with := MatchSettings.new()) -> void:
	settings = with
	time_left = settings.minutes * 60.0


func add_player(id: int) -> PlayerRecord:
	if not players.has(id):
		var record := PlayerRecord.new(id)
		if settings.is_teams():
			record.team = smaller_team()
		players[id] = record
	return players[id]


func remove_player(id: int) -> void:
	players.erase(id)


func player(id: int) -> PlayerRecord:
	return players.get(id)


## The team a new player should join: the one with fewer players, or with the
## fewer knock-outs when they're level, so a newcomer helps whoever is behind.
func smaller_team() -> int:
	var counts := [0, 0]
	for record: PlayerRecord in players.values():
		if record.team != NO_TEAM:
			counts[record.team] += 1
	if counts[CHEDDAR] != counts[BRIE]:
		return CHEDDAR if counts[CHEDDAR] < counts[BRIE] else BRIE
	return BRIE if team_kills[BRIE] < team_kills[CHEDDAR] else CHEDDAR


## Whether `a` and `b` are on the same side, so can't hurt each other.
func are_allies(a: int, b: int) -> bool:
	var first := player(a)
	var second := player(b)
	return settings.is_teams() and first != null and second != null and first.team != NO_TEAM and first.team == second.team


## Sets a player's name, cleaned. A name that cleans to nothing is refused and
## the player keeps the name they had. Returns whether anything changed.
func rename(id: int, wanted: String) -> bool:
	var record := player(id)
	var cleaned := PlayerName.clean(wanted)
	if record == null or cleaned.is_empty() or cleaned == record.name:
		return false
	record.name = cleaned
	return true


func name_of(id: int) -> String:
	var record := player(id)
	return record.name if record != null else PlayerName.FALLBACK


func is_playing() -> bool:
	return phase == PLAYING


## `attacker` hits `victim` for `amount`. Returns what came of it:
## {"dealt": health taken, "knocked_out": whether that finished them, "ended":
## whether that won the match}. Nothing happens between matches, to a player
## who is already out or still protected, to one hitting themselves, or between
## teammates.
func damage(attacker: int, victim: int, amount: float) -> Dictionary:
	var result := {"dealt": 0, "knocked_out": false, "ended": false}
	var target := player(victim)
	if not is_playing() or target == null or not target.alive or target.is_protected():
		return result
	if attacker == victim or amount <= 0.0 or are_allies(attacker, victim):
		return result
	var dealt := mini(roundi(amount), target.health)
	target.health -= dealt
	result.dealt = dealt
	if target.health <= 0:
		target.alive = false
		target.respawn_in = RESPAWN_DELAY
		target.deaths += 1
		target.last_attacker = attacker
		target.disarm()
		var scorer := player(attacker)
		if scorer != null:
			scorer.kills += 1
			if scorer.team != NO_TEAM:
				team_kills[scorer.team] += 1
		result.knocked_out = true
		if _limit_reached():
			end_match()
			result.ended = true
	return result


## Sets up `count` pickup pads, all full.
func set_pickup_count(count: int) -> void:
	pickups.resize(count)
	pickups.fill(0.0)


func pickup_ready(index: int) -> bool:
	return index >= 0 and index < pickups.size() and pickups[index] <= 0.0


## Player `id` collects pad `index`'s pickup of `kind`, if it is there and of
## use to them. Returns "" if not, "refill" for a weapon they already carry,
## or "taken".
func collect(index: int, kind: String, id: int) -> String:
	var record := player(id)
	if not is_playing() or not pickup_ready(index) or not Pickups.wanted(kind, record):
		return ""
	pickups[index] = Pickups.respawn_time(kind)
	return "refill" if Pickups.give(kind, record) else "taken"


## Moves the clocks on by `delta` seconds. Returns the ids of players who are
## now due to come back; the host brings them back with respawn(). A match
## running out of time ends here, and the host starts the next with
## start_match() once match_due() says so.
func tick(delta: float) -> Array[int]:
	var due: Array[int] = []
	for record: PlayerRecord in players.values():
		record.protected_for = maxf(record.protected_for - delta, 0.0)
		if not record.alive and record.respawn_in > 0.0:
			record.respawn_in = maxf(record.respawn_in - delta, 0.0)
			if record.respawn_in == 0.0:
				due.append(record.id)
	for index in pickups.size():
		pickups[index] = maxf(pickups[index] - delta, 0.0)
	time_left = maxf(time_left - delta, 0.0)
	if is_playing() and time_left == 0.0:
		end_match()
	return due


## Whether the break after a match is over, so the next should start.
func match_due() -> bool:
	return phase == ENDED and time_left == 0.0


func end_match() -> void:
	phase = ENDED
	time_left = INTERMISSION
	winners = _leaders()


## Begins a new match: scores back to zero and the clock reset. Bringing
## players back into play is up to the host, with respawn().
func start_match() -> void:
	phase = PLAYING
	time_left = settings.minutes * 60.0
	team_kills = [0, 0]
	winners.clear()
	pickups.fill(0.0)
	for record: PlayerRecord in players.values():
		record.kills = 0
		record.deaths = 0
		record.last_attacker = 0
		record.disarm()


func respawn(id: int) -> void:
	var record := player(id)
	if record == null:
		return
	record.alive = true
	record.health = PlayerRecord.MAX_HEALTH
	record.respawn_in = 0.0
	record.protected_for = PROTECTION


## What the end of the last match says, such as "Brie wins!".
func result_text() -> String:
	if winners.is_empty():
		return "Nobody wins"
	var names: Array[String] = []
	for winner in winners:
		names.append(TEAM_NAMES[winner] if settings.is_teams() else name_of(winner))
	if names.size() == 1:
		return "%s wins!" % names[0]
	return "Draw: %s" % " and ".join(names)


func _limit_reached() -> bool:
	if settings.is_teams():
		return team_kills.max() >= settings.kill_limit
	for record: PlayerRecord in players.values():
		if record.kills >= settings.kill_limit:
			return true
	return false


# The players, or teams, with the most knock-outs. Nobody, if nobody scored.
func _leaders() -> Array[int]:
	var scores := {}
	if settings.is_teams():
		scores = {CHEDDAR: team_kills[CHEDDAR], BRIE: team_kills[BRIE]}
	else:
		for record: PlayerRecord in players.values():
			scores[record.id] = record.kills
	var best := 0
	for score: int in scores.values():
		best = maxi(best, score)
	var leaders: Array[int] = []
	if best > 0:
		for who: int in scores:
			if scores[who] == best:
				leaders.append(who)
	leaders.sort()
	return leaders


func to_dict() -> Dictionary:
	var listed := []
	for record: PlayerRecord in players.values():
		listed.append(record.to_dict())
	return {
		"settings": settings.to_dict(),
		"players": listed,
		"phase": phase,
		"time_left": time_left,
		"team_kills": team_kills,
		"winners": winners,
		"pickups": pickups,
	}


static func from_dict(data: Dictionary) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict(data.get("settings")))
	var listed: Variant = data.get("players")
	if listed is Array:
		for entry: Variant in listed:
			if entry is Dictionary:
				var record := PlayerRecord.from_dict(entry)
				state.players[record.id] = record
	if data.get("phase") in [PLAYING, ENDED]:
		state.phase = data.phase
	var time: Variant = data.get("time_left")
	if (time is float or time is int) and is_finite(float(time)):
		state.time_left = maxf(float(time), 0.0)
	var kills: Variant = data.get("team_kills")
	if kills is Array and kills.size() == 2 and (kills[0] is int or kills[0] is float) and (kills[1] is int or kills[1] is float):
		state.team_kills = [int(kills[0]), int(kills[1])]
	var pads: Variant = data.get("pickups")
	if pads is Array:
		for left: Variant in pads:
			state.pickups.append(maxf(float(left), 0.0) if (left is float or left is int) and is_finite(float(left)) else 0.0)
	var won: Variant = data.get("winners")
	if won is Array:
		for winner: Variant in won:
			if winner is int or winner is float:
				state.winners.append(int(winner))
	return state
