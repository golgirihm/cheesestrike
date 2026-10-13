class_name Pickups
## What lies on the arena's pickup pads, and the rules for collecting it: a
## cheese wedge for health, or a weapon. The pads themselves are part of the
## arena; which are full is part of the match state, decided by the host.

const HEALTH := "health"
## A box of ammunition: a magazine's worth for every weapon carried.
const AMMO := "ammo"
## A rind to wear: armour, up to the most that can be worn.
const ARMOUR := "armour"
const ARMOUR_AMOUNT := 50
## A power-up (see Powers): which one, the taker finds out.
const POWER := "power"
## Seconds into a match before the first power-up is there.
const POWER_FIRST := 30.0
## Health a wedge gives back, up to full.
const HEALTH_AMOUNT := 50
## Seconds before a collected pickup is back.
const RESPAWN := {HEALTH: 15.0, AMMO: 12.0, ARMOUR: 25.0, POWER: 60.0}
const WEAPON_RESPAWN := 20.0
## Seconds a weapon dropped by a knocked-out player lies there, and the
## most that lie about at once: the oldest goes first.
const DROP_LIFE := 20.0
const MAX_DROPS := 12
## How close a player's feet must come to a pad to collect from it.
const REACH := 1.3
const REACH_UP := 2.0


## Whether `kind` is something a pad can hold.
static func is_kind(kind: String) -> bool:
	return kind == HEALTH or kind == AMMO or kind == ARMOUR or kind == POWER or Weapons.ids().has(kind)


static func respawn_time(kind: String) -> float:
	return RESPAWN.get(kind, WEAPON_RESPAWN)


## Whether a player standing at `feet` is close enough to collect from a pad at `pad`.
static func within_reach(pad: Vector3, feet: Vector3) -> bool:
	var across := Vector2(feet.x - pad.x, feet.z - pad.z).length()
	return across <= REACH and feet.y - pad.y >= -0.5 and feet.y - pad.y <= REACH_UP


## Whether `record` would gain anything from a pickup of `kind`: health only
## for someone hurt, a weapon always (more ammunition, if already carried).
static func wanted(kind: String, record: PlayerRecord) -> bool:
	if record == null or not record.alive:
		return false
	if kind == HEALTH:
		return record.health < PlayerRecord.MAX_HEALTH
	if kind == ARMOUR:
		return record.armour < PlayerRecord.MAX_ARMOUR
	if kind == POWER:
		return true
	if kind == AMMO:
		# Of use to anyone short of a grenade, or carrying more than the
		# sidearm, which never runs out.
		return record.grenades < Weapons.GRENADES or record.weapons.any(func(id: String) -> bool: return not Weapons.find(id).unlimited())
	return Weapons.find(kind) != null


## Gives `record` a pickup of `kind`. Returns true if it was a weapon they
## already carried, which only refills it.
static func give(kind: String, record: PlayerRecord) -> bool:
	if kind == HEALTH:
		record.health = mini(record.health + HEALTH_AMOUNT, PlayerRecord.MAX_HEALTH)
		return false
	if kind == ARMOUR:
		record.armour = mini(record.armour + ARMOUR_AMOUNT, PlayerRecord.MAX_ARMOUR)
		return false
	if kind == AMMO:
		record.grenades = mini(record.grenades + 1, Weapons.GRENADES)
		return false
	if record.weapons.has(kind):
		return true
	record.weapons.append(kind)
	return false


static func display_name(kind: String) -> String:
	if kind == HEALTH:
		return "Cheese wedge"
	if kind == AMMO:
		return "Ammunition"
	if kind == ARMOUR:
		return "Rind armour"
	if kind == POWER:
		return "Power-up"
	var weapon := Weapons.find(kind)
	return weapon.display_name if weapon != null else kind
