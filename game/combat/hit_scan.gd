class_name HitScan
## Where a shot goes and whom it hits, worked out from positions alone, without
## the physics engine, so that the host can test a shot against where players
## are (or, later, where they were) and so it can be tested on its own.
##
## A player's body is the wedge of their cheese character (body_shape.gd),
## standing on their position and facing where they face.


## How far along the ray from `origin` in direction `direction` (normalized) it
## first touches an upright capsule standing at `feet`, or INF if it misses. A
## ray that starts inside the capsule doesn't hit it.
static func ray_capsule(origin: Vector3, direction: Vector3, feet: Vector3, radius: float, height: float) -> float:
	var local := origin - feet
	var bottom := radius
	var top := height - radius
	var nearest := INF

	# The straight middle part: an upright cylinder, tested across the floor plane.
	var a := direction.x * direction.x + direction.z * direction.z
	var b := 2.0 * (local.x * direction.x + local.z * direction.z)
	var c := local.x * local.x + local.z * local.z - radius * radius
	if a > 1e-9:
		var discriminant := b * b - 4.0 * a * c
		if discriminant >= 0.0:
			var t := (-b - sqrt(discriminant)) / (2.0 * a)
			var y := local.y + t * direction.y
			if t >= 0.0 and y >= bottom and y <= top:
				nearest = t

	# The rounded ends.
	for centre_y: float in [bottom, top]:
		var t := ray_sphere(local - Vector3(0.0, centre_y, 0.0), direction, radius)
		if t < nearest:
			var y := local.y + t * direction.y
			# Only the outer half of each end sphere is part of the capsule.
			if (centre_y == bottom and y <= bottom) or (centre_y == top and y >= top):
				nearest = t
	return nearest


## A player standing at `feet`, turned by `yaw` and looking up or down by
## `pitch`, as `targets` in first_hit() hold them. Looking up or down doesn't
## move the wedge, so only where they stand, how they are turned and how
## far they `crouch` (0 standing to 1 crouched) and `lean` (-1 left to 1
## right) decide what hits.
static func pose(feet: Vector3, yaw := 0.0, pitch := 0.0, crouch := 0.0, lean := 0.0) -> Dictionary:
	return {"feet": feet, "yaw": yaw, "pitch": pitch, "crouch": crouch, "lean": lean}


## The first of `targets` that the ray hits within `max_distance`, as {id,
## distance, point, head}, or an empty dictionary. `targets` maps player ids to
## a pose(), or to just their feet for a player facing -Z and looking level.
static func first_hit(origin: Vector3, direction: Vector3, max_distance: float, targets: Dictionary) -> Dictionary:
	var best := {}
	var best_distance := max_distance
	for id: int in targets:
		var target: Variant = targets[id]
		var at: Dictionary = target if target is Dictionary else pose(target)
		var hit := BodyShape.ray_hit(origin, direction, at.feet, at.yaw, at.get("crouch", 0.0), at.get("lean", 0.0))
		if not hit.is_empty() and hit.distance <= best_distance:
			best_distance = hit.distance
			best = {"id": id, "distance": hit.distance, "point": origin + direction * hit.distance, "head": hit.head}
	return best


## The direction of each pellet of a shot aimed along `direction`, spread evenly
## at random over a cone `spread_degrees` wide. The same `seed` always gives the
## same directions, so the shooter and the host agree on where pellets went.
static func pellet_directions(direction: Vector3, pellets: int, spread_degrees: float, seed: int) -> Array[Vector3]:
	var result: Array[Vector3] = []
	direction = direction.normalized()
	if pellets <= 1 and spread_degrees <= 0.0:
		result.append(direction)
		return result
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var half_angle := deg_to_rad(spread_degrees) / 2.0
	# Any two directions at right angles to the aim and to each other.
	var side := direction.cross(Vector3.UP if absf(direction.y) < 0.99 else Vector3.RIGHT).normalized()
	var up := side.cross(direction).normalized()
	for i in maxi(pellets, 1):
		# Uniform over the cone's opening, not bunched at its centre.
		var off := half_angle * sqrt(rng.randf())
		var around := rng.randf() * TAU
		var tilt := (side * cos(around) + up * sin(around)) * tan(off)
		result.append((direction + tilt).normalized())
	return result


## How far along the ray from `local`, measured from a sphere's centre, it first
## touches the sphere, or INF. A ray that starts inside doesn't hit it.
static func ray_sphere(local: Vector3, direction: Vector3, radius: float) -> float:
	var b := local.dot(direction)
	var c := local.dot(local) - radius * radius
	if c <= 0.0:
		return INF  # Starts inside.
	var discriminant := b * b - c
	if discriminant < 0.0:
		return INF
	var t := -b - sqrt(discriminant)
	return t if t >= 0.0 else INF
