class_name Outlines
## Thin outlines round things a player must not lose sight of: teammates,
## whose outline shows through walls, and pickups close by. Never enemies:
## an outline through a wall on someone to shoot at would be a cheat built
## into the game.
##
## An outline is the thing drawn again a little fatter in one flat colour,
## everywhere but over the thing itself. Each of its meshes first marks
## where it is on the screen, then the fatter copy is drawn where no mark
## is.

## How far an outline stands out from what it is round, in metres.
const THICKNESS := 0.035
## How close a pickup must be to be outlined, and the colour of its outline.
const PICKUP_REACH := 16.0
const PICKUP_COLOUR := Color(1.0, 1.0, 1.0)
## The setting that turns outlines off.
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


## Whether a pickup `distance` metres away, lying ready, is outlined.
static func pickup_shows(distance: float, ready: bool) -> bool:
	return ready and distance <= PICKUP_REACH


## Outlines everything drawn under `root` in `colour`. One that shows
## `through_walls` is seen whatever is in front of it.
static func put(root: Node, colour: Color, through_walls := false) -> void:
	if root.has_meta("outline") and root.get_meta("outline") == [colour, through_walls]:
		return
	root.set_meta("outline", [colour, through_walls])
	var material := _material(colour, through_walls)
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


static func _meshes(root: Node) -> Array[MeshInstance3D]:
	var found: Array[MeshInstance3D] = []
	if root is MeshInstance3D:
		found.append(root)
	for child in root.get_children():
		found.append_array(_meshes(child))
	return found


static var _made := {}


# The mark and the fatter copy, for an outline of `colour`.
static func _material(colour: Color, through: bool) -> StandardMaterial3D:
	var key := [colour, through]
	if _made.has(key):
		return _made[key]
	# The mark: nothing to see, only where the thing is. One that shows
	# through walls marks where the thing would be if nothing hid it.
	var mark := StandardMaterial3D.new()
	mark.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mark.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mark.albedo_color = Color(0, 0, 0, 0)
	mark.stencil_mode = BaseMaterial3D.STENCIL_MODE_CUSTOM
	mark.stencil_flags = BaseMaterial3D.STENCIL_FLAG_WRITE | (BaseMaterial3D.STENCIL_FLAG_WRITE_DEPTH_FAIL if through else 0)
	mark.stencil_compare = BaseMaterial3D.STENCIL_COMPARE_ALWAYS
	mark.stencil_reference = _MARK
	# The fatter copy, drawn after every mark.
	var hull := StandardMaterial3D.new()
	hull.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	hull.albedo_color = colour
	hull.cull_mode = BaseMaterial3D.CULL_FRONT
	hull.grow = true
	hull.grow_amount = THICKNESS
	hull.no_depth_test = through
	hull.stencil_mode = BaseMaterial3D.STENCIL_MODE_CUSTOM
	hull.stencil_flags = BaseMaterial3D.STENCIL_FLAG_READ
	hull.stencil_compare = BaseMaterial3D.STENCIL_COMPARE_NOT_EQUAL
	hull.stencil_reference = _MARK
	hull.render_priority = 1
	mark.next_pass = hull
	_made[key] = mark
	return mark
