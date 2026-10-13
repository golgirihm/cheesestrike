class_name WeaponSway
## How the weapon in a player's own view moves with them, so that it feels
## held rather than fixed to the screen: it lags a little behind a turn of
## the view, tilts into a sidestep, dips on landing, and drops to one side at
## a run. Each is small, and all but the dip nearly vanish when aiming.
##
## Offsets are in metres and radians in the head's own directions: X to the
## right, Y up, Z toward the player.

## The most the weapon trails behind a turn, and the turning speed (radians
## a second) that takes it there.
const LAG_REACH := 0.028
const LAG_FULL_AT := 6.0
## The most it rolls into a sidestep.
const STRAFE_ROLL := 0.07
## How far it dips after the hardest landing, and the fall speed (metres a
## second) that counts as one.
const LANDING_DIP := 0.07
const HARD_LANDING := 14.0
## Where it goes at a full run: down, across and turned in.
const RUN_OFFSET := Vector3(-0.05, -0.06, 0.02)
const RUN_TURN := Vector3(-0.25, 0.45, 0.2)


## Where the weapon trails to as the view turns at `rate` (to the right,
## upward): behind the turn, and no further than LAG_REACH.
static func turning(rate: Vector2) -> Vector3:
	var share := (rate / LAG_FULL_AT).limit_length(1.0)
	return Vector3(-share.x, -share.y, 0.0) * LAG_REACH


## How far the weapon rolls for a player moving sideways at `sideways` of
## their walking speed (to the right above zero): its top leans away from
## the way they go.
static func strafing(sideways: float) -> float:
	return -clampf(sideways, -1.0, 1.0) * STRAFE_ROLL


## How hard a landing at `fall_speed` metres a second is, from 0 to 1.
static func impact(fall_speed: float) -> float:
	return clampf(fall_speed / HARD_LANDING, 0.0, 1.0)


## How far the weapon dips for a landing of `hardness` (see impact) that is
## still being felt.
static func landing(hardness: float) -> Vector3:
	return Vector3.DOWN * LANDING_DIP * clampf(hardness, 0.0, 1.0)


## How far through a reload (`progress`, 0 to 1) the weapon is lowered: down
## as the reload begins, held there, and back up as it ends.
static func reload_lowering(progress: float) -> float:
	if progress <= 0.0 or progress >= 1.0:
		return 0.0
	return smoothstep(0.0, 0.18, progress) * (1.0 - smoothstep(0.82, 1.0, progress))


## How far out of the weapon its magazine is at `progress` through a reload:
## out by two fifths of the way, and back in by four fifths.
static func magazine_out(progress: float) -> float:
	if progress <= 0.0 or progress >= 1.0:
		return 0.0
	return smoothstep(0.2, 0.4, progress) * (1.0 - smoothstep(0.6, 0.8, progress))
