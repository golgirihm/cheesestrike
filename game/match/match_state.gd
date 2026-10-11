class_name MatchState
extends RefCounted
## Everything about the game that the host decides, as opposed to where each
## player is, which players decide for themselves. The host changes its copy
## and sends the whole of it to everyone whenever it changes (see Match); the
## others replace theirs with what arrives.
##
## Kept free of nodes and networking so the rules can be tested on their own.

## Seconds a knocked-out player waits before coming back.
const RESPAWN_DELAY := 3.0
## Seconds a player who has come back can't be hurt, unless they fire first.
const PROTECTION := 2.0

## Player id -> PlayerRecord.
var players := {}


func add_player(id: int) -> PlayerRecord:
	if not players.has(id):
		players[id] = PlayerRecord.new(id)
	return players[id]


func remove_player(id: int) -> void:
	players.erase(id)


func player(id: int) -> PlayerRecord:
	return players.get(id)


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


## `attacker` hits `victim` for `amount`. Returns what came of it:
## {"dealt": health taken, "knocked_out": whether that finished them}. Nothing
## happens to a player who is already out or still protected, or to one hitting
## themselves.
func damage(attacker: int, victim: int, amount: float) -> Dictionary:
	var result := {"dealt": 0, "knocked_out": false}
	var target := player(victim)
	if target == null or not target.alive or target.is_protected() or attacker == victim or amount <= 0.0:
		return result
	var dealt := mini(roundi(amount), target.health)
	target.health -= dealt
	result.dealt = dealt
	if target.health <= 0:
		target.alive = false
		target.respawn_in = RESPAWN_DELAY
		target.deaths += 1
		target.last_attacker = attacker
		var scorer := player(attacker)
		if scorer != null:
			scorer.kills += 1
		result.knocked_out = true
	return result


## Moves the clocks on by `delta` seconds. Returns the ids of players who are
## now due to come back; the host brings them back with respawn().
func tick(delta: float) -> Array[int]:
	var due: Array[int] = []
	for record: PlayerRecord in players.values():
		record.protected_for = maxf(record.protected_for - delta, 0.0)
		if not record.alive and record.respawn_in > 0.0:
			record.respawn_in = maxf(record.respawn_in - delta, 0.0)
			if record.respawn_in == 0.0:
				due.append(record.id)
	return due


func respawn(id: int) -> void:
	var record := player(id)
	if record == null:
		return
	record.alive = true
	record.health = PlayerRecord.MAX_HEALTH
	record.respawn_in = 0.0
	record.protected_for = PROTECTION


func to_dict() -> Dictionary:
	var listed := []
	for record: PlayerRecord in players.values():
		listed.append(record.to_dict())
	return {"players": listed}


static func from_dict(data: Dictionary) -> MatchState:
	var state := MatchState.new()
	var listed: Variant = data.get("players")
	if listed is Array:
		for entry: Variant in listed:
			if entry is Dictionary:
				var record := PlayerRecord.from_dict(entry)
				state.players[record.id] = record
	return state
