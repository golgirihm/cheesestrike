extends GutTest
## What a realistic soldier's player sees of their own arms
## (player/first_person_arms.gd): the model's arms alone, both hands on the
## weapon in view. They need the licensed model in the private game/licensed
## submodule, so a checkout without it skips most of these.

const ARMS := preload("res://player/first_person_arms.gd")
const SOLDIER := preload("res://player/soldier_character.gd")
## As the player's scene holds the weapon in view (player.gd).
const VIEW_GUN_POSITION := Vector3(0.19, -0.18, -0.44)
const VIEW_GUN_SCALE := 0.6


func test_the_drawn_arms_and_mittens_on_a_weapon_in_view_are_hidden() -> void:
	var model := WeaponModels.build("blaster")
	autofree(model)
	WeaponModels.show_own_view(model, true)
	ARMS.hide_drawn_arms(model)
	for part in ["Hand", "OffHand", "ViewArmRight", "ViewArmLeft"]:
		assert_false((model.get_node(part) as Node3D).visible, part)
	assert_true((model.get_node("Muzzle") as Node3D) != null, "the weapon itself is all there")


func test_only_its_arms_show_and_take_the_players_colour() -> void:
	if not SOLDIER.available():
		pending("The licensed assets aren't checked out.")
		return
	var arms: Node3D = ARMS.new(VIEW_GUN_SCALE)
	add_child_autofree(arms)
	for mesh: MeshInstance3D in arms.find_children("*", "MeshInstance3D", true, false):
		assert_eq(mesh.visible, SOLDIER.FIRST_PERSON.has(String(mesh.name)), String(mesh.name))
	arms.set_colour(Color(0.2, 0.4, 0.9))
	assert_eq(arms._sleeves.albedo_color, SOLDIER.shirt_colour(Color(0.2, 0.4, 0.9)))


func test_both_hands_are_on_the_weapon_in_view() -> void:
	if not SOLDIER.available():
		pending("The licensed assets aren't checked out.")
		return
	var head := Node3D.new()
	add_child_autofree(head)
	var gun := Node3D.new()
	gun.position = VIEW_GUN_POSITION
	gun.scale = Vector3.ONE * VIEW_GUN_SCALE
	head.add_child(gun)
	var model := WeaponModels.build("blaster")
	gun.add_child(model)
	var arms: Node3D = ARMS.new(VIEW_GUN_SCALE)
	head.add_child(arms)
	arms.hold(gun)
	var skeleton: Skeleton3D = arms._skeleton
	var fists := {}
	var note := func() -> void:
		fists.Right = skeleton.global_transform * (skeleton.get_bone_global_pose(skeleton.find_bone("RightHand")) * SOLDIER._right_hold)
		fists.Left = skeleton.global_transform * (skeleton.get_bone_global_pose(skeleton.find_bone("LeftHand")) * SOLDIER.LEFT_HOLD)
	skeleton.skeleton_updated.connect(note)
	await wait_process_frames(10)
	skeleton.skeleton_updated.disconnect(note)
	var reach := VIEW_GUN_SCALE * 0.02
	assert_lt(fists.Right.distance_to(model.to_global(WeaponModels.GRIP)), reach, "the right fist round the grip")
	var fore_end := (model.get_node("OffHand") as Node3D).to_global(Vector3.UP * SOLDIER.FORE_END_LIFT)
	assert_lt(fists.Left.distance_to(fore_end), reach, "the left round the fore-end")
