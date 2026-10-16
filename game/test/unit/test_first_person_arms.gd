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


# Where the weapon in view is held from the eyes, and where to aim down the
# sights, near enough (player.gd's VIEW_GUN_POSITION and aim_gun_position).
const AIMED_GUN_POSITION := Vector3(0.0, -0.06, -0.33)


# Arms holding a weapon in view at `at`, with what each frame shows of where
# the hands and collarbones are, in the eye's space.
func _arms_holding(at: Vector3) -> Dictionary:
	var head := Node3D.new()
	add_child_autofree(head)
	var gun := Node3D.new()
	gun.position = at
	gun.scale = Vector3.ONE * VIEW_GUN_SCALE
	head.add_child(gun)
	var model := WeaponModels.build("blaster")
	gun.add_child(model)
	var arms: Node3D = ARMS.new(VIEW_GUN_SCALE)
	head.add_child(arms)
	arms.hold(gun)
	var seen := {"gun": gun, "model": model, "arms": arms}
	var skeleton: Skeleton3D = arms._skeleton
	skeleton.skeleton_updated.connect(func() -> void:
		var at_bone := func(name: String) -> Vector3:
			return skeleton.global_transform * skeleton.get_bone_global_pose(skeleton.find_bone(name)).origin
		seen.fist = skeleton.global_transform * arms._fist_now()
		for side in ["Left", "Right"]:
			seen[side + "Hand"] = skeleton.global_transform * skeleton.get_bone_global_pose(skeleton.find_bone(side + "Hand"))
			seen[side + "Collar"] = at_bone.call(side + "Shoulder"))
	await wait_process_frames(10)
	return seen


func test_the_weapon_in_view_is_held_as_others_see_the_soldiers_aimed_or_not() -> void:
	if not SOLDIER.available():
		pending("The licensed assets aren't checked out.")
		return
	for at: Vector3 in [VIEW_GUN_POSITION, AIMED_GUN_POSITION]:
		var seen: Dictionary = await _arms_holding(at)
		assert_lt(seen.fist.distance_to(seen.model.to_global(WeaponModels.GRIP)), 0.015, "the grip in the right fist, held at %s" % at)
		# The left hand where the aim has it against the right.
		var arms: Node3D = seen.arms
		var weapon: Transform3D = arms._skeleton.global_transform.affine_inverse() * seen.gun.global_transform
		var left: Vector3 = arms._skeleton.global_transform * (weapon * SOLDIER._in_hand.affine_inverse() * SOLDIER._left_from_right).origin
		assert_lt(left.distance_to(seen.LeftHand.origin), 0.005, "the left as the aim holds a rifle, held at %s" % at)


func test_where_the_arms_are_cut_from_the_body_is_out_of_view_aimed_or_not() -> void:
	if not SOLDIER.available():
		pending("The licensed assets aren't checked out.")
		return
	# The arms are cut where they meet the chest, past the collarbones: those
	# are below the bottom of the widest view a player can choose.
	var widest: float = Settings.RANGES["fov"][1]
	for at: Vector3 in [VIEW_GUN_POSITION, AIMED_GUN_POSITION]:
		var seen: Dictionary = await _arms_holding(at)
		for side in ["Left", "Right"]:
			var collar: Vector3 = seen[side + "Collar"]
			assert_gt(rad_to_deg(atan2(-collar.y, -collar.z)), widest / 2.0, "%s collarbone below the view, held at %s" % [side, at])
