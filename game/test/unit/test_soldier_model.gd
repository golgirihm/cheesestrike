extends GutTest
## The realistic soldier converted from the licensed download by
## tools/convert_soldier.py, and its animations, imported as
## tools/setup_soldier_import.gd sets: that the soldier has the bones and face
## shapes the game drives and stands as tall as a person, and that every
## animation plays on it. They are in the private game/licensed submodule, so
## a checkout without it skips these.

const PATH := "res://licensed/soldier/soldier.glb"
const ANIMATIONS := "res://licensed/animations"
## Bones the weapon and the hats are attached by, by the humanoid names both
## skeletons are mapped onto.
const BONES := ["Hips", "Spine", "UpperChest", "Head", "LeftHand", "RightHand", "LeftFoot", "RightFoot"]
## The animations the character plays, one to a file.
const PLAYED := ["idle", "aim", "walk", "walk_back", "walk_left", "walk_right", "run", "run_back", "run_left", "run_right", "jump", "slide", "fire", "reload", "throw", "melee", "hurt", "knocked_out", "emote_cheer", "emote_dance", "emote_shrug", "emote_wave", "crouch", "crouch_aim", "crouch_walk", "crouch_walk_back", "crouch_walk_left", "crouch_walk_right"]
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
	assert_eq(skeleton.name, &"GeneralSkeleton", "under the name the animations know it by")
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


func test_every_animation_moves_only_bones_the_soldier_has() -> void:
	if model == null:
		pending("The licensed assets aren't checked out.")
		return
	var skeleton: Skeleton3D = model.find_children("*", "Skeleton3D")[0]
	for name: String in PLAYED:
		var path := ANIMATIONS.path_join(name + ".fbx")
		assert_true(ResourceLoader.exists(path), path)
		if not ResourceLoader.exists(path):
			continue
		var library: AnimationLibrary = load(path)
		assert_eq(library.get_animation_list().size(), 1, name)
		var animation := library.get_animation(library.get_animation_list()[0])
		assert_gt(animation.get_track_count(), 20, "%s moves the body" % name)
		for track in animation.get_track_count():
			var target := animation.track_get_path(track)
			assert_eq(str(target.get_concatenated_names()), "%GeneralSkeleton", "%s moves the soldier's skeleton" % name)
			assert_gt(skeleton.find_bone(target.get_concatenated_subnames()), -1, "%s: %s" % [name, target])


func test_it_rests_in_a_level_t_pose_as_the_animations_do() -> void:
	# Animations are carried across from rest to rest, so a soldier resting
	# with its arms sloping would hold them wrong in every one.
	if model == null:
		pending("The licensed assets aren't checked out.")
		return
	var skeleton: Skeleton3D = model.find_children("*", "Skeleton3D")[0]
	for side in ["Left", "Right"]:
		var elbow := skeleton.get_bone_global_rest(skeleton.find_bone(side + "LowerArm")).origin
		var wrist := skeleton.get_bone_global_rest(skeleton.find_bone(side + "Hand")).origin
		var shoulder := skeleton.get_bone_global_rest(skeleton.find_bone(side + "UpperArm")).origin
		assert_almost_eq((elbow - shoulder).normalized().y, 0.0, 0.05, "%s upper arm level" % side)
		assert_almost_eq((wrist - elbow).normalized().y, 0.0, 0.05, "%s forearm level" % side)
