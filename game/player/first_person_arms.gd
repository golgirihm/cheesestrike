extends Node3D
## What a player sees of their own arms, where the realistic soldier is shown
## (see Characters): its arms and sleeves, cut from its model by
## tools/convert_soldier.py, holding the weapon in view as others see the
## soldier hold its own.
##
## The arms keep the aim's own pose; nothing bends them to fit the weapon.
## They go wherever the weapon in view is, its grip in the right hand just as
## in the soldier's, and at the same size against it, so the left hand is
## where the aim holds a rifle's fore-end. The weapons are to be made to fit
## the hands, not the hands the weapons.

const SOLDIER := preload("res://player/soldier_character.gd")
## How much nearer and lower the weapon in view is held, in metres, with
## these arms: they go where it is, and held where the drawn soldier's is,
## their shoulders would be just in front of the eye, at the bottom of the
## view, where the arms are cut from the body.
const VIEW_SHIFT := Vector3(0.0, -0.05, 0.07)

var _model: Node3D
var _skeleton: Skeleton3D
var _sleeves: StandardMaterial3D
var _gun: Node3D
var _right_hand := -1
# From the model to its skeleton.
var _to_skeleton := Transform3D()


func _init() -> void:
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
	_right_hand = _skeleton.find_bone("RightHand")
	var node: Node = _skeleton
	while node != _model:
		_to_skeleton = (node as Node3D).transform * _to_skeleton
		node = node.get_parent()
	var sleeves: MeshInstance3D = _model.find_child("FirstPersonSleeves")
	_sleeves = (sleeves.get_active_material(0) as StandardMaterial3D).duplicate() as StandardMaterial3D
	sleeves.material_override = _sleeves
	var player := AnimationPlayer.new()
	_model.add_child(player)
	player.add_animation_library("", SOLDIER.library())
	player.play("aim")
	_skeleton.skeleton_updated.connect(_follow_weapon)


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


# Puts the arms where the weapon in view is, its grip in the right hand as
# the skeleton now stands: the weapon in the hand is where the soldier holds
# it (SOLDIER._in_hand), so the skeleton is where that puts it.
func _follow_weapon() -> void:
	if _gun == null or not is_instance_valid(_gun) or not _gun.is_inside_tree():
		return
	var held := _skeleton.get_bone_global_pose(_right_hand) * SOLDIER._in_hand
	_model.global_transform = _gun.global_transform * held.affine_inverse() * _to_skeleton.affine_inverse()
