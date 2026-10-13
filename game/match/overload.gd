class_name Overload
## The Gouda Overload. Dealing damage fills a meter; once it is full, the player
## can set off an Overload: for a few seconds they move faster, and anyone they
## hit melts, slowed for a moment. It does no extra damage, so it helps a player
## who is doing well without deciding a match by itself.

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
	if record.overload_left > 0.0:
		factor = maxf(factor, SPEED_BOOST)
	if record.melted_for > 0.0:
		factor *= MELT_SLOW
	return factor


static func is_ready(record: PlayerRecord) -> bool:
	return record != null and record.alive and record.overload >= METER_FULL and record.overload_left <= 0.0
