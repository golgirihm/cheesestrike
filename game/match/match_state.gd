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

## How many players a match waits for before it begins.
const MIN_TO_START := 2
## Seconds of extra time when the scores are level at the end.
const OVERTIME := 60.0
## Seconds between one round of Last mouse standing and the next, for
## everyone to choose what to bring to it.
const ROUND_BREAK := 6.0

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
## Whether the match has begun. Until enough players are in, it is a
## warm-up: everything works, but the clock stands still and nothing done
## counts once the match begins.
var started := false
## Whether the match is in extra time, when the next score that leaves
## someone ahead wins: sudden death.
var overtime := false
## Set as the match begins and as overtime does, for the host to tell
## everyone.
var just_started := false
var just_overtime := false
## Which of the arena's doors are open (see MovingParts), in its own order.
var doors: Array = []
## Seconds the session has run, which its lifts go by. It never stops or
## starts again, so that a lift never jumps.
var uptime := 0.0
## Whether the arena has lifts, so that the clock they go by is worth sending.
var has_lifts := false
## What the players are voting on just now (see Ballot), or null.
var ballot: Ballot
## The arena's blocks of cheese, and how much of each is left.
var blocks := BlockRules.new()
## Points each team has, Cheddar first, in a mode won on points.
var team_points := [0, 0]
## The wheels of a match of Capture the wheel, and the zones of Hold the
## hill or Three points; null in any other mode.
var capture: CaptureRules
var zones: ZoneRules
## Seconds left of the break between rounds of Last mouse standing, when
## nobody can be hurt; and the team that has just won one (NO_TEAM for a
## round nobody won, -2 when there is nothing to tell), for the host.
var round_break := 0.0
var round_won := -2
## Who won the last match: player ids, or teams in a team match. More than one
## is a draw.
var winners: Array[int] = []
## For each of the arena's pickup pads, in order, seconds until its pickup is
## back: 0 when it is there to collect.
var pickups: Array[float] = []
## Weapons left behind by knocked-out players, for anyone to take, oldest
## first. Each: {"id": a number of its own, "weapon": String, "at": Vector3,
## "left": seconds until it is gone}.
var drops: Array[Dictionary] = []
## Set when a drop's time runs out, for the host to tell everyone.
var drop_ended := false
var _next_drop := 1
## How many power-ups have been taken this match, which decides the next.
var powers_taken := 0
## The ids of players whose Overload has just finished building up and
## begun, for the host to act on and tell everyone.
var overloads_begun: Array[int] = []
## Set when a power-up runs out, for the host to tell everyone.
var power_ended := false


func _init(with := MatchSettings.new()) -> void:
	settings = with
	time_left = settings.minutes * 60.0
	if settings.mode == MatchSettings.CAPTURE:
		capture = CaptureRules.new()
	elif settings.mode == MatchSettings.HILL or settings.mode == MatchSettings.POINTS:
		zones = ZoneRules.new()


## A team's score in the mode being played: points, or knock-outs.
func team_score(team: int) -> int:
	return team_points[team] if settings.by_points() else team_kills[team]


## Gives `team` `points`, which may win the match. Returns whether it did.
func score(team: int, points: int) -> bool:
	if not is_playing() or team < 0 or team > 1 or points <= 0:
		return false
	team_points[team] += points
	if started and (_limit_reached() or (overtime and not _level())):
		end_match()
		return true
	return false


## Leaves `record`'s owner with what they come into play with: their
## loadout, or in a gun game the weapon they have worked their way to.
func rearm(record: PlayerRecord) -> void:
	record.disarm()
	record.endless = false
	if settings.mode == MatchSettings.GUN_GAME:
		var weapon := GunGame.weapon_for(record.kills)
		record.weapons = [weapon]
		record.weapon = weapon
		record.fitted = Attachments.NONE
	elif settings.fixed_weapons():
		# The rules say what everyone has: that, and it never runs out, as
		# there is nothing else to fall back on.
		record.weapons = MatchSettings.WEAPON_SETS[settings.weapons].duplicate()
		record.weapon = record.weapons[0]
		record.fitted = Attachments.NONE
		record.endless = true


## How fast `record`'s owner moves, as a share of normal speed.
func speed_factor(record: PlayerRecord) -> float:
	var factor := Overload.speed_factor(record) * settings.speed
	if record != null and capture != null and capture.carried_by(record.id) != -1:
		factor *= CaptureRules.CARRY_SPEED
	return factor


## Host only: moves the mode's own rules on by `delta` seconds. `spots` are
## the arena's places for them ({"bases", "hills", "points"}); `standing`
## maps each player's id to {"at": their feet, "team", "alive"}. Returns
## what everyone should be told, in order: each {"text", "team": whose
## colour to write it in}.
func update_objectives(delta: float, spots: Dictionary, standing: Dictionary) -> Array[Dictionary]:
	var told: Array[Dictionary] = []
	if not is_playing():
		return told
	if capture != null and spots.get("bases", []).size() == 2:
		for event: Dictionary in capture.update(delta, spots.bases, standing):
			var wheel: String = TEAM_NAMES[event.wheel]
			var other: int = 1 - event.wheel
			match event.type:
				CaptureRules.TAKEN:
					told.append({"text": "%s has the %s wheel!" % [name_of(event.by), wheel], "team": other, "by": event.by, "points": Stats.TAKE_POINTS})
				CaptureRules.FELL:
					told.append({"text": "The %s wheel is dropped" % wheel, "team": event.wheel})
				CaptureRules.RETURNED:
					told.append({"text": "The %s wheel is home" % wheel, "team": event.wheel})
				CaptureRules.CAPTURED:
					told.append({"text": "%s scores for %s!" % [name_of(event.by), TEAM_NAMES[other]], "team": other, "by": event.by, "points": Stats.CAPTURE_POINTS})
					score(other, 1)
	elif zones != null and settings.mode == MatchSettings.HILL:
		var hill := zones.update_hill(delta, spots.get("hills", []), standing)
		for team in 2:
			score(team, hill.points[team])
		if hill.moved:
			told.append({"text": "The hill has moved", "team": NO_TEAM})
	elif zones != null:
		var held := zones.update_points(delta, spots.get("points", []), standing)
		for taken: Array in held.taken:
			told.append({"text": "%s takes point %s" % [TEAM_NAMES[taken[1]], ZoneRules.NAMES[taken[0]]], "team": taken[1]})
		for team in 2:
			score(team, held.points[team])
	return told


## What a player is told of the mode's own business, under the score: where
## the wheels are, when the hill moves, who has which point, how the round
## stands, or which weapon of the gun game they are on.
func objective_text(own: int) -> String:
	if not is_playing():
		return ""
	if settings.is_practice():
		return "Every weapon is on the rack behind you  ·  what each has done is on the right"
	if capture != null:
		var parts: Array[String] = []
		for team in 2:
			var wheel: Dictionary = capture.wheels[team]
			var where := "home"
			if wheel.status == CaptureRules.CARRIED:
				where = "with %s" % name_of(wheel.carrier)
			elif wheel.status == CaptureRules.DROPPED:
				where = "dropped"
			parts.append("%s wheel %s" % [TEAM_NAMES[team], where])
		return " · ".join(parts)
	if zones != null and settings.mode == MatchSettings.HILL:
		return "The hill moves in %ds" % ceili(zones.hill_left)
	if zones != null:
		return zones.points_text()
	if settings.mode == MatchSettings.ROUNDS:
		if round_break > 0.0:
			return "Next round in %d: choose your weapons in the menu" % ceili(round_break)
		return "%d left of Cheddar · %d of Brie" % [alive_in(CHEDDAR), alive_in(BRIE)]
	if settings.mode == MatchSettings.GUN_GAME and player(own) != null:
		return GunGame.text(player(own).kills)
	return ""


## How many of `team` are in play.
func alive_in(team: int) -> int:
	var count := 0
	for record: PlayerRecord in players.values():
		if record.team == team and record.alive:
			count += 1
	return count


# Last mouse standing: a round is over once a team with players has none
# left in play. The other takes it, and after a break everyone comes back.
func _check_round() -> void:
	if settings.mode != MatchSettings.ROUNDS or not is_playing() or round_break > 0.0 or not started:
		return
	var members := [0, 0]
	for record: PlayerRecord in players.values():
		if record.team != NO_TEAM:
			members[record.team] += 1
	if members[CHEDDAR] == 0 or members[BRIE] == 0:
		return
	var left := [alive_in(CHEDDAR), alive_in(BRIE)]
	if left[CHEDDAR] > 0 and left[BRIE] > 0:
		return
	round_won = NO_TEAM
	for team in 2:
		if left[team] > 0:
			round_won = team
	if round_won != NO_TEAM and score(round_won, 1):
		return
	round_break = ROUND_BREAK
	for record: PlayerRecord in players.values():
		record.respawn_in = ROUND_BREAK


func add_player(id: int) -> PlayerRecord:
	if not players.has(id):
		var record := PlayerRecord.new(id)
		record.health = settings.health
		if settings.is_teams():
			record.team = smaller_team()
		players[id] = record
		rearm(record)
		# The practice range has no match to begin.
		if not started and playing_count() >= MIN_TO_START and not settings.is_practice():
			_begin()
	return players[id]


## How many are in the match to play, rather than to watch.
func playing_count() -> int:
	var count := 0
	for record: PlayerRecord in players.values():
		if not record.watching:
			count += 1
	return count


## Player `id` turns to watching, or back to playing. One who watches is out
## of the arena at once and on no team; one who goes back to playing joins
## the smaller team and comes into play as a knocked-out player does.
## Returns whether anything changed.
func set_watching(id: int, on: bool) -> bool:
	var record := player(id)
	if record == null or record.watching == on:
		return false
	record.watching = on
	if on:
		record.alive = false
		record.respawn_in = 0.0
		record.team = NO_TEAM
		record.overload_left = 0.0
		record.overload_build = 0.0
		record.give_power(Powers.NONE, 0.0)
		_check_round()
		# Someone who joined to watch was a player for a moment, which may
		# have begun the match: with too few left to play and nothing yet
		# scored, it goes back to waiting.
		if started and playing_count() < MIN_TO_START and _nothing_scored():
			started = false
			overtime = false
			time_left = settings.minutes * 60.0
	else:
		if settings.is_teams():
			record.team = smaller_team()
		rearm(record)
		record.respawn_in = RESPAWN_DELAY
		if not started and playing_count() >= MIN_TO_START and not settings.is_practice():
			_begin()
	return true


## Whether the match is waiting for players before it begins.
func is_warm_up() -> bool:
	return is_playing() and not started


# Enough players are in: the warm-up is over and the match begins from
# nothing, with everyone where they stand.
func _begin() -> void:
	started = true
	just_started = true
	overtime = false
	time_left = settings.minutes * 60.0
	team_kills = [0, 0]
	team_points = [0, 0]
	blocks.reset(blocks.health.size())
	for record: PlayerRecord in players.values():
		record.kills = 0
		record.deaths = 0
		record.overload = 0.0
		if settings.mode == MatchSettings.GUN_GAME:
			rearm(record)


func _nothing_scored() -> bool:
	return team_points == [0, 0] and players.values().all(func(record: PlayerRecord) -> bool: return record.kills == 0)


# Whether nobody is ahead: the leaders are level, or nobody has scored.
func _level() -> bool:
	return _leaders().size() != 1


## A state for a match on `next` settings with the same players in it: who
## they are, what they wear and what they have chosen, on new teams if
## there are teams, and nothing else of the match that was.
func with_settings(next: MatchSettings) -> MatchState:
	var fresh := MatchState.new(next)
	fresh.uptime = uptime
	for old: PlayerRecord in players.values():
		var record := fresh.add_player(old.id)
		record.name = old.name
		record.look = old.look
		record.main = old.main
		record.side = old.side
		record.attachment = old.attachment
		record.overload_kind = old.overload_kind
		record.ping = old.ping
		fresh.rearm(record)
		if old.watching:
			fresh.set_watching(old.id, true)
	return fresh


## The real players in the session, who may vote: not bots.
func voters() -> Array[int]:
	var found: Array[int] = []
	for record: PlayerRecord in players.values():
		if not Bots.is_bot(record.id):
			found.append(record.id)
	return found


func remove_player(id: int) -> void:
	players.erase(id)
	if ballot != null:
		ballot.forget(id)
	_check_round()


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
## the player keeps the name they had; one with bad language in it is
## replaced (see PlayerName.decent). Returns whether anything changed.
func rename(id: int, wanted: String) -> bool:
	var record := player(id)
	var cleaned := PlayerName.clean(wanted)
	if not cleaned.is_empty() and not PlayerName.decent(cleaned):
		cleaned = PlayerName.stand_in(id)
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
## whether that won the match, "absorbed": armour taken with it}. Nothing
## happens between matches, to a player
## who is already out or still protected, or between teammates. A player only
## hurts themselves with `own_too`, as with a burst of their own; that fills
## no meter and scores nothing.
func damage(attacker: int, victim: int, amount: float, own_too := false) -> Dictionary:
	var result := {"dealt": 0, "knocked_out": false, "ended": false, "absorbed": 0}
	var target := player(victim)
	if not is_playing() or target == null or not target.alive or target.is_protected() or round_break > 0.0:
		return result
	var own := attacker == victim
	# What someone threw before they turned to watching does nothing.
	if not own and player(attacker) != null and player(attacker).watching:
		return result
	# Teammates can't hurt each other, unless the host's rules say they can:
	# then it does them no good.
	var mates := not own and are_allies(attacker, victim)
	if (own and not own_too) or amount <= 0.0 or (mates and not settings.friendly_fire):
		return result
	var scorer := player(attacker) if not own and not mates else null
	# Power-ups: more from an attacker with double damage, less to a shield.
	amount *= Powers.outgoing(scorer) * Powers.incoming(target) * Overload.incoming(target)
	var taken := target.take(roundi(amount))
	var dealt: int = taken.dealt
	result.dealt = dealt
	result.absorbed = taken.absorbed
	if scorer != null:
		if Overload.is_active(scorer, Overload.MELT):
			target.melted_for = Overload.MELT_DURATION
		elif scorer.overload_left <= 0.0:
			scorer.overload = minf(scorer.overload + dealt * Overload.CHARGE_PER_DAMAGE, Overload.METER_FULL)
	if target.health <= 0:
		target.alive = false
		# One life a round, in Last mouse standing.
		target.respawn_in = 0.0 if settings.mode == MatchSettings.ROUNDS else settings.respawn
		target.deaths += 1
		target.last_attacker = attacker
		rearm(target)
		target.overload *= Overload.KEPT_ON_KNOCK_OUT
		target.overload_left = 0.0
		target.overload_build = 0.0
		target.melted_for = 0.0
		target.give_power(Powers.NONE, 0.0)
		if scorer != null:
			scorer.kills += 1
			if scorer.team != NO_TEAM:
				team_kills[scorer.team] += 1
			if settings.mode == MatchSettings.GUN_GAME and scorer.alive:
				# On to the next weapon, at once.
				rearm(scorer)
		result.knocked_out = true
		if started and (_limit_reached() or (overtime and not _level())):
			end_match()
			result.ended = true
		_check_round()
		result.ended = not is_playing()
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
	if not is_playing() or not pickup_ready(index) or not Pickups.wanted(kind, record, settings.health):
		return ""
	# In a gun game the only weapon is the one a player has earned, and under
	# some rules the only ones are those the rules give.
	if (settings.mode == MatchSettings.GUN_GAME or settings.fixed_weapons()) and Weapons.ids().has(kind):
		return ""
	# On the practice range everything is back at once, and a weapon taken
	# from the rack comes into hand.
	pickups[index] = Pickups.respawn_time(kind) if not settings.is_practice() else 1.0
	if settings.is_practice() and Weapons.ids().has(kind) and not record.weapons.has(kind):
		record.weapon = kind
	if kind == Pickups.POWER:
		var power := Powers.next(powers_taken)
		powers_taken += 1
		record.give_power(power, Powers.DURATION[power])
		return "taken"
	return "refill" if Pickups.give(kind, record, settings.health) else "taken"


## Leaves `weapon` lying at `at`. Returns the drop's id, or 0 if it is not
## a weapon that is left behind: a sidearm, which everyone has, is not.
func drop(weapon: String, at: Vector3) -> int:
	var stats := Weapons.find(weapon)
	if not is_playing() or stats == null or not Weapons.ids().has(weapon) or stats.slot == Weapons.SIDE or not at.is_finite():
		return 0
	if settings.mode == MatchSettings.GUN_GAME or settings.fixed_weapons():
		return 0
	drops.append({"id": _next_drop, "weapon": weapon, "at": at, "left": Pickups.DROP_LIFE})
	_next_drop += 1
	while drops.size() > Pickups.MAX_DROPS:
		drops.pop_front()
	return _next_drop - 1


## Player `id` takes drop `drop_id`, if it is there and they are in play.
## Returns "taken" for a weapon new to them, "refill" for one they carry
## already, which it fills, or "" if they took nothing.
func take_drop(drop_id: int, id: int) -> String:
	var record := player(id)
	if not is_playing() or record == null or not record.alive:
		return ""
	for index in drops.size():
		if drops[index].id == drop_id:
			var weapon: String = drops[index].weapon
			drops.remove_at(index)
			if record.weapons.has(weapon):
				return "refill"
			record.weapons.append(weapon)
			return "taken"
	return ""


## Empties pad `index` for `seconds`, as a power-up's is when a match begins.
func hold_back(index: int, seconds: float) -> void:
	if index >= 0 and index < pickups.size():
		pickups[index] = seconds


## Moves the clocks on by `delta` seconds. Returns the ids of players who are
## now due to come back; the host brings them back with respawn(). A match
## running out of time ends here, and the host starts the next with
## start_match() once match_due() says so.
func tick(delta: float) -> Array[int]:
	var due: Array[int] = []
	for record: PlayerRecord in players.values():
		record.protected_for = maxf(record.protected_for - delta, 0.0)
		if record.overload_build > 0.0:
			# The Overload itself doesn't start to run down until it begins.
			var into := delta - record.overload_build
			record.overload_build = maxf(record.overload_build - delta, 0.0)
			if record.overload_build == 0.0:
				overloads_begun.append(record.id)
				record.overload_left = maxf(record.overload_left - maxf(into, 0.0), 0.0)
		else:
			record.overload_left = maxf(record.overload_left - delta, 0.0)
		record.melted_for = maxf(record.melted_for - delta, 0.0)
		if record.power != Powers.NONE:
			record.power_left = maxf(record.power_left - delta, 0.0)
			if record.power_left == 0.0:
				record.power = Powers.NONE
				power_ended = true
		# Whoever is waiting to come back: the knocked out, and at the end of
		# a round of Last mouse standing, everyone.
		if record.respawn_in > 0.0:
			record.respawn_in = maxf(record.respawn_in - delta, 0.0)
			if record.respawn_in == 0.0:
				due.append(record.id)
	for index in pickups.size():
		pickups[index] = maxf(pickups[index] - delta, 0.0)
	for lying in drops:
		lying.left -= delta
	if drops.any(func(lying: Dictionary) -> bool: return lying.left <= 0.0):
		drops = drops.filter(func(lying: Dictionary) -> bool: return lying.left > 0.0)
		drop_ended = true
	round_break = maxf(round_break - delta, 0.0)
	uptime += delta
	if ballot != null:
		ballot.left = maxf(ballot.left - delta, 0.0)
	# The clock stands still through a warm-up.
	if is_warm_up():
		return due
	time_left = maxf(time_left - delta, 0.0)
	if is_playing() and time_left == 0.0:
		if not overtime and _level() and playing_count() >= MIN_TO_START:
			# Level at the end: extra time, and the next score wins.
			overtime = true
			just_overtime = true
			time_left = OVERTIME
		else:
			end_match()
	return due


## Player `id` sets off their Overload, if their meter is full. Returns
## whether it began.
func set_off_overload(id: int) -> bool:
	var record := player(id)
	if not is_playing() or not Overload.is_ready(record):
		return false
	record.overload = 0.0
	record.overload_left = Overload.DURATIONS[record.overload_kind]
	record.overload_build = Overload.BUILD_UP
	return true


## Player `id` chooses Overload `kind` for when they next set one off: not
## the one already under way.
func choose_overload(id: int, kind: Variant) -> bool:
	var record := player(id)
	if record == null or not Overload.is_kind(kind) or record.overload_kind == kind or record.overload_left > 0.0:
		return false
	record.overload_kind = kind
	return true


## Whether the break after a match is over, so the next should start.
func match_due() -> bool:
	return phase == ENDED and time_left == 0.0


func end_match() -> void:
	overtime = false
	# While the result is up, everyone votes on what to play next.
	ballot = Ballot.for_next(settings) if not settings.is_practice() else null
	phase = ENDED
	time_left = INTERMISSION
	winners = _leaders()


## Begins a new match: scores back to zero and the clock reset. Bringing
## players back into play is up to the host, with respawn().
func start_match() -> void:
	phase = PLAYING
	overtime = false
	ballot = null
	started = playing_count() >= MIN_TO_START and not settings.is_practice()
	time_left = settings.minutes * 60.0
	team_kills = [0, 0]
	team_points = [0, 0]
	round_break = 0.0
	round_won = -2
	if capture != null:
		capture = CaptureRules.new()
	if zones != null:
		zones = ZoneRules.new()
	winners.clear()
	pickups.fill(0.0)
	blocks.reset(blocks.health.size())
	drops.clear()
	powers_taken = 0
	for record: PlayerRecord in players.values():
		record.give_power(Powers.NONE, 0.0)
		record.kills = 0
		record.deaths = 0
		record.last_attacker = 0
		rearm(record)
		record.overload = 0.0
		record.overload_left = 0.0
		record.overload_build = 0.0
		record.melted_for = 0.0


func respawn(id: int) -> void:
	var record := player(id)
	# Someone watching stays out until they choose to play.
	if record == null or record.watching:
		return
	record.alive = true
	record.health = settings.health
	record.respawn_in = 0.0
	record.protected_for = PROTECTION
	record.armour = 0
	record.grenades = Weapons.GRENADES


## Player `id` throws a grenade, if they are in play and have one. Returns
## whether they did.
func throw_grenade(id: int) -> bool:
	var record := player(id)
	if not is_playing() or record == null or not record.alive or record.grenades <= 0:
		return false
	record.grenades -= 1
	return true


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
	if settings.by_points():
		return team_points.max() >= settings.limit()
	if settings.mode == MatchSettings.GUN_GAME:
		return players.values().any(func(record: PlayerRecord) -> bool: return record.kills >= settings.limit())
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
		scores = {CHEDDAR: team_score(CHEDDAR), BRIE: team_score(BRIE)}
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
		listed.append(record.to_packed())
	var data := {
		"settings": settings.to_dict(),
		"players": listed,
		"phase": phase,
		"time_left": time_left,
		"team_kills": team_kills,
		"winners": winners,
		"pickups": pickups,
	}
	# Only what is so, and what the mode being played has, as this is sent often.
	if not started:
		data["warm_up"] = true
	if overtime:
		data["overtime"] = true
	if settings.by_points():
		data["points"] = team_points
	if capture != null or zones != null:
		data["objective"] = capture.to_array() if capture != null else zones.to_array()
	if settings.mode == MatchSettings.ROUNDS:
		data["round_break"] = round_break
	if not blocks.health.is_empty():
		data["blocks"] = blocks.to_array()
	if ballot != null:
		data["ballot"] = ballot.to_array()
	if not doors.is_empty():
		data["doors"] = doors
		data["uptime"] = uptime
	elif uptime > 0.0 and has_lifts:
		data["uptime"] = uptime
	if not drops.is_empty():
		data["drops"] = drops.map(func(lying: Dictionary) -> Array: return [lying.id, Weapons.ORDER.find(lying.weapon), lying.at, lying.left])
	return data


static func from_dict(data: Dictionary) -> MatchState:
	var state := MatchState.new(MatchSettings.from_dict(data.get("settings")))
	var listed: Variant = data.get("players")
	if listed is Array:
		for entry: Variant in listed:
			var record := PlayerRecord.from_packed(entry)
			if record != null:
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
	var points: Variant = data.get("points")
	if points is Array and points.size() == 2 and (points[0] is int or points[0] is float) and (points[1] is int or points[1] is float):
		state.team_points = [maxi(int(points[0]), 0), maxi(int(points[1]), 0)]
	if state.capture != null:
		state.capture = CaptureRules.from_array(data.get("objective"))
	if state.zones != null:
		state.zones = ZoneRules.from_array(data.get("objective"))
	var pause: Variant = data.get("round_break")
	if (pause is float or pause is int) and is_finite(float(pause)):
		state.round_break = clampf(float(pause), 0.0, ROUND_BREAK)
	state.started = data.get("warm_up") != true
	state.overtime = data.get("overtime") == true
	state.just_started = false
	state.blocks = BlockRules.from_array(data.get("blocks"))
	state.ballot = Ballot.from_array(data.get("ballot"))
	var open: Variant = data.get("doors")
	if open is Array:
		for door: Variant in open.slice(0, 32):
			state.doors.append(door == true)
	var run: Variant = data.get("uptime")
	if (run is float or run is int) and is_finite(float(run)):
		state.uptime = maxf(float(run), 0.0)
		state.has_lifts = true
	var lying: Variant = data.get("drops")
	if lying is Array:
		for entry: Variant in lying.slice(0, Pickups.MAX_DROPS):
			if not entry is Array or entry.size() != 4 or not entry[0] is int or not entry[1] is int or not entry[2] is Vector3:
				continue
			if entry[1] < 0 or entry[1] >= Weapons.ORDER.size() or not entry[2].is_finite() or not (entry[3] is float or entry[3] is int) or not is_finite(float(entry[3])):
				continue
			state.drops.append({"id": entry[0], "weapon": Weapons.ORDER[entry[1]], "at": entry[2], "left": clampf(float(entry[3]), 0.0, Pickups.DROP_LIFE)})
	var won: Variant = data.get("winners")
	if won is Array:
		for winner: Variant in won:
			if winner is int or winner is float:
				state.winners.append(int(winner))
	return state
