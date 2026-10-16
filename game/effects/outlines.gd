class_name Outlines
## Marks round things a player must not lose sight of: a thin outline round
## teammates, which shows through walls, and a soft glow round pickups lying
## ready, however far off. Never enemies: an outline through a wall on
## someone to shoot at would be a cheat built into the game.
##
## An outline is the thing drawn again a little fatter in one flat colour,
## everywhere but over the thing itself. A glow is the same, fatter still
## and fading out from the thing; it grows with distance as the thing
## shrinks, so that it is as plain far off as close by.

## How far an outline stands out from what it is round, in metres.
const THICKNESS := 0.035
## How far a glow reaches out from what it is round, in metres, close by;
## and how much further for each metre away it is seen from.
const GLOW_REACH := 0.12
const GLOW_SPREAD := 0.004
## The colour of a pickup's glow.
const PICKUP_COLOUR := Color(1.0, 0.96, 0.8)
## The setting that turns outlines and glows off.
const SETTING := "outlines"

const _MARK := 1


## Whether the player with record `viewer` sees the one with record `other`
## outlined: a living teammate, in a mode with teams. Nobody when watching.
static func shows(viewer: PlayerRecord, other: PlayerRecord, teams: bool) -> bool:
	if viewer == null or other == null or viewer == other or not teams:
		return false
	if viewer.team == MatchState.NO_TEAM or viewer.team != other.team:
		return false
	return other.alive


## Whether a pickup glows: whenever it is lying ready, however far off.
static func pickup_shows(ready: bool) -> bool:
	return ready


## Outlines everything drawn under `root` in `colour`. One that shows
## `through_walls` is seen whatever is in front of it.
static func put(root: Node, colour: Color, through_walls := false) -> void:
	_cover(root, [colour, through_walls, false], _material(colour, through_walls))


## Makes everything drawn under `root` glow in `colour`, where it can be seen.
static func glow(root: Node, colour: Color) -> void:
	_cover(root, [colour, false, true], _glow(colour))


static func _cover(root: Node, how: Array, material: Material) -> void:
	if root.has_meta("outline") and root.get_meta("outline") == how:
		return
	root.set_meta("outline", how)
	for mesh in _meshes(root):
		mesh.material_overlay = material


## Takes the outline off everything under `root`.
static func take(root: Node) -> void:
	if not root.has_meta("outline"):
		return
	root.remove_meta("outline")
	for mesh in _meshes(root):
		mesh.material_overlay = null


## The colour of the outline on `root`, or null if it has none.
static func colour_on(root: Node) -> Variant:
	return root.get_meta("outline")[0] if root.has_meta("outline") else null


## Whether the outline on `root` shows through walls.
static func through_walls(root: Node) -> bool:
	return root.has_meta("outline") and root.get_meta("outline")[1]


## Whether `root` glows, rather than being outlined.
static func is_glow(root: Node) -> bool:
	return root.has_meta("outline") and root.get_meta("outline")[2]


## How far a glow reaches out from what it is round, in metres, seen from
## `distance` metres away, as the glow's shader works it out.
static func glow_reach(distance: float) -> float:
	return GLOW_REACH + GLOW_SPREAD * maxf(distance, 0.0)


static func _meshes(root: Node) -> Array[MeshInstance3D]:
	var found: Array[MeshInstance3D] = []
	if root is MeshInstance3D:
		found.append(root)
	for child in root.get_children():
		found.append_array(_meshes(child))
	return found


static var _made := {}


# The glow: the thing grown outward along its surface by glow_reach() in the
# world, whatever the thing's own scale, turned inside out so that only the
# far side of it is drawn, brightest next to the thing and fading to nothing
# at its edge, and added to what is behind it.
const _GLOW_SHADER := """
shader_type spatial;
render_mode unshaded, cull_front, blend_add, depth_draw_never, shadows_disabled, fog_disabled;
uniform vec4 colour : source_color;
uniform float reach;
uniform float spread;

void vertex() {
	float scale = max(length(MODEL_MATRIX[0].xyz), 0.0001);
	float far = max(-(MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).z, 0.0);
	VERTEX += NORMAL * (reach + spread * far) / scale;
}

void fragment() {
	// Looking straight through the shell is next to the thing; looking along
	// it is at the glow's edge.
	float near_thing = pow(clamp(abs(dot(NORMAL, VIEW)), 0.0, 1.0), 2.0);
	ALBEDO = colour.rgb * near_thing * colour.a;
	ALPHA = near_thing * colour.a;
}
"""
static var _glow_shader: Shader


static func _glow(colour: Color) -> ShaderMaterial:
	var key := [colour, "glow"]
	if not _made.has(key):
		if _glow_shader == null:
			_glow_shader = Shader.new()
			_glow_shader.code = _GLOW_SHADER
		var glowing := ShaderMaterial.new()
		glowing.shader = _glow_shader
		glowing.set_shader_parameter("colour", colour)
		glowing.set_shader_parameter("reach", GLOW_REACH)
		glowing.set_shader_parameter("spread", GLOW_SPREAD)
		_made[key] = glowing
	return _made[key]


# What draws an outline of `colour`. One that is hidden by what is in front
# of it is only the fatter copy, turned inside out: the thing itself covers
# all of it but the rim. One that shows through walls can't rely on that, so
# the thing first marks where it is, even where it is hidden, and the copy
# is drawn over everything except where the mark is.
static func _material(colour: Color, through: bool) -> StandardMaterial3D:
	var key := [colour, through]
	if _made.has(key):
		return _made[key]
	var hull := StandardMaterial3D.new()
	hull.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	hull.albedo_color = colour
	hull.cull_mode = BaseMaterial3D.CULL_FRONT
	hull.grow = true
	hull.grow_amount = THICKNESS
	if not through:
		_made[key] = hull
		return hull
	hull.no_depth_test = true
	hull.stencil_mode = BaseMaterial3D.STENCIL_MODE_CUSTOM
	hull.stencil_flags = BaseMaterial3D.STENCIL_FLAG_READ
	hull.stencil_compare = BaseMaterial3D.STENCIL_COMPARE_NOT_EQUAL
	hull.stencil_reference = _MARK
	hull.render_priority = 1
	# The mark: nothing to see, only where the thing is, hidden or not.
	var mark := StandardMaterial3D.new()
	mark.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mark.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mark.albedo_color = Color(0, 0, 0, 0)
	mark.stencil_mode = BaseMaterial3D.STENCIL_MODE_CUSTOM
	mark.stencil_flags = BaseMaterial3D.STENCIL_FLAG_WRITE | BaseMaterial3D.STENCIL_FLAG_WRITE_DEPTH_FAIL
	mark.stencil_compare = BaseMaterial3D.STENCIL_COMPARE_ALWAYS
	mark.stencil_reference = _MARK
	mark.next_pass = hull
	_made[key] = mark
	return mark
