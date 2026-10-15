class_name Ghost
## A pale, see-through look for a player who has just come back and can't be
## hurt yet (see MatchState.PROTECTION), so everyone can tell at a glance.
##
## Each mesh's material is swapped for a see-through copy, and put back after:
## materials are often shared between players and weapons, so they are never
## changed in place.

## How much of a ghost shows, and how far toward white it is washed.
const OPACITY := 0.4
const PALE := 0.35


## Makes everything drawn under `root` a ghost, or puts it back (`on` false).
## Making it a ghost again takes up any change made to its materials since.
static func apply(root: Node, on: bool) -> void:
	for mesh: Node in root.find_children("*", "MeshInstance3D", true, false) + ([root] if root is MeshInstance3D else []):
		var shown := mesh as MeshInstance3D
		if shown.has_meta("solid"):
			shown.material_override = shown.get_meta("solid")
			shown.remove_meta("solid")
		if on and shown.material_override is BaseMaterial3D:
			shown.set_meta("solid", shown.material_override)
			shown.material_override = _ghostly(shown.material_override)


## Whether what is drawn under `root` is a ghost.
static func is_on(root: Node) -> bool:
	return root.find_children("*", "MeshInstance3D", true, false).any(func(mesh: Node) -> bool: return mesh.has_meta("solid"))


static func _ghostly(solid: BaseMaterial3D) -> BaseMaterial3D:
	var ghost: BaseMaterial3D = solid.duplicate()
	ghost.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	ghost.albedo_color = Color(solid.albedo_color.lerp(Color.WHITE, PALE), solid.albedo_color.a * OPACITY)
	return ghost
