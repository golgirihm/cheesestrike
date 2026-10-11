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


func gravity() -> float:
	return ProjectSettings.get_setting("physics/3d/default_gravity", 9.8) * weapon.projectile_gravity


## Moves on by `delta` seconds. `wall_distance` is a Callable(from, to) giving
## how far along the segment the arena stops it (INF if not at all), and
## `targets` maps player ids to their feet. Returns where it burst, or null if
## it is still flying.
func advance(delta: float, wall_distance: Callable, targets: Dictionary) -> Variant:
	age += delta
	var next := position + velocity * delta + Vector3.DOWN * gravity() * 0.5 * delta * delta
	velocity += Vector3.DOWN * gravity() * delta
	var step := next - position
	var length := step.length()
	if length > 0.0:
		var direction := step / length
		var reach := minf(wall_distance.call(position, next), length)
		var hit := HitScan.first_hit(position, direction, reach, targets)
		if not hit.is_empty():
			position = hit.point
			return position
		if reach < length:
			position += direction * reach
			return position
	position = next
	if age >= weapon.fuse:
		return position
	return null
