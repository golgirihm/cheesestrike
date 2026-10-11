class_name PlayerRecord
extends RefCounted
## What the host knows about one player beyond where they are: their name and,
## as the game grows, their health and score. The host owns these and sends
## them to everyone (see MatchState).

var id: int
var name := PlayerName.FALLBACK


func _init(player_id := 0) -> void:
	id = player_id


func to_dict() -> Dictionary:
	return {"id": id, "name": name}


## The record described by `data`, as made by to_dict. Anything missing or of
## the wrong type is left at its default.
static func from_dict(data: Dictionary) -> PlayerRecord:
	var record := PlayerRecord.new(_int(data, "id", 0))
	record.name = _string(data, "name", PlayerName.FALLBACK)
	return record


static func _int(data: Dictionary, key: String, default: int) -> int:
	var value: Variant = data.get(key)
	return int(value) if value is int or value is float else default


static func _string(data: Dictionary, key: String, default: String) -> String:
	var value: Variant = data.get(key)
	return value if value is String else default
