extends Node3D
## What a player sees of their own arms, where the realistic soldier is shown
## (see Characters): its arms and sleeves, cut from its model by
## tools/convert_soldier.py, holding the weapon in view as others see the
## soldier hold its own.
##
## The body stands where the player's own would, its eyes at theirs, so that
## where the arms are cut from it stays out of view however the weapon moves,
## aimed down the sights or not. The arms reach for the weapon: the right hand
## holds it as the soldier's does (SOLDIER.hold), and the left where the aim
## has it against the right, so both hands hold it as the animation holds
## that kind of weapon, a rifle or a pistol. The forearms' twist bones (forearm_twist.gd) keep the wrists from
## pinching. The weapons are to be made to fit the hands, not the hands the
## weapons.

const SOLDIER := preload("res://player/soldier_character.gd")
const HAND_POSE := preload("res://player/hand_pose.gd")
## How far the body stands ahead of and below the eye, in the model's own
## metres: a little ahead, so that the hands reach the weapon, which is held
## further out than arms in proportion could, and low enough that its
## collarbones, where the arms are cut, stay below the view.
const BODY_AHEAD := 0.16
const BODY_DOWN := 0.16
## How much bigger than the weapon in view's size calls for the arms are
## drawn: a little, again so that the hands reach.
const LARGER := 1.15
## Where each elbow points, from the eyes, in the model's own metres: down
## and out to its side.
const ELBOWS := {"Right": Vector3(0.5, -0.6, 0.2), "Left": Vector3(-0.5, -0.6, 0.0)}

var _model: Node3D
var _skeleton: Skeleton3D
var _sleeves: StandardMaterial3D
var _gun: Node3D
# The model's size, its node's against this one's.
var _scale := 1.0
# Each hand's IK target (where its wrist goes) and the modifier turning it.
var _hands := {}
var _player: AnimationPlayer
# How the hands hold the weapon in view (see SOLDIER.GRIPS).
var _grip := "rifle"


## Arms for a weapon in view drawn `view_scale` times the size the drawn
## soldier's is.
func _init(view_scale: float) -> void:
	name = "FirstPersonArms"
	SOLDIER.find_hold_once()
	_model = load(SOLDIER.MODEL).instantiate()
	_model.name = "Soldier"
	add_child(_model)
	for mesh: MeshInstance3D in _model.find_children("*", "MeshInstance3D"):
		mesh.visible = SOLDIER.FIRST_PERSON.has(String(mesh.name))
		# Close to the eye, it is never in shadow's way.
		mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_skeleton = _model.find_children("*", "Skeleton3D")[0]
	# As big against the weapon in view as the soldier is against the one
	# others see, and LARGER.
	var chain := Transform3D()
	var node: Node = _skeleton
	while node != _model:
		chain = (node as Node3D).transform * chain
		node = node.get_parent()
	var rifle: Transform3D = SOLDIER.hold("rifle").in_hand
	var skeleton_scale := view_scale / rifle.basis.get_scale().x * LARGER
	_scale = skeleton_scale / chain.basis.get_scale().x
	# Facing ahead, its eyes at this node's origin, then ahead and down.
	_model.rotation.y = PI
	_model.scale = Vector3.ONE * _scale
	_model.position = Vector3(0.0, -SOLDIER.MODEL_EYE_HEIGHT - BODY_DOWN, -BODY_AHEAD) * _scale
	var sleeves: MeshInstance3D = _model.find_child("FirstPersonSleeves")
	_sleeves = (sleeves.get_active_material(0) as StandardMaterial3D).duplicate() as StandardMaterial3D
	sleeves.material_override = _sleeves
	_player = AnimationPlayer.new()
	_model.add_child(_player)
	_player.add_animation_library("", SOLDIER.library())
	_player.play(SOLDIER.GRIPS[_grip])
	for side: String in ["Right", "Left"]:
		_hands[side] = _arm(side)
	var twist: SkeletonModifier3D = preload("res://player/forearm_twist.gd").new()
	twist.name = "ForearmTwist"
	_skeleton.add_child(twist)


# Reaches `side`'s arm for where its wrist is to go, and turns its hand.
func _arm(side: String) -> Dictionary:
	var wrist := Node3D.new()
	wrist.name = side + "Wrist"
	add_child(wrist)
	var elbow := Node3D.new()
	elbow.name = side + "Elbow"
	elbow.position = ELBOWS[side] * _scale
	add_child(elbow)
	var ik := TwoBoneIK3D.new()
	ik.name = side + "ArmReach"
	_skeleton.add_child(ik)
	ik.set_setting_count(1)
	ik.set_root_bone_name(0, side + "UpperArm")
	ik.set_middle_bone_name(0, side + "LowerArm")
	ik.set_end_bone_name(0, side + "Hand")
	ik.set_target_node(0, ik.get_path_to(wrist))
	ik.set_pole_node(0, ik.get_path_to(elbow))
	var turn: SkeletonModifier3D = HAND_POSE.new()
	turn.name = side + "HandPose"
	turn.bone = _skeleton.find_bone(side + "Hand")
	_skeleton.add_child(turn)
	return {"wrist": wrist, "turn": turn}


## Tints the sleeves' camouflage for a player of `colour`, as others see it.
func set_colour(colour: Color) -> void:
	_sleeves.albedo_color = SOLDIER.shirt_colour(colour)


## Holds `gun`, the node the weapon in view is in.
func hold(gun: Node3D) -> void:
	_gun = gun


## Hides the drawn soldier's arms and mittens on `model`, a weapon model in
## view: these arms hold it instead.
static func hide_drawn_arms(model: Node3D) -> void:
	for part: String in ["Hand", "OffHand", "ViewArmRight", "ViewArmLeft"]:
		var drawn := model.get_node_or_null(part) as Node3D
		if drawn != null:
			drawn.visible = false


# Where the right fist closes now, in the skeleton's space.
func _fist_now() -> Vector3:
	var base := _skeleton.get_bone_global_pose(_skeleton.find_bone("RightMiddleProximal")).origin
	var tip := _skeleton.get_bone_global_pose(_skeleton.find_bone("RightMiddleDistal")).origin
	return (base + tip) / 2.0


func _process(_delta: float) -> void:
	if _gun == null or not is_instance_valid(_gun) or not _gun.is_inside_tree():
		return
	# Where the hands go, in the skeleton's space: the right holding the
	# weapon as the soldier's does, the left where the aim has it against the
	# right.
	# The newest model: one changed from waits to be freed beside it.
	var model := _gun.get_child(-1) as Node3D if _gun.get_child_count() > 0 else null
	var grip := SOLDIER.grip_of(model.get_meta("weapon", "") if model != null else "")
	if grip != _grip:
		_grip = grip
		_player.play(SOLDIER.GRIPS[grip])
	var hold: Dictionary = SOLDIER.hold(grip)
	var weapon := _skeleton.global_transform.affine_inverse() * _gun.global_transform
	var right := weapon * (hold.in_hand as Transform3D).affine_inverse()
	var left := right * (hold.left as Transform3D)
	for side: String in _hands:
		var hand: Transform3D = right if side == "Right" else left
		(_hands[side].wrist as Node3D).global_position = _skeleton.global_transform * hand.origin
		_hands[side].turn.facing = hand.basis
