class_name PickupModels
## What pickups look like on their pads: a wedge of cheese for health, the
## weapon itself otherwise, floating above a round base.


static func build(kind: String) -> Node3D:
	var holder := Node3D.new()
	holder.position = Vector3(0, 0.9, 0)
	if kind == Pickups.HEALTH:
		var wedge := PrismMesh.new()
		wedge.size = Vector3(0.6, 0.45, 0.4)
		wedge.left_to_right = 0.0
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(1.0, 0.85, 0.3)
		material.emission_enabled = true
		material.emission = Color(1.0, 0.7, 0.1)
		material.emission_energy_multiplier = 0.4
		wedge.material = material
		var mesh := MeshInstance3D.new()
		mesh.mesh = wedge
		holder.add_child(mesh)
	else:
		var model := WeaponModels.build(kind)
		model.scale = Vector3.ONE * 1.6
		model.rotation.y = PI / 2.0
		holder.add_child(model)
	return holder


static func base() -> MeshInstance3D:
	var disc := CylinderMesh.new()
	disc.top_radius = 0.7
	disc.bottom_radius = 0.75
	disc.height = 0.06
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.85, 0.55, 0.15)
	disc.material = material
	var mesh := MeshInstance3D.new()
	mesh.mesh = disc
	mesh.position = Vector3(0, 0.03, 0)
	return mesh
