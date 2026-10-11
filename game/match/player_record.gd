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
## Every weapon they carry. Picked-up weapons are lost on being knocked out.
var weapons: Array = [Weapons.STARTING]
## Their team in a team match, or MatchState.NO_TEAM.
var team := -1
## The Overload meter, filling to Overload.METER_FULL; seconds left in an
## Overload set off; and seconds left melted by someone else's.
var overload := 0.0
var overload_left := 0.0
var melted_for := 0.0
## Round trip to the host, in milliseconds, as the host measures it.
var ping := 0

# Where each number sits in a packed record (see to_packed).
enum { _ID, _HEALTH, _FLAGS, _RESPAWN, _PROTECTED, _KILLS, _DEATHS, _LAST_ATTACKER,
	_WEAPON, _WEAPONS, _TEAM, _OVERLOAD, _OVERLOAD_LEFT, _MELTED, _PING, _COUNT }
const _ALIVE := 1


func _init(player_id := 0) -> void:
	id = player_id


## Back to only the starting weapon, as after being knocked out.
func disarm() -> void:
	weapons = [Weapons.STARTING]
	weapon = Weapons.STARTING


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
		"weapons": weapons,
		"team": team,
		"overload": overload,
		"overload_left": overload_left,
		"melted_for": melted_for,
		"ping": ping,
	}


## The record as compactly as it can travel, as the host sends it many times a
## second to everyone: [name, PackedInt32Array of the rest]. Times are whole
## milliseconds, the meter hundredths, and the weapons carried a bit each.
func to_packed() -> Array:
	var numbers := PackedInt32Array()
	numbers.resize(_COUNT)
	numbers[_ID] = id
	numbers[_HEALTH] = health
	numbers[_FLAGS] = _ALIVE if alive else 0
	numbers[_RESPAWN] = roundi(respawn_in * 1000.0)
	numbers[_PROTECTED] = roundi(protected_for * 1000.0)
	numbers[_KILLS] = kills
	numbers[_DEATHS] = deaths
	numbers[_LAST_ATTACKER] = last_attacker
	numbers[_WEAPON] = Weapons.ORDER.find(weapon)
	var mask := 0
	for carried: String in weapons:
		mask |= 1 << Weapons.ORDER.find(carried)
	numbers[_WEAPONS] = mask
	numbers[_TEAM] = team
	numbers[_OVERLOAD] = roundi(overload * 100.0)
	numbers[_OVERLOAD_LEFT] = roundi(overload_left * 1000.0)
	numbers[_MELTED] = roundi(melted_for * 1000.0)
	numbers[_PING] = ping
	return [name, numbers]


## The record packed by to_packed, or null if `data` isn't one.
static func from_packed(data: Variant) -> PlayerRecord:
	if not (data is Array and data.size() == 2 and data[0] is String and data[1] is PackedInt32Array):
		return null
	var numbers: PackedInt32Array = data[1]
	if numbers.size() < _COUNT:
		return null
	var record := PlayerRecord.new(numbers[_ID])
	record.name = data[0]
	record.health = clampi(numbers[_HEALTH], 0, MAX_HEALTH)
	record.alive = numbers[_FLAGS] & _ALIVE != 0
	record.respawn_in = maxi(numbers[_RESPAWN], 0) / 1000.0
	record.protected_for = maxi(numbers[_PROTECTED], 0) / 1000.0
	record.kills = numbers[_KILLS]
	record.deaths = numbers[_DEATHS]
	record.last_attacker = numbers[_LAST_ATTACKER]
	record.weapons = []
	for index in Weapons.ORDER.size():
		if numbers[_WEAPONS] & (1 << index):
			record.weapons.append(Weapons.ORDER[index])
	if record.weapons.is_empty():
		record.weapons = [Weapons.STARTING]
	var held := numbers[_WEAPON]
	record.weapon = Weapons.ORDER[held] if held >= 0 and held < Weapons.ORDER.size() else Weapons.STARTING
	if not record.weapons.has(record.weapon):
		record.weapon = record.weapons[0]
	record.team = clampi(numbers[_TEAM], -1, 1)
	record.overload = clampf(numbers[_OVERLOAD] / 100.0, 0.0, Overload.METER_FULL)
	record.overload_left = maxi(numbers[_OVERLOAD_LEFT], 0) / 1000.0
	record.melted_for = maxi(numbers[_MELTED], 0) / 1000.0
	record.ping = maxi(numbers[_PING], 0)
	return record


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
	record.team = clampi(_int(data, "team", record.team), -1, 1)
	record.overload = clampf(_float(data, "overload", 0.0), 0.0, Overload.METER_FULL)
	record.overload_left = maxf(_float(data, "overload_left", 0.0), 0.0)
	record.melted_for = maxf(_float(data, "melted_for", 0.0), 0.0)
	record.ping = maxi(_int(data, "ping", 0), 0)
	var carried: Variant = data.get("weapons")
	if carried is Array:
		record.weapons = carried.filter(func(id: Variant) -> bool: return Weapons.find(id) != null)
		if record.weapons.is_empty():
			record.weapons = [Weapons.STARTING]
	if not record.weapons.has(record.weapon):
		record.weapon = record.weapons[0]
	return record


static func _int(data: Dictionary, key: String, default: int) -> int:
	var value: Variant = data.get(key)
	return int(value) if value is int or value is float else default


static func _float(data: Dictionary, key: String, default: float) -> float:
	var value: Variant = data.get(key)
	return float(value) if (value is int or value is float) and is_finite(float(value)) else default


static func _bool(data: Dictionary, key: String, default: bool) -> bool:
	var value: Variant = data.get(key)
	return value if value is bool else default


static func _string(data: Dictionary, key: String, default: String) -> String:
	var value: Variant = data.get(key)
	return value if value is String else default
