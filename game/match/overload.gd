class_name Overload
## The Gouda Overload. Dealing damage fills a meter; once it is full, the player
## can set off an Overload, of the kind they chose beforehand:
##
##   MELT    for a few seconds they move faster, and anyone they hit melts,
##           slowed for a moment
##   SHIELD  for a few seconds, hits on them do half
##   RAIN    wheels of Gouda fall around them and burst
##
## Each begins with a build-up, when the player glows and is heard but
## nothing has happened yet, so that others can get out of the way. None
## adds to the damage of the player's own weapons, so an Overload helps a
## player who is doing well without deciding a match by itself.

const MELT := 0
const SHIELD := 1
const RAIN := 2
const KINDS := [MELT, SHIELD, RAIN]
## What each is called in the settings, and on screen.
const IDS := ["melt", "shield", "rain"]
const NAMES := {MELT: "Melt", SHIELD: "Rind", RAIN: "Rain of wheels"}
const NOTES := {MELT: "faster, and your hits slow others", SHIELD: "hits on you do half", RAIN: "wheels fall and burst around you"}
## Seconds of build-up before any of them does anything.
const BUILD_UP := 1.0
## Seconds each lasts once it has begun.
const DURATIONS := {MELT: 8.0, SHIELD: 8.0, RAIN: 3.0}
const SHIELD_FACTOR := 0.5
## A rain: how many wheels, seconds between them, how far up they start, and
## how near and far from the player they come down.
const RAIN_WHEELS := 8
const RAIN_INTERVAL := 0.3
const RAIN_HEIGHT := 14.0
const RAIN_NEAR := 5.0
const RAIN_FAR := 9.0
const RAIN_WEAPON := "overload_wheel"

const METER_FULL := 100.0
## Meter gained per point of damage dealt: two knock-outs' worth fills it.
const CHARGE_PER_DAMAGE := 0.5
const DURATION := 8.0
const SPEED_BOOST := 1.35
const MELT_DURATION := 2.0
const MELT_SLOW := 0.6
## Share of the meter a player keeps on being knocked out.
const KEPT_ON_KNOCK_OUT := 0.5


## How fast `record`'s owner moves, as a share of normal speed.
static func speed_factor(record: PlayerRecord) -> float:
	if record == null:
		return 1.0
	# An Overload and a speed power-up don't add up: the faster of the two.
	var factor := Powers.speed_factor(record)
	if is_active(record, MELT):
		factor = maxf(factor, SPEED_BOOST)
	if record.melted_for > 0.0:
		factor *= MELT_SLOW
	return factor


static func is_ready(record: PlayerRecord) -> bool:
	return record != null and record.alive and record.overload >= METER_FULL and record.overload_left <= 0.0


static func is_kind(kind: Variant) -> bool:
	return kind is int and KINDS.has(kind)


## The Overload called `id` in the settings, or the usual.
static func from_id(id: Variant) -> int:
	return maxi(IDS.find(id), 0) if id is String else MELT


## What the settings say of Overload `kind`.
static func text(kind: int) -> String:
	return "%s (%s)" % [NAMES[kind], NOTES[kind]]


## Whether `record`'s owner has set one off and it is still building up.
static func is_building(record: PlayerRecord) -> bool:
	return record != null and record.overload_left > 0.0 and record.overload_build > 0.0


## Whether `record`'s owner is in an Overload that has begun: of `kind`, if
## that is given.
static func is_active(record: PlayerRecord, kind := -1) -> bool:
	return record != null and record.overload_left > 0.0 and record.overload_build <= 0.0 and (kind == -1 or record.overload_kind == kind)


## How much of each hit on `record`'s owner gets through.
static func incoming(record: PlayerRecord) -> float:
	return SHIELD_FACTOR if is_active(record, SHIELD) else 1.0


## Where wheel `index` of a rain round a player at `feet` starts, high above
## the ground: each a turn further round than the last and nearer or
## further out, so they fall all about and never on the player.
static func rain_start(feet: Vector3, index: int) -> Vector3:
	var angle := index * 2.4
	var out := lerpf(RAIN_NEAR, RAIN_FAR, fmod(index * 0.37, 1.0))
	return feet + Vector3(cos(angle) * out, RAIN_HEIGHT, sin(angle) * out)
