class_name WeaponModels
## What each weapon looks like in a player's hands, built from rounded
## shapes. Each model has a "Muzzle" marker where its shots leave from, and
## points along -Z like the camera. On top it has a rear sight, a notch
## between two posts, and a front sight, one post: the "Sight" and
## "FrontSight" markers are at their tops, and the line from one to the other
## runs the way the weapon points. A weapon whose magazine comes out to
## reload has it as a part called "Magazine".
##
## Two mittens hold it: "Hand" on the grip, and "OffHand" steadying it
## further forward. In a player's own view both are shown, each on the end
## of a stick arm coming up from below ("ViewArmRight", "ViewArmLeft").
## Everyone else sees only the hand on the grip, which their character's own
## arm reaches to; the rest are in the "view_only" group for the player to
## show or hide.
##
## Yellow is kept for cheese that can be eaten, so the weapons are steel,
## wood and wax, each with a colour of its own to tell it by.

const STEEL := Color(0.58, 0.61, 0.66)
const GUNMETAL := Color(0.2, 0.22, 0.26)
const DARK := Color(0.1, 0.1, 0.12)
const WOOD := Color(0.47, 0.29, 0.16)
## A burrata's pale, soft skin, and the leek it is tied with.
const BURRATA := Color(0.95, 0.93, 0.84)
const LEEK := Color(0.36, 0.55, 0.24)
const TEAL := Color(0.13, 0.5, 0.55)
const PURPLE := Color(0.45, 0.3, 0.62)
const GREEN := Color(0.22, 0.4, 0.26)
## The blue of a Roquefort's veins.
const BLUE := Color(0.24, 0.32, 0.52)
## A Munster's washed rind, gone to rust.
const RUST := Color(0.5, 0.2, 0.14)
## A breadstick's crust.
const CRUST := Color(0.5, 0.31, 0.17)
const LENS := Color(0.85, 0.93, 1.0)
## The red wax a Gouda comes wrapped in.
const WAX := Color(0.75, 0.12, 0.1)
## The mitten of the hand holding the weapon.
const HAND := Color(0.1, 0.09, 0.09)
## Where the hand holds every weapon, from the model's middle.
const GRIP := Vector3(0.0, -0.09, 0.12)
## Where a hand that closes round it holds each weapon whose grip is
## somewhere else, from the model's middle: a pistol's, further forward,
## under the slide. The drawn soldier's mittens stay at GRIP.
const HELD_AT := {
	"pistol": Vector3(0.0, -0.08, 0.05),
	"pistols": Vector3(0.0, -0.075, 0.05),
	"burratas": Vector3(0.0, -0.075, 0.05),
}
## How much nearer the eye than others each weapon sits in its holder's own
## view: a pistol, held out in the hand, is short, and nearer still would
## leave it lost at the bottom of the view.
const VIEW_NEARER := {"pistol": 0.1, "pistols": 0.08, "burratas": 0.08}
## Where the other hand steadies each weapon, from the model's middle.
const OFF_HAND := {
	"pistol": Vector3(-0.035, -0.125, 0.085),
	"blaster": Vector3(0.0, -0.085, -0.2),
	"shotgun": Vector3(0.0, -0.1, -0.14),
	"grater": Vector3(0.0, -0.105, -0.18),
	"launcher": Vector3(0.0, -0.085, -0.2),
	# The other hand has a pistol of its own.
	"pistols": Vector3(-PAIR_GAP, -0.09, 0.12),
	"burratas": Vector3(-PAIR_GAP, -0.09, 0.12),
	"carbine": Vector3(0.0, -0.085, -0.17),
	"rifle": Vector3(0.0, -0.075, -0.24),
	"lmg": Vector3(0.0, -0.1, -0.2),
	"bow": Vector3(0.0, -0.06, -0.2),
}
## How far to the left of the first the second of a pair of pistols is held.
const PAIR_GAP := 0.17
## Where each arm comes from, below and behind its hand, out of view.
const SHOULDER_RIGHT := Vector3(0.2, -0.4, 0.5)
const SHOULDER_LEFT := Vector3(-0.34, -0.4, 0.55)
## How tall the sights stand.
const SIGHT_HEIGHT := 0.022

static var _materials := {}


## Where a hand closes round weapon `id`, from the model's middle.
static func held_at(id: String) -> Vector3:
	return HELD_AT.get(id, GRIP)


## How far weapon `id` sits from where the others do in its holder's own
## view (see VIEW_NEARER).
static func view_offset(id: String) -> Vector3:
	return Vector3(0.0, 0.0, VIEW_NEARER.get(id, 0.0))


static func build(id: String) -> Node3D:
	var model := Node3D.new()
	model.name = "Model"
	# Which weapon it is, for a body holding it to know how.
	model.set_meta("weapon", id)
	var muzzle_z := -0.47
	match id:
		"pistol":
			# A slide over a short grip.
			_box(model, Vector3(0.07, 0.085, 0.27), Vector3(0.0, 0.0, -0.04), STEEL)
			_box(model, Vector3(0.06, 0.15, 0.085), Vector3(0.0, -0.09, 0.05), GUNMETAL)
			_box(model, Vector3(0.045, 0.05, 0.07), Vector3(0.0, -0.165, 0.05), DARK).name = "Magazine"
			_sights(model, 0.0425, 0.07, -0.16)
			muzzle_z = -0.18
		"pistols":
			# One in each hand; the sights are the right one's. The other is a
			# node of its own, for a hand that holds it to carry.
			var other := Node3D.new()
			other.name = "LeftPistol"
			other.position = Vector3(-PAIR_GAP, 0.0, 0.0)
			model.add_child(other)
			for pistol: Node3D in [model, other]:
				_box(pistol, Vector3(0.065, 0.08, 0.25), Vector3(0.0, 0.0, -0.04), GUNMETAL)
				_box(pistol, Vector3(0.055, 0.14, 0.08), Vector3(0.0, -0.085, 0.05), WOOD)
			_box(model, Vector3(0.042, 0.05, 0.065), Vector3(0.0, -0.16, 0.05), DARK).name = "Magazine"
			_sights(model, 0.04, 0.065, -0.15)
			muzzle_z = -0.17
		"burratas":
			# A pair, each a burrata: a soft white slide ending in the round
			# of the cheese, knotted at the grip with leek. The second is a
			# node of its own, for a hand that holds it; the sights are the
			# right one's.
			var other := Node3D.new()
			other.name = "LeftPistol"
			other.position = Vector3(-PAIR_GAP, 0.0, 0.0)
			model.add_child(other)
			for pistol: Node3D in [model, other]:
				_box(pistol, Vector3(0.07, 0.085, 0.24), Vector3(0.0, 0.0, -0.05), BURRATA, 0.03)
				_box(pistol, Vector3(0.085, 0.09, 0.07), Vector3(0.0, 0.0, 0.08), BURRATA, 0.04)
				_box(pistol, Vector3(0.055, 0.14, 0.08), Vector3(0.0, -0.085, 0.05), GUNMETAL)
				_box(pistol, Vector3(0.066, 0.02, 0.09), Vector3(0.0, -0.035, 0.05), LEEK, 0.006)
			_box(model, Vector3(0.042, 0.05, 0.065), Vector3(0.0, -0.16, 0.05), DARK).name = "Magazine"
			_sights(model, 0.0425, 0.07, -0.15)
			muzzle_z = -0.18
		"carbine":
			# Short and square, with a stock and a straight magazine.
			_box(model, Vector3(0.085, 0.11, 0.4), Vector3(0.0, 0.0, 0.0), GREEN)
			_box(model, Vector3(0.07, 0.08, 0.16), Vector3(0.0, -0.02, 0.27), DARK)
			_tube(model, 0.024, 0.2, Vector3(0.0, 0.02, -0.29), GUNMETAL)
			_box(model, Vector3(0.055, 0.17, 0.07), Vector3(0.0, -0.12, -0.05), GUNMETAL).name = "Magazine"
			_sights(model, 0.055, 0.14, -0.16)
			muzzle_z = -0.4
		"rifle":
			# Long and thin, with a scope along the top.
			_box(model, Vector3(0.07, 0.09, 0.62), Vector3(0.0, 0.0, 0.04), BLUE)
			_tube(model, 0.02, 0.4, Vector3(0.0, 0.015, -0.46), GUNMETAL)
			_box(model, Vector3(0.05, 0.1, 0.07), Vector3(0.0, -0.085, -0.04), DARK).name = "Magazine"
			_tube(model, 0.034, 0.3, Vector3(0.0, 0.084, 0.0), DARK)
			for end: float in [-0.1, 0.1]:
				_box(model, Vector3(0.03, 0.03, 0.03), Vector3(0.0, 0.05, end), GUNMETAL, 0.006)
			# The lens, which catches the light for others when it is looked through.
			var lens := SphereMesh.new()
			lens.radius = 0.03
			lens.height = 0.06
			lens.radial_segments = 10
			lens.rings = 5
			var shine := StandardMaterial3D.new()
			shine.albedo_color = LENS
			shine.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			var glint := Rounded.instance(lens, shine, Vector3(0.0, 0.084, -0.16))
			glint.name = "Glint"
			glint.visible = false
			model.add_child(glint)
			_sights(model, 0.118, 0.1, -0.1)
			muzzle_z = -0.66
		"lmg":
			# Heavy, with a box of rounds hung under it.
			_box(model, Vector3(0.12, 0.14, 0.5), Vector3(0.0, 0.0, 0.04), RUST)
			_tube(model, 0.034, 0.3, Vector3(0.0, 0.02, -0.34), GUNMETAL)
			_tube(model, 0.045, 0.1, Vector3(0.0, 0.02, -0.26), DARK)
			_box(model, Vector3(0.15, 0.13, 0.15), Vector3(0.0, -0.13, -0.06), GUNMETAL).name = "Magazine"
			_box(model, Vector3(0.08, 0.1, 0.16), Vector3(0.0, -0.02, 0.36), DARK)
			_sights(model, 0.07, 0.2, -0.16)
			muzzle_z = -0.49
		"bow":
			# A bow held flat across a wooden stock, with a breadstick laid
			# on it: the stick is what a reload puts back.
			_box(model, Vector3(0.05, 0.06, 0.42), Vector3(0.0, 0.0, 0.02), WOOD)
			_box(model, Vector3(0.62, 0.022, 0.04), Vector3(0.0, 0.0, -0.2), GREEN, 0.008)
			for side: float in [-1.0, 1.0]:
				_box(model, Vector3(0.012, 0.012, 0.2), Vector3(0.3 * side, 0.0, -0.1), DARK, 0.004)
			_box(model, Vector3(0.6, 0.006, 0.006), Vector3(0.0, 0.0, 0.0), DARK, 0.002)
			var stick := _box(model, Vector3(0.022, 0.022, 0.5), Vector3(0.0, 0.019, -0.04), CRUST, 0.008)
			stick.name = "Magazine"
			_sights(model, 0.03, 0.2, -0.08)
			muzzle_z = -0.3
		"shotgun":
			# Two barrels on a wooden stock.
			_box(model, Vector3(0.11, 0.11, 0.3), Vector3(0.0, -0.01, 0.1), WOOD)
			_box(model, Vector3(0.1, 0.07, 0.16), Vector3(0.0, -0.045, -0.12), WOOD)
			for side in [-1.0, 1.0]:
				_tube(model, 0.03, 0.44, Vector3(0.029 * side, 0.02, -0.14), GUNMETAL)
			_sights(model, 0.045, 0.2, -0.33)
			muzzle_z = -0.36
		"grater":
			# A boxy grater with rows of slots down each side.
			_box(model, Vector3(0.12, 0.16, 0.42), Vector3.ZERO, STEEL)
			for row in 4:
				_box(model, Vector3(0.128, 0.014, 0.3), Vector3(0.0, -0.05 + row * 0.03, 0.0), DARK, 0.004)
			_box(model, Vector3(0.09, 0.08, 0.12), Vector3(0.0, -0.1, 0.12), PURPLE)
			_box(model, Vector3(0.07, 0.12, 0.08), Vector3(0.0, -0.13, -0.08), DARK).name = "Magazine"
			_sights(model, 0.08, 0.18, -0.19)
			muzzle_z = -0.22
		"launcher":
			# A fat tube, wide at the mouth.
			_tube(model, 0.085, 0.52, Vector3(0, 0.02, -0.05), WAX)
			_tube(model, 0.1, 0.07, Vector3(0, 0.02, -0.29), GUNMETAL)
			_tube(model, 0.07, 0.03, Vector3(0, 0.02, -0.315), DARK)
			_box(model, Vector3(0.07, 0.16, 0.1), Vector3(0.0, -0.11, 0.08), GUNMETAL)
			_sights(model, 0.105, 0.16, -0.27)
			muzzle_z = -0.33
		_:
			# The blaster: a long body, a barrel and a magazine.
			_box(model, Vector3(0.1, 0.12, 0.5), Vector3(0.0, 0.0, 0.02), TEAL)
			_tube(model, 0.028, 0.24, Vector3(0.0, 0.02, -0.34), GUNMETAL)
			_box(model, Vector3(0.06, 0.15, 0.09), Vector3(0.0, -0.11, -0.06), GUNMETAL).name = "Magazine"
			_box(model, Vector3(0.03, 0.035, 0.2), Vector3(0.0, 0.075, 0.02), DARK)
			_sights(model, 0.0925, 0.11, -0.07)
	# The hand holding it, which also stands for the arm in first person.
	var hand := SphereMesh.new()
	hand.radius = 0.055
	hand.height = 0.11
	hand.radial_segments = 12
	hand.rings = 6
	var mitten := Rounded.instance(hand, _material(HAND), GRIP)
	mitten.name = "Hand"
	model.add_child(mitten)
	var steadying: Vector3 = OFF_HAND.get(id, OFF_HAND["blaster"])
	var other := Rounded.instance(hand, _material(HAND), steadying)
	other.name = "OffHand"
	model.add_child(other)
	var right := _arm(GRIP, GRIP + SHOULDER_RIGHT)
	right.name = "ViewArmRight"
	model.add_child(right)
	var left := _arm(steadying, steadying + SHOULDER_LEFT)
	left.name = "ViewArmLeft"
	model.add_child(left)
	for part: Node in [other, right, left]:
		part.add_to_group("view_only")
	var muzzle := Marker3D.new()
	muzzle.name = "Muzzle"
	muzzle.position = Vector3(0, 0.02, muzzle_z)
	model.add_child(muzzle)
	return model


## Shows the parts of `model` that only its holder sees, or hides them.
static func show_own_view(model: Node3D, own: bool) -> void:
	for part in model.get_children():
		if part.is_in_group("view_only"):
			part.visible = own


# A stick arm from a hand at `from` to a shoulder at `to`.
static func _arm(from: Vector3, to: Vector3) -> MeshInstance3D:
	var stick := CapsuleMesh.new()
	stick.radius = 0.022
	stick.height = from.distance_to(to)
	stick.radial_segments = 8
	stick.rings = 2
	var arm := Rounded.instance(stick, _material(HAND), (from + to) / 2.0)
	# The stick runs along its own Y axis.
	arm.quaternion = Quaternion(Vector3.UP, (to - from).normalized())
	return arm


static func _material(color: Color) -> StandardMaterial3D:
	if not _materials.has(color):
		var material := StandardMaterial3D.new()
		material.albedo_color = color
		material.roughness = 0.6
		_materials[color] = material
	return _materials[color]


static func _box(parent: Node3D, size: Vector3, at: Vector3, color: Color, rounding := 0.018) -> MeshInstance3D:
	var box := Rounded.instance(Rounded.box(size, rounding), _material(color), at)
	parent.add_child(box)
	return box


# Sights standing on the top of a weapon, which is `top` high: a rear notch
# at `rear` along it and a front post at `front`.
static func _sights(parent: Node3D, top: float, rear: float, front: float) -> void:
	var middle := top + SIGHT_HEIGHT / 2.0
	for side in [-1.0, 1.0]:
		_box(parent, Vector3(0.01, SIGHT_HEIGHT, 0.012), Vector3(0.013 * side, middle, rear), DARK, 0.002)
	_box(parent, Vector3(0.007, SIGHT_HEIGHT, 0.012), Vector3(0.0, middle, front), DARK, 0.002)
	for place: Array in [["Sight", rear], ["FrontSight", front]]:
		var marker := Marker3D.new()
		marker.name = place[0]
		marker.position = Vector3(0.0, top + SIGHT_HEIGHT, place[1])
		parent.add_child(marker)


# A round barrel of `radius`, `length` long, lying along the way the weapon
# points.
static func _tube(parent: Node3D, radius: float, length: float, at: Vector3, color: Color) -> void:
	var tube := Rounded.instance(Rounded.cylinder(radius, length, minf(0.012, radius * 0.4), 14), _material(color), at)
	tube.rotation.x = PI / 2.0
	parent.add_child(tube)
