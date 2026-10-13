class_name Detail
## Less detail far away. An arena is dressed with many small things: the
## holes in a block of cheese, the windows of a tower, the posts of a fence,
## the leaves of a far tree. From across the arena each is a few pixels, and
## drawing it costs as much as drawing it close up. So each stops being drawn
## beyond a distance that depends on how big it is, and sooner on lower
## graphics.
##
## Only dressing is thinned: never anything a player stands on, hides behind,
## picks up or shoots at, and nothing big enough to be missed.

## Metres at which a thing is last drawn, for each metre of its size.
const REACH_PER_METRE := {GraphicsQuality.LOW: 35.0, GraphicsQuality.MEDIUM: 90.0, GraphicsQuality.HIGH: 160.0}
## Nothing is hidden nearer than this, whatever its size.
const NEAREST := 22.0
## Things at least this big, in metres, are always drawn.
const ALWAYS_FROM := 3.0


## How far away a thing `size` metres across is last drawn on graphics
## `level`, or 0 for one that is always drawn.
static func reach(size: float, level: String) -> float:
	if size >= ALWAYS_FROM or size <= 0.0:
		return 0.0
	return maxf(size * REACH_PER_METRE.get(level, REACH_PER_METRE[GraphicsQuality.HIGH]), NEAREST)


## Thins the dressing of the arena `map` for graphics `level`: what stands
## on its solid things, and the scenery round it. Returns how many things
## were given a distance.
static func thin(map: Node3D, level: String) -> int:
	var thinned := 0
	var geometry := map.get_node_or_null("Geometry")
	if geometry != null:
		for body in geometry.get_children():
			for part in body.get_children():
				# A solid thing's own shape stays: only what is put on it goes.
				if part.name != "MeshInstance3D":
					thinned += _thin(part, level)
	for holder: String in ["Scenery", "AmbientLife"]:
		var dressing := map.get_node_or_null(holder)
		if dressing != null:
			thinned += _thin(dressing, level)
	return thinned


## How many of the things drawn under `root` would be drawn from `eye`:
## those near enough for their size, or with no distance set. With
## `within`, only those no further off than that are counted at all.
static func drawn_from(root: Node, eye: Vector3, within := INF) -> int:
	var count := 0
	if root is GeometryInstance3D and root.is_visible_in_tree():
		var distance: float = root.global_position.distance_to(eye)
		if distance <= within and (root.visibility_range_end <= 0.0 or distance <= root.visibility_range_end):
			count += 1
	for child in root.get_children():
		count += drawn_from(child, eye, within)
	return count


## How big the thing `instance` draws is, in metres: the longest side of the
## box round it.
static func size_of(instance: GeometryInstance3D) -> float:
	var box := instance.get_aabb()
	var scale := instance.global_basis.get_scale() if instance.is_inside_tree() else instance.basis.get_scale()
	var sides := box.size * scale.abs()
	return maxf(sides.x, maxf(sides.y, sides.z))


static func _thin(root: Node, level: String) -> int:
	var thinned := 0
	# A light is no bigger than its bulb but is seen from anywhere.
	if root is GeometryInstance3D and not root.has_meta("always_drawn"):
		root.visibility_range_end = reach(size_of(root), level)
		thinned += 1 if root.visibility_range_end > 0.0 else 0
	for child in root.get_children():
		thinned += _thin(child, level)
	return thinned
