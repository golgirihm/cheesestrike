extends Node3D
## What the modes with places of their own put in the arena for everyone to
## see: each team's base and its wheel of cheese, the hill, the three points.
## The world adds this as a child and tells it when the match state changes;
## it only draws, and none of it is solid.

## How far up a wheel sits when it is on the ground, and on a carrier's back.
const WHEEL_HEIGHT := 1.1
const NEUTRAL := Color(0.92, 0.92, 0.95)

var world: Node3D
# What is drawn now, to tell when it must be drawn again: the arena and mode.
var _drawn := ""
var _bases: Array[Node3D] = []
var _wheels: Array[Node3D] = []
var _zones: Array[Node3D] = []


func _init(of: Node3D) -> void:
	world = of
	name = "ObjectiveMarks"


## Brings what is drawn in line with the match state.
func refresh() -> void:
	var state: MatchState = world.match_.state if world.match_.has_state() else null
	var wanted := "%s:%s" % [world.map_id, state.settings.mode] if state != null and (state.capture != null or state.zones != null) else ""
	if wanted != _drawn:
		_drawn = wanted
		_build(state)
	if state == null:
		return
	if state.zones != null:
		var hill := state.settings.mode == MatchSettings.HILL
		for index in _zones.size():
			var owner: int = ZoneRules.NOBODY if hill else (state.zones.owners[index] if index < state.zones.owners.size() else ZoneRules.NOBODY)
			_zones[index].visible = not hill or index == state.zones.hill
			_tint(_zones[index], TeamColors.of(owner) if owner >= 0 else NEUTRAL)
	_place_wheels()


## Where the objectives showing are, and the colour of each: [{"at",
## "colour"}], for the compass.
func shown() -> Array:
	var found := []
	for mark: Node3D in _bases + _wheels + _zones:
		if is_instance_valid(mark) and mark.is_visible_in_tree():
			found.append({"at": mark.global_position, "colour": mark.get_meta("colour", NEUTRAL)})
	return found


## How many bases, wheels and zones are drawn, for tests.
func counts() -> Dictionary:
	return {"bases": _bases.size(), "wheels": _wheels.size(), "zones": _zones.size(), "shown": _zones.filter(func(zone: Node3D) -> bool: return zone.visible).size()}


func _process(_delta: float) -> void:
	# A carried wheel goes where its carrier does, between the host's words.
	if not _wheels.is_empty():
		_place_wheels()


func _build(state: MatchState) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_bases.clear()
	_wheels.clear()
	_zones.clear()
	if _drawn.is_empty():
		return
	var spots: Dictionary = world.objective_spots()
	if state.capture != null:
		for team in mini(spots.bases.size(), 2):
			var base := _disc(CaptureRules.BASE_REACH, TeamColors.of(team))
			add_child(base)
			base.global_position = spots.bases[team]
			_bases.append(base)
			var wheel := _wheel(TeamColors.of(team))
			add_child(wheel)
			_wheels.append(wheel)
	else:
		var places: Array = spots.hills if state.settings.mode == MatchSettings.HILL else spots.points
		for index in places.size():
			var zone := _disc(ZoneRules.RADIUS, NEUTRAL)
			add_child(zone)
			zone.global_position = places[index]
			# A beam to find it by from across the arena.
			var beam := MeshInstance3D.new()
			var shaft := CylinderMesh.new()
			shaft.top_radius = 0.12
			shaft.bottom_radius = 0.12
			shaft.height = 14.0
			shaft.radial_segments = 8
			beam.mesh = shaft
			beam.position = Vector3(0, 7.0, 0)
			beam.name = "Beam"
			beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			zone.add_child(beam)
			if state.settings.mode == MatchSettings.POINTS:
				var letter := Label3D.new()
				letter.text = ZoneRules.NAMES[index]
				letter.font_size = 160
				letter.outline_size = 24
				letter.billboard = BaseMaterial3D.BILLBOARD_ENABLED
				letter.no_depth_test = true
				letter.position = Vector3(0, 4.0, 0)
				zone.add_child(letter)
			_tint(zone, NEUTRAL)
			_zones.append(zone)


func _place_wheels() -> void:
	var state: MatchState = world.match_.state if world.match_.has_state() else null
	if state == null or state.capture == null:
		return
	for team in mini(_wheels.size(), state.capture.wheels.size()):
		var wheel: Dictionary = state.capture.wheels[team]
		var carrier: Node3D = world.player_node(wheel.carrier) if wheel.status == CaptureRules.CARRIED else null
		if carrier != null:
			# On their back.
			_wheels[team].global_position = carrier.global_position + carrier.global_basis.z * (BodyShape.RADIUS + 0.3) + Vector3.UP * BodyShape.WEDGE_CENTRE.y * BodyShape.SIZE
			_wheels[team].rotation.y = carrier.rotation.y + PI / 2.0
		else:
			_wheels[team].global_position = wheel.at + Vector3.UP * WHEEL_HEIGHT
			_wheels[team].rotation.y = 0.0


# A flat round mark on the ground, `radius` across.
func _disc(radius: float, colour: Color) -> Node3D:
	var holder := Node3D.new()
	var disc := MeshInstance3D.new()
	var flat := CylinderMesh.new()
	flat.top_radius = radius
	flat.bottom_radius = radius
	flat.height = 0.04
	flat.radial_segments = 40
	disc.mesh = flat
	disc.position = Vector3(0, 0.03, 0)
	disc.name = "Disc"
	disc.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	holder.add_child(disc)
	_tint(holder, colour)
	return holder


# A wheel of cheese in its team's wax, standing on its rim.
func _wheel(colour: Color) -> Node3D:
	var wax := StandardMaterial3D.new()
	wax.albedo_color = colour
	wax.emission_enabled = true
	wax.emission = colour
	wax.emission_energy_multiplier = 0.5
	var wheel := Rounded.instance(Rounded.cylinder(1.0, 0.5, 0.12, 28), wax)
	wheel.rotation.x = PI / 2.0
	var holder := Node3D.new()
	holder.add_child(wheel)
	return holder


# Colours a mark's disc and beam, see-through.
func _tint(mark: Node3D, colour: Color) -> void:
	if mark.get_meta("colour", Color.BLACK) == colour:
		return
	mark.set_meta("colour", colour)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(colour, 0.35)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	for part: MeshInstance3D in mark.find_children("*", "MeshInstance3D", false, false):
		part.material_override = material
