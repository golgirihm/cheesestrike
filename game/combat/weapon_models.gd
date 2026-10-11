class_name WeaponModels
## What each weapon looks like in a player's hands, built from simple shapes.
## Each model has a "Muzzle" marker where its shots leave from, and points
## along -Z like the camera.

const CHEDDAR := Color(1.0, 0.62, 0.15)
const SWISS := Color(0.98, 0.9, 0.55)
const PARMESAN := Color(0.93, 0.88, 0.72)
const GOUDA := Color(0.95, 0.75, 0.2)
const RIND := Color(0.75, 0.12, 0.1)
const METAL := Color(0.62, 0.65, 0.7)
const DARK := Color(0.12, 0.12, 0.15)


static func build(id: String) -> Node3D:
	var model := Node3D.new()
	model.name = "Model"
	var muzzle_z := -0.47
	match id:
		"shotgun":
			_box(model, Vector3(0.16, 0.14, 0.5), Vector3.ZERO, SWISS)
			# The holes, of course.
			for hole in [Vector3(0.081, 0.02, -0.1), Vector3(0.081, -0.03, 0.08), Vector3(-0.081, 0.0, -0.02)]:
				_sphere(model, 0.035, hole, DARK)
			_box(model, Vector3(0.07, 0.07, 0.2), Vector3(0.0, 0.02, -0.33), DARK)
			muzzle_z = -0.334
		"grater":
			_box(model, Vector3(0.12, 0.16, 0.42), Vector3.ZERO, METAL)
			for row in 4:
				_box(model, Vector3(0.125, 0.012, 0.3), Vector3(0.0, -0.05 + row * 0.03, 0.0), DARK)
			_box(model, Vector3(0.09, 0.07, 0.12), Vector3(0.0, -0.1, 0.12), PARMESAN)
			muzzle_z = -0.24
		"launcher":
			var tube := CylinderMesh.new()
			tube.top_radius = 0.085
			tube.bottom_radius = 0.085
			tube.height = 0.52
			tube.material = _material(RIND)
			var barrel := MeshInstance3D.new()
			barrel.mesh = tube
			barrel.rotation.x = PI / 2.0
			barrel.position = Vector3(0, 0.02, -0.05)
			model.add_child(barrel)
			_box(model, Vector3(0.08, 0.16, 0.12), Vector3(0.0, -0.12, 0.08), GOUDA)
			muzzle_z = -0.33
		_:
			_box(model, Vector3(0.1, 0.12, 0.55), Vector3.ZERO, CHEDDAR)
			_box(model, Vector3(0.05, 0.05, 0.2), Vector3(0.0, 0.02, -0.36), DARK)
	# The hand holding it, which also stands for the arm in first person.
	_sphere(model, 0.055, Vector3(0.0, -0.09, 0.12), Color(0.95, 0.85, 0.6))
	var muzzle := Marker3D.new()
	muzzle.name = "Muzzle"
	muzzle.position = Vector3(0, 0.02, muzzle_z)
	model.add_child(muzzle)
	return model


static func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.6
	return material


static func _box(parent: Node3D, size: Vector3, at: Vector3, color: Color) -> void:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = _material(color)
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = at
	parent.add_child(instance)


static func _sphere(parent: Node3D, radius: float, at: Vector3, color: Color) -> void:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.material = _material(color)
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = at
	parent.add_child(instance)
