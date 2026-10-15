class_name Clusters
extends Node3D
## Draws each building of a large map as one mesh rather than as the dozens
## of walls and floors it is built of, as drawing each thing costs the same
## however small it is, and a large map has hundreds of pieces (see BigMap).
##
## What is solid is still each piece, with its own collision, eaten on its
## own; only the drawing is shared. When a piece goes, the building's mesh is
## made again without it. The pieces of one building say which it is in
## their `group` metadata, and keep their own mesh, unseen, for this to be
## made from.
##
## The world adds this as a child of the map.

# Each building by its group: {"pieces": its bodies, "drawn": the one mesh,
# "showing": how many pieces were standing when it was last made}.
var _groups := {}


## Gathers the pieces under `geometry` into their buildings and draws each.
func build(geometry: Node) -> void:
	name = "Clusters"
	for body: Node in geometry.get_children():
		if not body.has_meta("group"):
			continue
		var own := body.get_node_or_null("MeshInstance3D") as MeshInstance3D
		if own == null:
			continue
		var group: int = body.get_meta("group")
		if not _groups.has(group):
			var drawn := MeshInstance3D.new()
			drawn.name = "Building%d" % group
			drawn.material_override = own.material_override
			add_child(drawn)
			_groups[group] = {"pieces": [], "drawn": drawn, "showing": -1}
		_groups[group].pieces.append(body)
		own.visible = false
	refresh()


## Makes again the mesh of any building whose pieces have come or gone.
func refresh() -> void:
	for group: Dictionary in _groups.values():
		var showing: int = group.pieces.filter(func(body: Node3D) -> bool: return body.visible).size()
		if showing != group.showing:
			group.showing = showing
			_draw(group)


## How many buildings there are, and how many pieces each is drawn with,
## for tests: [[group, pieces standing], ...].
func counts() -> Array:
	var found := []
	for key: int in _groups:
		found.append([key, _groups[key].showing])
	return found


func _draw(group: Dictionary) -> void:
	var drawn: MeshInstance3D = group.drawn
	if group.showing == 0:
		drawn.mesh = null
		return
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	for body: Node3D in group.pieces:
		if body.visible:
			tool.append_from((body.get_node("MeshInstance3D") as MeshInstance3D).mesh, 0, body.transform)
	drawn.mesh = tool.commit()
