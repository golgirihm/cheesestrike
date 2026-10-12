class_name WeaponModels
## What each weapon looks like in a player's hands, built from rounded
## shapes. Each model has a "Muzzle" marker where its shots leave from, and
## points along -Z like the camera.
##
## Yellow is kept for cheese that can be eaten, so the weapons are steel,
## wood and wax, each with a colour of its own to tell it by.

const STEEL := Color(0.58, 0.61, 0.66)
const GUNMETAL := Color(0.2, 0.22, 0.26)
const DARK := Color(0.1, 0.1, 0.12)
const WOOD := Color(0.47, 0.29, 0.16)
const TEAL := Color(0.13, 0.5, 0.55)
const PURPLE := Color(0.45, 0.3, 0.62)
## The red wax a Gouda comes wrapped in.
const WAX := Color(0.75, 0.12, 0.1)
## The mitten of the hand holding the weapon.
const HAND := Color(0.1, 0.09, 0.09)
## Where the hand holds every weapon, from the model's middle.
const GRIP := Vector3(0.0, -0.09, 0.12)

static var _materials := {}


static func build(id: String) -> Node3D:
	var model := Node3D.new()
	model.name = "Model"
	var muzzle_z := -0.47
	match id:
		"pistol":
			# A slide over a short grip.
			_box(model, Vector3(0.07, 0.085, 0.27), Vector3(0.0, 0.0, -0.04), STEEL)
			_box(model, Vector3(0.06, 0.15, 0.085), Vector3(0.0, -0.09, 0.05), GUNMETAL)
			_box(model, Vector3(0.03, 0.03, 0.05), Vector3(0.0, 0.045, 0.06), DARK)
			muzzle_z = -0.18
		"shotgun":
			# Two barrels on a wooden stock.
			_box(model, Vector3(0.11, 0.11, 0.3), Vector3(0.0, -0.01, 0.1), WOOD)
			_box(model, Vector3(0.1, 0.07, 0.16), Vector3(0.0, -0.045, -0.12), WOOD)
			for side in [-1.0, 1.0]:
				_tube(model, 0.03, 0.44, Vector3(0.029 * side, 0.02, -0.14), GUNMETAL)
			muzzle_z = -0.36
		"grater":
			# A boxy grater with rows of slots down each side.
			_box(model, Vector3(0.12, 0.16, 0.42), Vector3.ZERO, STEEL)
			for row in 4:
				_box(model, Vector3(0.128, 0.014, 0.3), Vector3(0.0, -0.05 + row * 0.03, 0.0), DARK, 0.004)
			_box(model, Vector3(0.09, 0.08, 0.12), Vector3(0.0, -0.1, 0.12), PURPLE)
			muzzle_z = -0.22
		"launcher":
			# A fat tube, wide at the mouth.
			_tube(model, 0.085, 0.52, Vector3(0, 0.02, -0.05), WAX)
			_tube(model, 0.1, 0.07, Vector3(0, 0.02, -0.29), GUNMETAL)
			_tube(model, 0.07, 0.03, Vector3(0, 0.02, -0.315), DARK)
			_box(model, Vector3(0.07, 0.16, 0.1), Vector3(0.0, -0.11, 0.08), GUNMETAL)
			muzzle_z = -0.33
		_:
			# The blaster: a long body, a barrel and a magazine.
			_box(model, Vector3(0.1, 0.12, 0.5), Vector3(0.0, 0.0, 0.02), TEAL)
			_tube(model, 0.028, 0.24, Vector3(0.0, 0.02, -0.34), GUNMETAL)
			_box(model, Vector3(0.06, 0.15, 0.09), Vector3(0.0, -0.11, -0.06), GUNMETAL)
			_box(model, Vector3(0.03, 0.035, 0.2), Vector3(0.0, 0.075, 0.02), DARK)
	# The hand holding it, which also stands for the arm in first person.
	var hand := SphereMesh.new()
	hand.radius = 0.055
	hand.height = 0.11
	hand.radial_segments = 12
	hand.rings = 6
	var mitten := Rounded.instance(hand, _material(HAND), GRIP)
	mitten.name = "Hand"
	model.add_child(mitten)
	var muzzle := Marker3D.new()
	muzzle.name = "Muzzle"
	muzzle.position = Vector3(0, 0.02, muzzle_z)
	model.add_child(muzzle)
	return model


static func _material(color: Color) -> StandardMaterial3D:
	if not _materials.has(color):
		var material := StandardMaterial3D.new()
		material.albedo_color = color
		material.roughness = 0.6
		_materials[color] = material
	return _materials[color]


static func _box(parent: Node3D, size: Vector3, at: Vector3, color: Color, rounding := 0.018) -> void:
	parent.add_child(Rounded.instance(Rounded.box(size, rounding), _material(color), at))


# A round barrel of `radius`, `length` long, lying along the way the weapon
# points.
static func _tube(parent: Node3D, radius: float, length: float, at: Vector3, color: Color) -> void:
	var tube := Rounded.instance(Rounded.cylinder(radius, length, minf(0.012, radius * 0.4), 14), _material(color), at)
	tube.rotation.x = PI / 2.0
	parent.add_child(tube)
