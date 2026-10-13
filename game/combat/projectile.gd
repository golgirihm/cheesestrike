class_name Projectile
extends RefCounted
## A projectile in flight, such as a Gouda Launcher wheel. The host moves it
## and decides where it bursts; everyone else draws it from where and how it
## was launched, as its flight is the same everywhere.

var id: int
var shooter: int
var weapon: WeaponStats
var position: Vector3
var velocity: Vector3
var age := 0.0
## Times it has bounced, and whether it has come to rest on the ground.
var bounces := 0
var resting := false
## The player it struck (0 for none), and whether on the head.
var struck := 0
var struck_head := false


func _init(projectile_id: int, from: int, stats: WeaponStats, at: Vector3, direction: Vector3) -> void:
	id = projectile_id
	shooter = from
	weapon = stats
	position = at
	velocity = direction.normalized() * stats.projectile_speed


## Where a projectile launched from `at` with `launch_velocity` is after `time`
## seconds, in the air.
static func position_at(at: Vector3, launch_velocity: Vector3, gravity: float, time: float) -> Vector3:
	return at + launch_velocity * time + Vector3.DOWN * gravity * 0.5 * time * time


## How hard gravity pulls in the match being played, as a share of the usual
## (see MatchSettings.gravity). The same on every machine in a session.
static var gravity_scale := 1.0


func gravity() -> float:
	return ProjectSettings.get_setting("physics/3d/default_gravity", 9.8) * weapon.projectile_gravity * gravity_scale


## Slower than this, a bouncing projectile that lands stays where it is.
const REST_SPEED := 1.2


## Moves on by `delta` seconds. `wall` is a Callable(from, to) telling where
## along the segment the arena stops it: how far (INF if not at all), or
## {distance, normal} with the way the surface faces, which a bouncing
## projectile comes off. `targets` maps player ids to their feet. Returns
## where it burst, or null if it is still going.
##
## A projectile that doesn't bounce bursts on touching the arena or a player.
## One that does comes off the arena, passes players by, and bursts when its
## fuse runs out.
func advance(delta: float, wall: Callable, targets: Dictionary) -> Variant:
	age += delta
	if resting:
		return position if age >= weapon.fuse else null
	var next := position + velocity * delta + Vector3.DOWN * gravity() * 0.5 * delta * delta
	velocity += Vector3.DOWN * gravity() * delta
	var step := next - position
	var length := step.length()
	if length > 0.0:
		var direction := step / length
		var met: Variant = wall.call(position, next)
		var reach := minf(met.distance if met is Dictionary else met, length)
		if not weapon.bounces:
			var hit := HitScan.first_hit(position, direction, reach, targets)
			if not hit.is_empty():
				position = hit.point
				struck = hit.id
				struck_head = hit.head
				return position
		if reach < length:
			position += direction * reach
			if not weapon.bounces:
				return position
			var normal: Vector3 = met.normal if met is Dictionary else -direction
			velocity = velocity.bounce(normal) * weapon.bounce
			# Clear of the surface, so the next step doesn't start inside it.
			position += normal * 0.03
			bounces += 1
			if normal.y > 0.7 and velocity.length() < REST_SPEED:
				velocity = Vector3.ZERO
				resting = true
			return position if age >= weapon.fuse else null
	position = next
	if age >= weapon.fuse:
		return position
	return null
