class_name MatchState
extends RefCounted
## Everything about the game that the host decides, as opposed to where each
## player is, which players decide for themselves. The host changes its copy
## and sends the whole of it to everyone whenever it changes (see Match); the
## others replace theirs with what arrives.
##
## Kept free of nodes and networking so the rules can be tested on their own.

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
