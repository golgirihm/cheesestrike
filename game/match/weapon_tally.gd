class_name WeaponTally
extends RefCounted
## What each weapon did in a match, as the host saw it: how often it was
## fired, how often it hit, the damage it did and the players it knocked
## out. Shown to the host when a match ends, for telling which weapons are
## too strong or too weak.

# Weapon id -> {"shots", "hits", "damage", "knock_outs"}.
var _by_weapon := {}


func _entry(weapon: String) -> Dictionary:
	if not _by_weapon.has(weapon):
		_by_weapon[weapon] = {"shots": 0, "hits": 0, "damage": 0, "knock_outs": 0}
	return _by_weapon[weapon]


## `weapon` was fired once (one pull of the trigger, however many pellets).
func shot(weapon: String) -> void:
	_entry(weapon).shots += 1


## `weapon` hurt a player for `dealt`, knocking them out or not.
func hit(weapon: String, dealt: int, knocked_out: bool) -> void:
	var entry := _entry(weapon)
	entry.hits += 1
	entry.damage += dealt
	if knocked_out:
		entry.knock_outs += 1


func clear() -> void:
	_by_weapon.clear()


## What `weapon` has done, as {shots, hits, damage, knock_outs}.
func of(weapon: String) -> Dictionary:
	return _entry(weapon).duplicate()


## Hits for each time `weapon` was fired. More than 1 for a weapon whose
## shots can hit several players, or several times.
func accuracy(weapon: String) -> float:
	var entry := _entry(weapon)
	return float(entry.hits) / entry.shots if entry.shots > 0 else 0.0


## A line for each weapon that was used, in the order they were first
## fired, saying how well: shots, the share that hit, and the damage done.
func practice_lines() -> Array[String]:
	var text: Array[String] = []
	for weapon: String in _by_weapon:
		var entry: Dictionary = _by_weapon[weapon]
		if entry.shots <= 0:
			continue
		var stats := Weapons.find(weapon)
		var name := stats.display_name if stats != null else (Melee.NAME if weapon == Melee.ID else weapon)
		var each: float = float(entry.damage) / entry.hits if entry.hits > 0 else 0.0
		# Rounded here, as formatting rounds a half differently from one machine to another.
		text.append("%s: %d fired · %d%% hit · %d damage (%d a hit)" % [name, entry.shots, roundi(minf(accuracy(weapon), 1.0) * 100.0), entry.damage, roundi(each)])
	return text


## A line for each weapon that was used, the one that did most damage first.
func lines() -> Array[String]:
	var used := _by_weapon.keys().filter(func(weapon: String) -> bool:
		return _by_weapon[weapon].shots > 0 or _by_weapon[weapon].hits > 0)
	used.sort_custom(func(a: String, b: String) -> bool: return _by_weapon[a].damage > _by_weapon[b].damage)
	var text: Array[String] = []
	for weapon: String in used:
		var entry: Dictionary = _by_weapon[weapon]
		var stats := Weapons.find(weapon)
		var name := stats.display_name if stats != null else (Melee.NAME if weapon == Melee.ID else weapon)
		text.append("%s: %d fired, %d hit, %d damage, %d knocked out" % [name, entry.shots, entry.hits, entry.damage, entry.knock_outs])
	return text
