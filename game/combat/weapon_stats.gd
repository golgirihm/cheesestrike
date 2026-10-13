class_name WeaponStats
extends Resource
## How one kind of weapon behaves. Each weapon is a resource in
## combat/weapons/, so it can be tuned without touching code.

## Short name used on the network and in settings, such as "blaster".
@export var id := ""
@export var display_name := ""
## Damage per hit, per pellet for a weapon with several.
@export var damage := 20.0
@export var headshot_multiplier := 1.75
## Seconds between shots.
@export var fire_interval := 0.12
## Whether holding the trigger keeps firing.
@export var automatic := true
@export var magazine := 24
## Rounds carried besides those in the magazine when the weapon is taken up,
## and the most that can be carried. Below zero, there is no end to them.
@export var reserve := -1
@export var reserve_max := -1
@export var reload_time := 1.6
@export var max_range := 150.0
## A hit does all of its damage out to `falloff_start` metres, then less,
## down to `falloff_floor` of it at `falloff_end` and beyond. No falloff
## while `falloff_end` is 0.
@export var falloff_start := 0.0
@export var falloff_end := 0.0
@export var falloff_floor := 0.5
## Rays per shot, spread over a cone `spread_degrees` wide when aiming down
## the sights, or `hip_spread_degrees` wide when fired from the hip.
@export var pellets := 1
@export var spread_degrees := 0.0
@export var hip_spread_degrees := 8.0
## The share of the player's field of view left when aiming down the sights.
@export var aim_zoom := 0.7
## From the hip, the first shots of a run of fire spread over only
## HIP_START of `hip_spread_degrees`, opening to all of it by shot
## `bloom_shots`.
@export var bloom_shots := 6

@export_group("Recoil")
## Degrees each shot of a run of fire climbs above the last, at first (see
## Recoil), and the most shots' worth it ever climbs.
@export var recoil_up := 0.5
@export var recoil_cap := 8.0
## Degrees it wanders from side to side.
@export var recoil_side := 0.3
## Seconds without a shot after which the next is the first of a new run.
@export var recoil_reset := 0.35

@export_group("Projectile")
## Weapons with a projectile speed fire a projectile instead of a hitscan shot,
## which bursts on touching anything, or when its fuse runs out.
@export var projectile_speed := 0.0
## How strongly gravity pulls the projectile, as a share of normal gravity.
@export var projectile_gravity := 0.0
@export var fuse := 3.0
## Damage at the middle of the burst, falling to nothing at its edge.
@export var splash_damage := 0.0
@export var splash_radius := 0.0
## How hard the burst shoves players, in metres per second at its middle.
@export var knockback := 0.0
## A projectile that bounces comes off whatever it touches with this share of
## its speed, passes players by, and bursts only when its fuse runs out.
@export var bounces := false
@export var bounce := 0.45


## How a player stands as they fire, as bits: what they claim, as the host
## can't see it. Each widens or narrows the spread.
const AIMED := 1
const MOVING := 2
const AIRBORNE := 4
const CROUCHED := 8
## The share of the hip spread that the first shot of a run of fire has.
const HIP_START := 0.55
## How much wider shots spread on the move, and off the ground.
const MOVING_SPREAD := 1.25
const AIRBORNE_SPREAD := 1.6
## How much tighter they are from a crouch.
const CROUCHED_SPREAD := 0.75
## How much wider aimed shots spread at the end of a long run of fire.
const AIMED_BLOOM := 1.5


## How wide a cone a shot's pellets spread over at the worst, standing still:
## in degrees, down the sights or from the hip.
func spread(aimed: bool) -> float:
	return spread_degrees if aimed else maxf(hip_spread_degrees, spread_degrees)


## How wide a cone shot number `run` of a run of fire (0 for the first)
## spreads over, in degrees, for a player standing as `stance` says. It opens
## as the run goes on, and is wider on the move and wider still in the air.
func spread_for(stance: int, run := 0) -> float:
	var heat := clampf(float(run) / maxi(bloom_shots, 1), 0.0, 1.0)
	var cone: float
	if stance & AIMED:
		cone = spread_degrees * lerpf(1.0, AIMED_BLOOM, heat)
	else:
		cone = maxf(spread(false) * lerpf(HIP_START, 1.0, heat), spread_degrees)
	if stance & AIRBORNE:
		cone *= AIRBORNE_SPREAD
	elif stance & MOVING:
		cone *= MOVING_SPREAD
	if stance & CROUCHED and not stance & AIRBORNE:
		cone *= CROUCHED_SPREAD
	return cone


## The damage a hit at `distance` metres does, before any headshot bonus.
func damage_at(distance: float) -> float:
	if falloff_end <= falloff_start:
		return damage
	var faded := clampf((distance - falloff_start) / (falloff_end - falloff_start), 0.0, 1.0)
	return damage * lerpf(1.0, falloff_floor, faded)


## Whether the weapon never runs out of rounds to reload with.
func unlimited() -> bool:
	return reserve < 0


func fires_projectile() -> bool:
	return projectile_speed > 0.0
