extends GutTest
## The realistic soldier converted from the licensed download by
## tools/convert_soldier.py: that it has the bones and face shapes the game
## drives, and stands as tall as a person. It is in the private game/licensed
## submodule, so a checkout without it skips these.

const PATH := "res://licensed/soldier/soldier.glb"
## Bones the animations, the weapon and the hats are attached by.
const BONES := ["CC_Base_Hip", "CC_Base_Waist", "CC_Base_Spine02", "CC_Base_Head", "CC_Base_L_Hand", "CC_Base_R_Hand", "CC_Base_L_Foot", "CC_Base_R_Foot"]
## Face shapes the faces are made of: blinking, wincing, cheering and firing.
const FACE := ["Eye_Blink_L", "Eye_Blink_R", "Eye_Squint_L", "Eye_Squint_R", "Brow_Drop_L", "Brow_Drop_R", "Mouth_Smile_L", "Mouth_Smile_R", "Mouth_Stretch_L", "Mouth_Stretch_R", "Mouth_Press_L", "Mouth_Press_R", "Jaw_Open"]

var model: Node3D


func before_each() -> void:
	if ResourceLoader.exists(PATH):
		model = load(PATH).instantiate()
		add_child_autofree(model)


func test_it_has_the_bones_the_game_drives() -> void:
	if model == null:
		pending("The licensed assets aren't checked out.")
		return
	var skeleton: Skeleton3D = model.find_children("*", "Skeleton3D")[0]
	for bone: String in BONES:
		assert_gt(skeleton.find_bone(bone), -1, bone)


func test_its_face_has_the_shapes_the_faces_are_made_of() -> void:
	if model == null:
		pending("The licensed assets aren't checked out.")
		return
	var body: MeshInstance3D = model.find_child("CC_Base_Body")
	for shape: String in FACE:
		assert_gt(body.find_blend_shape_by_name(shape), -1, shape)


func test_it_stands_as_tall_as_a_person_on_the_ground() -> void:
	if model == null:
		pending("The licensed assets aren't checked out.")
		return
	var lowest := INF
	var highest := -INF
	for mesh: MeshInstance3D in model.find_children("*", "MeshInstance3D"):
		var box := mesh.get_aabb()
		lowest = minf(lowest, box.position.y)
		highest = maxf(highest, box.end.y)
	assert_almost_eq(lowest, 0.0, 0.03, "its feet are on the ground")
	assert_between(highest, 1.75, 1.9, "and the top of its cap is about 1.8 m up")
