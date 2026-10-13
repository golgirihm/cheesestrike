class_name Powers
## Power-ups: rare pickups, announced to everyone, that change the fight for
## whoever takes one, for a few seconds. One pad on each arena holds them, on
## a long timer; what it gives goes round in turn, so nobody knows it for
## theirs until they have it.

const NONE := 0
## Hits do twice the damage.
const DAMAGE := 1
## Faster on their feet.
const SPEED := 2
## Hits on them do less than half.
const SHIELD := 3
## Everyone else is seen through walls.
const SIGHT := 4
## In the order they come round.
const KINDS := [DAMAGE, SPEED, SHIELD, SIGHT]
const NAMES := {DAMAGE: "Double damage", SPEED: "Speed", SHIELD: "Shield", SIGHT: "Sight"}
## Seconds each lasts.
const DURATION := {DAMAGE: 12.0, SPEED: 15.0, SHIELD: 12.0, SIGHT: 10.0}
## The colour each is shown in, on its holder and in what is written.
const COLOURS := {DAMAGE: Color(0.95, 0.25, 0.2), SPEED: Color(0.3, 0.75, 1.0), SHIELD: Color(0.45, 0.9, 0.5), SIGHT: Color(0.8, 0.5, 1.0)}
const DAMAGE_FACTOR := 2.0
const SPEED_FACTOR := 1.3
const SHIELD_FACTOR := 0.4


static func is_kind(kind: int) -> bool:
	return KINDS.has(kind)


## What the pad gives after `taken` have been taken from it.
static func next(taken: int) -> int:
	return KINDS[posmod(taken, KINDS.size())]


## Whether `record`'s owner has power-up `kind` just now.
static func has(record: PlayerRecord, kind: int) -> bool:
	return record != null and record.alive and record.power == kind and record.power_left > 0.0


## How much more the hits of `record`'s owner do, and how much less the hits
## on them.
static func outgoing(record: PlayerRecord) -> float:
	return DAMAGE_FACTOR if has(record, DAMAGE) else 1.0


static func incoming(record: PlayerRecord) -> float:
	return SHIELD_FACTOR if has(record, SHIELD) else 1.0


static func speed_factor(record: PlayerRecord) -> float:
	return SPEED_FACTOR if has(record, SPEED) else 1.0


## What a player is shown of their own power-up, such as "Shield 9", or "".
static func text(record: PlayerRecord) -> String:
	if record == null or not is_kind(record.power) or record.power_left <= 0.0 or not record.alive:
		return ""
	return "%s %d" % [NAMES[record.power], ceili(record.power_left)]


## What everyone is told when `name` takes power-up `kind`.
static func announcement(name: String, kind: int) -> String:
	return "%s has %s!" % [name, String(NAMES[kind]).to_upper()] if is_kind(kind) else ""
