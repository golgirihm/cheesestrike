extends GutTest
## What a player sees of their own arms (player/first_person_arms.gd): the
## soldier's arms alone, both hands on the weapon in view.

const ARMS := preload("res://player/first_person_arms.gd")
const SOLDIER := preload("res://player/soldier_character.gd")
## As the player's scene holds the weapon in view (player.gd).
const VIEW_GUN_POSITION := Vector3(0.19, -0.18, -0.44)
const VIEW_GUN_SCALE := 0.6


func test_a_weapon_carries_no_drawn_hands_or_arms() -> void:
	var model := WeaponModels.build("blaster")
	autofree(model)
	for part in ["Hand", "OffHand", "ViewArmRight", "ViewArmLeft"]:
		assert_null(model.get_node_or_null(part), "%s: the soldier's own hands hold it" % part)
	assert_not_null(model.get_node_or_null("Muzzle"), "the weapon itself is all there")


func test_a_players_own_view_shows_their_arms_on_the_weapon() -> void:
	var player: CharacterBody3D = preload("res://player/player.tscn").instantiate()
	player.name = "1"
	add_child_autofree(player)
	await wait_process_frames(1)
	assert_not_null(player._arms, "their own")
	assert_eq(player._arms.get_parent(), player.head)


func test_only_its_arms_show_and_take_the_players_colour() -> void:
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
	for at: Vector3 in [VIEW_GUN_POSITION, AIMED_GUN_POSITION]:
		var seen: Dictionary = await _arms_holding(at)
		assert_lt(seen.fist.distance_to(seen.model.to_global(WeaponModels.GRIP)), 0.015, "the grip in the right fist, held at %s" % at)
		# The left hand where the aim has it against the right.
		var arms: Node3D = seen.arms
		var weapon: Transform3D = arms._skeleton.global_transform.affine_inverse() * seen.gun.global_transform
		var hold: Dictionary = SOLDIER.hold("rifle")
		var left: Vector3 = arms._skeleton.global_transform * (weapon * (hold.in_hand as Transform3D).affine_inverse() * (hold.left as Transform3D)).origin
		assert_lt(left.distance_to(seen.LeftHand.origin), 0.005, "the left as the aim holds a rifle, held at %s" % at)


func test_where_the_arms_are_cut_from_the_body_is_out_of_view_aimed_or_not() -> void:
	# The arms are cut where they meet the chest, past the collarbones: those
	# are below the bottom of the widest view a player can choose.
	var widest: float = Settings.RANGES["fov"][1]
	for at: Vector3 in [VIEW_GUN_POSITION, AIMED_GUN_POSITION]:
		var seen: Dictionary = await _arms_holding(at)
		for side in ["Left", "Right"]:
			var collar: Vector3 = seen[side + "Collar"]
			assert_gt(rad_to_deg(atan2(-collar.y, -collar.z)), widest / 2.0, "%s collarbone below the view, held at %s" % [side, at])


func test_a_pistol_in_view_is_held_as_the_pistol_aim_holds_it() -> void:
	var head := Node3D.new()
	add_child_autofree(head)
	var gun := Node3D.new()
	gun.position = VIEW_GUN_POSITION
	gun.scale = Vector3.ONE * VIEW_GUN_SCALE
	head.add_child(gun)
	var model := WeaponModels.build("pistol")
	gun.add_child(model)
	var arms: Node3D = ARMS.new(VIEW_GUN_SCALE)
	head.add_child(arms)
	arms.hold(gun)
	var fist := {}
	var shoulder: int = arms._skeleton.find_bone("LeftShoulder")
	arms._skeleton.skeleton_updated.connect(func() -> void:
		fist.at = arms._skeleton.global_transform * arms._fist_now()
		fist.shoulder = arms._skeleton.get_bone_global_pose(shoulder).basis.get_scale().x)
	await wait_process_frames(10)
	assert_eq(arms._player.current_animation, SOLDIER.GRIPS.pistol)
	# Near enough: the fingers move a little after the first frame, which
	# the hold is worked out from.
	assert_lt((fist.at as Vector3).distance_to(model.to_global(WeaponModels.held_at("pistol"))), 0.025, "the grip in the right fist")
	assert_true(arms._hide_left.active, "the left arm, with nothing to hold, out of view")
	assert_lt(fist.shoulder, 0.01)


func test_a_pair_of_pistols_in_view_is_held_one_in_each_hand() -> void:
	var head := Node3D.new()
	add_child_autofree(head)
	var gun := Node3D.new()
	gun.position = VIEW_GUN_POSITION
	gun.scale = Vector3.ONE * VIEW_GUN_SCALE
	head.add_child(gun)
	var model := WeaponModels.build("pistols")
	gun.add_child(model)
	var arms: Node3D = ARMS.new(VIEW_GUN_SCALE)
	head.add_child(arms)
	arms.hold(gun)
	var left := {}
	arms._skeleton.skeleton_updated.connect(func() -> void: left.hand = arms._skeleton.global_transform * arms._skeleton.get_bone_global_pose(arms._skeleton.find_bone("LeftHand")))
	await wait_process_frames(10)
	assert_eq(arms._player.current_animation, SOLDIER.GRIPS.dual)
	assert_false(arms._hide_left.active, "both arms in view")
	var second := model.get_node("LeftPistol") as Node3D
	var held: Transform3D = ARMS.as_held(arms._skeleton.global_transform.affine_inverse() * second.global_transform, "pistols")
	var wanted: Transform3D = arms._skeleton.global_transform * held * SOLDIER.left_in_hand("pistols").affine_inverse()
	assert_lt((left.hand as Transform3D).origin.distance_to(wanted.origin), 0.01, "the left hand on the other pistol")
