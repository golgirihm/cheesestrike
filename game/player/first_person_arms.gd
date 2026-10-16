extends Node3D
## What a player sees of their own arms, where the realistic soldier is shown
## (see Characters): its arms and sleeves, cut from its model by
## tools/convert_soldier.py, with both hands on the weapon in view, held as
## others see it hold its own. The right hand closes round the grip and the
## left wraps the fore-end.
##
## It sits at the eyes, facing ahead, under the node the view and the weapon
## turn with. The weapon in view is drawn smaller and nearer than others see
## it, so the arms are drawn smaller too, though not by as much: the weapon
## is held further out than arms in proportion could reach, so they are
## LARGER, as first-person arms usually are, and their shoulders come
## forward.

const SOLDIER := preload("res://player/soldier_character.gd")
const HAND_TURN := preload("res://player/hand_turn.gd")
## How much larger the arms are drawn than the weapon in view's size calls
## for, and how far ahead of and below the eyes their shoulders are put, in
## the model's own metres: enough for each hand to reach, with the upper arms
## kept low, out of the way of the view.
const LARGER := 1.4
const SHOULDERS_AHEAD := 0.2
const SHOULDERS_DOWN := 0.1
## Where each elbow points, from the eyes in the model's own metres: down
## and out to its side.
const ELBOWS := {"Right": Vector3(0.45, -0.7, 0.1), "Left": Vector3(-0.45, -0.7, 0.0)}

var _model: Node3D
var _sleeves: StandardMaterial3D
var _skeleton: Skeleton3D
var _gun: Node3D
var _scale := 1.0
## For each side, its IK, the modifier turning its hand, and where its wrist
## is to go.
var _reach := {}


## Arms for a weapon in view drawn `view_scale` times the size the drawn
## soldier's is.
func _init(view_scale: float) -> void:
	name = "FirstPersonArms"
	SOLDIER.find_hold_once()
	_scale = SOLDIER.model_scale() * view_scale / SOLDIER.WEAPON_SCALE * LARGER
	_model = load(SOLDIER.MODEL).instantiate()
	_model.name = "Soldier"
	# The model faces +Z; the view looks down -Z.
	_model.rotation.y = PI
	_model.scale = Vector3.ONE * _scale
	_model.position = Vector3(0.0, -SOLDIER.MODEL_EYE_HEIGHT - SHOULDERS_DOWN, -SHOULDERS_AHEAD) * _scale
	add_child(_model)
	for mesh: MeshInstance3D in _model.find_children("*", "MeshInstance3D"):
		mesh.visible = SOLDIER.FIRST_PERSON.has(String(mesh.name))
		# Close to the eye, it is never in shadow's way.
		mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_skeleton = _model.find_children("*", "Skeleton3D")[0]
	var sleeves: MeshInstance3D = _model.find_child("FirstPersonSleeves")
	_sleeves = (sleeves.get_active_material(0) as StandardMaterial3D).duplicate() as StandardMaterial3D
	sleeves.material_override = _sleeves
	# The arms rest as in the aim, the hands then pulled onto the weapon.
	var player := AnimationPlayer.new()
	_model.add_child(player)
	player.add_animation_library("", SOLDIER.library())
	player.play("aim")
	for side: String in ["Right", "Left"]:
		_reach[side] = _arm(side)


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
	var turn: SkeletonModifier3D = HAND_TURN.new()
	turn.name = side + "HandTurn"
	turn.bone = _skeleton.find_bone(side + "Hand")
	turn.side = side
	# The right hand's fingers are round the grip in the aim already.
	turn.bends_fingers = side == "Left"
	turn.curl = SOLDIER.GRIP_CURL
	_skeleton.add_child(turn)
	return {"wrist": wrist, "turn": turn}


## Tints the sleeves' camouflage for a player of `colour`, as others see it.
func set_colour(colour: Color) -> void:
	_sleeves.albedo_color = SOLDIER.shirt_colour(colour)


## Holds `gun`, the node the weapon in view is in.
func hold(gun: Node3D) -> void:
	_gun = gun


## Hides the drawn soldier's arms and mittens on `model`, a weapon model in
## view: these arms hold it instead. Its "OffHand" stays, out of sight, to
## mark where the left hand goes.
static func hide_drawn_arms(model: Node3D) -> void:
	for part: String in ["Hand", "OffHand", "ViewArmRight", "ViewArmLeft"]:
		var drawn := model.get_node_or_null(part) as Node3D
		if drawn != null:
			drawn.visible = false


func _process(_delta: float) -> void:
	if _gun == null or not is_instance_valid(_gun) or _gun.get_child_count() == 0:
		return
	# The newest model: one changed from waits to be freed beside it.
	var model := _gun.get_child(-1) as Node3D
	var fore_end := model.get_node_or_null("OffHand") as Node3D
	var space := _skeleton.global_transform
	var weapon := (space.basis.inverse() * _gun.global_basis).orthonormalized()
	# The right fist round the grip, turned against the weapon as others see
	# it hold its own; the left round the fore-end.
	var right := weapon * SOLDIER._in_hand.basis.orthonormalized().inverse()
	_put("Right", right, space.affine_inverse() * model.to_global(WeaponModels.GRIP), SOLDIER._right_hold)
	_reach.Left.turn.active = fore_end != null
	(_skeleton.get_node("LeftArmReach") as SkeletonModifier3D).active = fore_end != null
	if fore_end != null:
		var left := weapon * SOLDIER.LEFT_TURN
		_put("Left", left, space.affine_inverse() * fore_end.to_global(Vector3.UP * SOLDIER.FORE_END_LIFT), SOLDIER.LEFT_HOLD)


# Turns `side`'s hand to `facing` and puts its wrist so that the point
# `hold` from it, in its own space, is at `at`; both in the skeleton's space.
func _put(side: String, facing: Basis, at: Vector3, hold: Vector3) -> void:
	_reach[side].turn.facing = facing
	(_reach[side].wrist as Node3D).global_position = _skeleton.global_transform * (at - facing * hold)
