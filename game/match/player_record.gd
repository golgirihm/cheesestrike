class_name PlayerRecord
extends RefCounted
## What the host knows about one player beyond where they are: their name,
## health and score. The host owns these and sends them to everyone (see
## MatchState).

const MAX_HEALTH := 100
## The most any rules give a player.
const MOST_HEALTH := 300
## The most armour a player can wear, and the share of each hit it takes in
## their place for as long as it lasts.
const MAX_ARMOUR := 50
const ARMOUR_SHARE := 0.5

var id: int
var name := PlayerName.FALLBACK
var health := MAX_HEALTH
## Armour left: a rind that takes part of each hit until it is gone.
var armour := 0
var alive := true
## Whether they are watching rather than playing (see Watching): not in the
## arena, and not coming back until they choose to.
var watching := false
## Whether their weapons never run out, as under rules that give everyone
## one weapon and nothing else to fall back on.
var endless := false
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
## Their loadout: the main weapon ("" for none) and sidearm they come into
## play with, and keep when knocked out.
var main := ""
var side := Weapons.STARTING
## What they have chosen to fit to their main weapon (see Attachments), and
## what is fitted now, to which weapon: a change waits, as one of weapon
## does, until they next come back into play.
var attachment := 0
var fitted := 0
var fitted_to := ""
## Their team in a team match, or MatchState.NO_TEAM.
var team := -1
## The Overload meter, filling to Overload.METER_FULL; seconds left in an
## Overload set off; and seconds left melted by someone else's.
var overload := 0.0
var overload_left := 0.0
## The kind of Overload they have chosen (see Overload), and seconds of
## build-up left before the one they set off begins.
var overload_kind := 0
var overload_build := 0.0
var melted_for := 0.0
## The power-up they have (see Powers; 0 for none) and seconds of it left.
var power := 0
var power_left := 0.0
## Round trip to the host, in milliseconds, as the host measures it.
var ping := 0
## Grenades left to throw.
var grenades := Weapons.GRENADES
## What their character looks like, as they chose it (see Looks).
var look := 0
## What tells this player apart from one session to the next, so that a
## player can block them for good: made by the host from the key their
## game keeps (see Match.tag_of), and telling nothing of the key itself.
var tag := ""

# Where each number sits in a packed record (see to_packed).
enum { _ID, _HEALTH, _FLAGS, _RESPAWN, _PROTECTED, _KILLS, _DEATHS, _LAST_ATTACKER,
	_WEAPON, _WEAPONS, _TEAM, _OVERLOAD, _OVERLOAD_LEFT, _MELTED, _PING, _GRENADES, _LOOK, _ARMOUR, _MAIN, _SIDE, _POWER, _FITTED, _COUNT }
const _ALIVE := 1


func _init(player_id := 0) -> void:
	id = player_id


## Back to only their loadout, with the main weapon in hand, as after being
## knocked out.
func disarm() -> void:
	weapons = [side]
	weapon = side
	fitted = attachment if not main.is_empty() else Attachments.NONE
	fitted_to = main
	if not main.is_empty():
		weapons.append(main)
		weapon = main


## Gives them power-up `kind` for `seconds`, or takes away what they have
## if it is none, or over.
func give_power(kind: int, seconds: float) -> void:
	var real := Powers.is_kind(kind) and seconds > 0.0 and is_finite(seconds)
	power = kind if real else Powers.NONE
	power_left = minf(seconds, 60.0) if real else 0.0


## The weapon `id` as this player has it: with their attachment, if it is
## the one that is fitted to. Null if there is no such weapon to carry.
func stats(id: String) -> WeaponStats:
	if not Weapons.ids().has(id):
		return null
	var base := Weapons.endless(Weapons.find(id)) if endless else Weapons.find(id)
	return Attachments.fitted(base, fitted if id == fitted_to else Attachments.NONE)


## Whether they carry exactly their loadout: nothing found, nothing lost.
func has_only_loadout() -> bool:
	return weapons.size() == (1 if main.is_empty() else 2) and weapons.has(side) and (main.is_empty() or weapons.has(main))


## Takes a hit of `amount`: armour absorbs its share for as long as it
## lasts, and health the rest. Returns {"dealt": health lost, "absorbed":
## armour lost}.
func take(amount: int) -> Dictionary:
	var absorbed := mini(armour, roundi(amount * ARMOUR_SHARE))
	var dealt := mini(amount - absorbed, health)
	armour -= absorbed
	health -= dealt
	return {"dealt": dealt, "absorbed": absorbed}


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
		"grenades": grenades,
		"look": look,
		"tag": tag,
		"armour": armour,
		"main": main,
		"attachment": attachment,
		"watching": watching,
		"endless": endless,
		"overload_kind": overload_kind,
		"overload_build": overload_build,
		"fitted": fitted,
		"fitted_to": fitted_to,
		"power": power,
		"power_left": power_left,
		"side": side,
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
	numbers[_GRENADES] = grenades
	numbers[_LOOK] = look
	numbers[_ARMOUR] = armour
	# Kept small, as this is sent often: the power-up in the low four bits
	# with its hundredths of a second above them.
	numbers[_POWER] = power | (roundi(power_left * 100.0) << 4)
	numbers[_MAIN] = Weapons.ORDER.find(main)
	# Likewise: what is chosen, what is fitted, and the weapon it is on.
	# And above those, their kind of Overload and hundredths of its build-up.
	numbers[_FITTED] = attachment | (fitted << 4) | ((Weapons.ORDER.find(fitted_to) + 1) << 8) | (overload_kind << 16) | (roundi(overload_build * 100.0) << 20) | ((1 if watching else 0) << 28) | ((1 if endless else 0) << 29)
	numbers[_SIDE] = Weapons.ORDER.find(side)
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
	record.health = clampi(numbers[_HEALTH], 0, MOST_HEALTH)
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
	record.grenades = clampi(numbers[_GRENADES], 0, Weapons.GRENADES)
	record.look = Looks.clean(numbers[_LOOK])
	record.armour = clampi(numbers[_ARMOUR], 0, MAX_ARMOUR)
	record.give_power(numbers[_POWER] & 15, maxi(numbers[_POWER] >> 4, 0) / 100.0)
	var fit := maxi(numbers[_FITTED], 0)
	var chosen := Weapons.clean_loadout(_weapon_at(numbers[_MAIN]), _weapon_at(numbers[_SIDE]), fit & 15)
	record.main = chosen[0]
	record.side = chosen[1]
	record.attachment = chosen[2]
	record.fitted = (fit >> 4) & 15 if Attachments.is_kind((fit >> 4) & 15) else Attachments.NONE
	record.fitted_to = _weapon_at(((fit >> 8) & 255) - 1)
	record.overload_kind = (fit >> 16) & 15 if Overload.is_kind((fit >> 16) & 15) else Overload.MELT
	record.overload_build = clampf(((fit >> 20) & 255) / 100.0, 0.0, Overload.BUILD_UP)
	record.watching = (fit >> 28) & 1 == 1
	record.endless = (fit >> 29) & 1 == 1
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
	record.grenades = clampi(_int(data, "grenades", Weapons.GRENADES), 0, Weapons.GRENADES)
	record.look = Looks.clean(_int(data, "look", 0))
	record.tag = Match.clean_tag(_string(data, "tag", ""))
	record.armour = clampi(_int(data, "armour", 0), 0, MAX_ARMOUR)
	record.give_power(_int(data, "power", 0), maxf(_float(data, "power_left", 0.0), 0.0))
	var chosen := Weapons.clean_loadout(data.get("main"), data.get("side"), data.get("attachment"))
	record.main = chosen[0]
	record.side = chosen[1]
	record.attachment = chosen[2]
	record.fitted = data.get("fitted") if Attachments.is_kind(data.get("fitted")) else Attachments.NONE
	record.overload_kind = data.get("overload_kind") if Overload.is_kind(data.get("overload_kind")) else Overload.MELT
	record.overload_build = clampf(_float(data, "overload_build", 0.0), 0.0, Overload.BUILD_UP)
	record.watching = data.get("watching") == true
	record.endless = data.get("endless") == true
	record.fitted_to = data.get("fitted_to") if data.get("fitted_to") is String and Weapons.ids().has(data.get("fitted_to")) else ""
	var carried: Variant = data.get("weapons")
	if carried is Array:
		record.weapons = carried.filter(func(id: Variant) -> bool: return id is String and Weapons.ids().has(id))
		if record.weapons.is_empty():
			record.weapons = [Weapons.STARTING]
	if not record.weapons.has(record.weapon):
		record.weapon = record.weapons[0]
	return record


# The weapon at `index` in the usual order, or "".
static func _weapon_at(index: int) -> String:
	return Weapons.ORDER[index] if index >= 0 and index < Weapons.ORDER.size() else ""


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
