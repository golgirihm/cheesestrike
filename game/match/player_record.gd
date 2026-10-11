class_name PlayerRecord
extends RefCounted
## What the host knows about one player beyond where they are: their name,
## health and score. The host owns these and sends them to everyone (see
## MatchState).

const MAX_HEALTH := 100

var id: int
var name := PlayerName.FALLBACK
var health := MAX_HEALTH
var alive := true
## Seconds until a knocked-out player comes back.
var respawn_in := 0.0
## Seconds left in which a player who has just come back can't be hurt.
var protected_for := 0.0
var kills := 0
var deaths := 0
## Who knocked this player out last, or 0.
var last_attacker := 0
## The weapon in their hands.
var weapon := Weapons.STARTING


func _init(player_id := 0) -> void:
	id = player_id


func is_protected() -> bool:
	return protected_for > 0.0


func to_dict() -> Dictionary:
	return {
		"id": id,
		"name": name,
		"health": health,
		"alive": alive,
		"respawn_in": respawn_in,
		"protected_for": protected_for,
		"kills": kills,
		"deaths": deaths,
		"last_attacker": last_attacker,
		"weapon": weapon,
	}


## The record described by `data`, as made by to_dict. Anything missing or of
## the wrong type is left at its default.
static func from_dict(data: Dictionary) -> PlayerRecord:
	var record := PlayerRecord.new(_int(data, "id", 0))
	record.name = _string(data, "name", record.name)
	record.health = _int(data, "health", record.health)
	record.alive = _bool(data, "alive", record.alive)
	record.respawn_in = _float(data, "respawn_in", record.respawn_in)
	record.protected_for = _float(data, "protected_for", record.protected_for)
	record.kills = _int(data, "kills", record.kills)
	record.deaths = _int(data, "deaths", record.deaths)
	record.last_attacker = _int(data, "last_attacker", record.last_attacker)
	record.weapon = _string(data, "weapon", record.weapon)
	return record


static func _int(data: Dictionary, key: String, default: int) -> int:
	var value: Variant = data.get(key)
	return int(value) if value is int or value is float else default


static func _float(data: Dictionary, key: String, default: float) -> float:
	var value: Variant = data.get(key)
	return float(value) if value is int or value is float else default


static func _bool(data: Dictionary, key: String, default: bool) -> bool:
	var value: Variant = data.get(key)
	return value if value is bool else default


static func _string(data: Dictionary, key: String, default: String) -> String:
	var value: Variant = data.get(key)
	return value if value is String else default
