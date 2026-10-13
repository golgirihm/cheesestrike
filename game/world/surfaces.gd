class_name Surfaces
## What the things in an arena are made of, for what a shot does to them:
## the colour of the dust or crumbs it knocks off, and whether it strikes
## sparks.

const STONE := "stone"
const METAL := "metal"
const CHEESE := "cheese"
const WOOD := "wood"

## The colour of the dust off something whose colour can't be found.
const DUST := Color(0.6, 0.58, 0.55)


## What `body`, a solid thing in an arena, is made of.
static func kind(body: Node) -> String:
	var name := String(body.name) if body != null else ""
	if name.begins_with("Block"):
		return CHEESE
	if name.begins_with("Car") or name.begins_with("Bus"):
		return METAL
	if name.begins_with("Ramp"):
		return WOOD
	return STONE


## Whether feet leave prints on the floor of `map`: on earth and dust, not
## on paving.
static func takes_prints(map: Node) -> bool:
	return map != null and String(map.get_meta("scenery", "")) != Scenery.CITY


## Whether a shot strikes sparks off `body`.
static func sparks(body: Node) -> bool:
	return kind(body) == METAL


## The colour of what a shot knocks off `body`: its own colour, a little
## paler, as dust is.
static func dust(body: Node) -> Color:
	if body == null:
		return DUST
	# The body's own shape, rather than anything added to dress it.
	var drawn := body.get_node_or_null("MeshInstance3D") as MeshInstance3D
	if drawn == null:
		for child in body.get_children():
			if child is MeshInstance3D:
				drawn = child
				break
	var material := drawn.material_override as StandardMaterial3D if drawn != null else null
	# A wall of windows is a picture tinted by its colour: plain dust off that.
	if material != null and material.albedo_texture == null:
		return material.albedo_color.lerp(Color.WHITE, 0.25)
	return DUST
