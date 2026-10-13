class_name PickupModels
## What pickups look like on their pads: a wedge of cheese for health, the
## weapon itself otherwise, floating above a round base.

## Cheese to eat is the one thing in the arena that is yellow.
const CHEESE := Color(1.0, 0.82, 0.22)
const BASE := Color(0.36, 0.38, 0.43)


static func build(kind: String) -> Node3D:
	var holder := Node3D.new()
	holder.position = Vector3(0, 0.9, 0)
	if kind == Pickups.HEALTH:
		var material := StandardMaterial3D.new()
		material.albedo_color = CHEESE
		material.emission_enabled = true
		material.emission = Color(1.0, 0.7, 0.1)
		material.emission_energy_multiplier = 0.4
		# A slice lying on its side: a right-angled wedge, point forward.
		var outline := PackedVector2Array([Vector2(-0.3, -0.2), Vector2(0.3, -0.2), Vector2(-0.3, 0.25)])
		holder.add_child(Rounded.instance(Rounded.prism(outline, 0.4, 0.05), material))
	elif kind == Pickups.ARMOUR:
		# A thick curl of wax rind, to wear.
		var wax := StandardMaterial3D.new()
		wax.albedo_color = WeaponModels.WAX
		var inside := StandardMaterial3D.new()
		inside.albedo_color = Color(0.95, 0.93, 0.88)
		holder.add_child(Rounded.instance(Rounded.cylinder(0.3, 0.16, 0.06, 24), wax))
		holder.add_child(Rounded.instance(Rounded.cylinder(0.2, 0.17, 0.03, 24), inside))
	elif kind == Pickups.AMMO:
		# A crate with a pale band round it.
		var crate := StandardMaterial3D.new()
		crate.albedo_color = Color(0.24, 0.42, 0.3)
		var band := StandardMaterial3D.new()
		band.albedo_color = Color(0.9, 0.9, 0.86)
		holder.add_child(Rounded.instance(Rounded.box(Vector3(0.55, 0.36, 0.4), 0.05), crate))
		holder.add_child(Rounded.instance(Rounded.box(Vector3(0.57, 0.1, 0.42), 0.02), band))
	else:
		var model := WeaponModels.build(kind)
		model.scale = Vector3.ONE * 1.6
		model.rotation.y = PI / 2.0
		# Nobody is holding it yet.
		model.get_node("Hand").visible = false
		WeaponModels.show_own_view(model, false)
		holder.add_child(model)
	return holder


static func base() -> MeshInstance3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = BASE
	return Rounded.instance(Rounded.cylinder(0.75, 0.08, 0.03, 28), material, Vector3(0, 0.04, 0))
