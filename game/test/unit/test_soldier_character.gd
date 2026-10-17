extends GutTest
## The soldier everyone sees (player/soldier_character.gd): how it paces its
## legs and keeps its animations in place, and how it shows what the player
## is doing.

const SOLDIER := preload("res://player/soldier_character.gd")

var soldier: Node3D


func before_each() -> void:
	soldier = SOLDIER.new()
	add_child_autofree(soldier)


func _frames(count: int, delta := 1.0 / 30.0) -> void:
	for frame in count:
		soldier._process(delta)
		soldier._tree.advance(delta)


func test_its_legs_blend_from_standing_through_walking_and_running_to_sprinting() -> void:
	assert_eq(SOLDIER.stride(0.0), Vector2(0.0, 1.0), "standing still")
	var arena := BodyShape.SIZE * SOLDIER.model_scale()
	assert_almost_eq(SOLDIER.stride(SOLDIER.WALK_PACE * arena).x, 1.0, 0.001, "walking at the walk's own pace")
	assert_almost_eq(SOLDIER.stride(SOLDIER.RUN_PACE * arena).x, 2.0, 0.001, "running at the run's own pace")
	assert_almost_eq(SOLDIER.stride(SOLDIER.SPRINT_PACE * arena).x, 3.0, 0.001, "sprinting at the sprint's own pace")
	assert_between(SOLDIER.stride(Movement.WALK_SPEED).x, 1.0, 2.0, "a player walks between its walk and its run")
	assert_between(SOLDIER.stride(Movement.WALK_SPEED * Movement.RUN_FACTOR).x, 2.0, 3.0, "and runs between its run and its sprint")
	var last := -1.0
	for speed in range(0, 20):
		var gait: Vector2 = SOLDIER.stride(speed * 0.5)
		assert_gte(gait.x, last, "further along at %s m/s" % (speed * 0.5))
		last = gait.x


func test_faster_than_its_sprint_it_sprints_faster_so_its_feet_keep_pace() -> void:
	var sprint_speed := SOLDIER.SPRINT_PACE * BodyShape.SIZE * SOLDIER.model_scale()
	assert_almost_eq(SOLDIER.stride(sprint_speed * 1.5).y, 1.5, 0.001)
	assert_eq(SOLDIER.stride(sprint_speed * 0.5).y, 1.0, "and no faster below it")
	var crouch_speed := SOLDIER.CROUCH_PACE * BodyShape.SIZE * SOLDIER.model_scale()
	assert_eq(SOLDIER.crouched_stride(crouch_speed * 0.5), Vector2(0.5, 1.0), "crouched, halfway to its walk")
	assert_eq(SOLDIER.crouched_stride(crouch_speed * 2.0), Vector2(1.0, 2.0), "and twice as fast past it")


func test_every_gait_goes_round_and_round_and_a_jump_once() -> void:
	for name in ["walk", "run_back_left", "sprint_ahead_right", "crouch_walk_left", "idle", "jump_loop"]:
		assert_true(SOLDIER.loops(name), name)
	for name in ["jump_up", "jump_down", "reload", "knocked_out"]:
		assert_false(SOLDIER.loops(name), name)


func test_an_animation_put_in_place_keeps_its_bob_but_not_its_travel() -> void:
	var animation := Animation.new()
	var track := animation.add_track(Animation.TYPE_POSITION_3D)
	animation.track_set_path(track, NodePath("%GeneralSkeleton:Hips"))
	animation.track_insert_key(track, 0.0, Vector3(0.1, 1.0, 0.0))
	animation.track_insert_key(track, 0.5, Vector3(0.3, 0.95, 0.7))
	animation.track_insert_key(track, 1.0, Vector3(0.5, 1.0, 1.4))
	SOLDIER.in_place(animation)
	for key in 3:
		var at: Vector3 = animation.track_get_key_value(track, key)
		assert_eq(Vector2(at.x, at.z), Vector2(0.1, 0.0), "over the feet")
	assert_almost_eq((animation.track_get_key_value(track, 1) as Vector3).y, 0.95, 0.0001, "bobbing as it did")


func test_the_web_build_takes_it_along() -> void:
	var presets := ConfigFile.new()
	assert_eq(presets.load("res://export_presets.cfg"), OK)
	var web := ""
	for section in presets.get_sections():
		if presets.get_value(section, "platform", "") == "Web":
			web = section
	assert_ne(web, "", "there is a web build")
	for filter: String in str(presets.get_value(web, "exclude_filter", "")).split(", "):
		assert_false(filter.begins_with("licensed"), "the licensed assets go with it")


func test_its_legs_play_what_the_player_is_doing() -> void:
	soldier.set_motion(Movement.WALK_SPEED * Movement.RUN_FACTOR, false)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "ground")
	var running := SOLDIER.stride(Movement.WALK_SPEED * Movement.RUN_FACTOR).x
	assert_almost_eq((soldier._tree.get("parameters/ground/blend_position") as Vector2).y, running, 0.001, "running ahead")
	soldier.set_heading(Vector2.LEFT)
	_frames(1)
	assert_almost_eq((soldier._tree.get("parameters/ground/blend_position") as Vector2).x, -running, 0.001, "and to the left")
	soldier.set_motion(0.0, true, 5.0)
	_frames(3)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "jump", "leaving the ground")
	soldier.set_motion(0.0, true, -5.0)
	_frames(3)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "air", "then in the air")
	soldier.set_motion(0.0, false)
	_frames(1)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "land", "and landing")
	soldier.set_motion(Movement.WALK_SPEED * Movement.CROUCH_FACTOR, false)
	soldier.set_crouch(1.0)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "crouch", "and crouched")
	assert_gt((soldier._tree.get("parameters/crouched/blend_position") as Vector2).length(), 0.5, "walking crouched")


func test_its_upper_body_acts_out_what_the_arms_do() -> void:
	_frames(10)
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 0.0, 0.001, "with nothing else to do, carrying the weapon as the legs do")
	assert_almost_eq(soldier._spine.influence, 0.0, 0.001, "not aimed")
	soldier.set_holding(true, false)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "aim", "aiming, up at the shoulder")
	assert_almost_eq(soldier._spine.influence, 1.0, 0.001, "and pointed where it looks")
	soldier.set_holding(false, false)
	soldier.act(CharacterMotion.FIRE)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "aim", "still up a moment after firing")
	_frames(int(SOLDIER.RAISED_AFTER_FIRING * 30.0) + 5)
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 0.0, 0.001, "then carried again")
	soldier.set_holding(false, true)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "reload")
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 1.0, 0.001)


func test_its_face_shows_how_things_are_going() -> void:
	var face: MeshInstance3D = soldier._face_mesh
	var smile := face.find_blend_shape_by_name("Mouth_Smile_L")
	var squint := face.find_blend_shape_by_name("Eye_Squint_L")
	soldier.cheer()
	_frames(10)
	assert_gt(face.get_blend_shape_value(smile), 0.9, "grinning after a knock-out")
	soldier.grimace()
	_frames(10)
	assert_gt(face.get_blend_shape_value(squint), 0.9, "screwed up when hurt")
	assert_lt(face.get_blend_shape_value(smile), 0.1, "the grin gone")


func test_knocked_out_it_falls_back_whole_then_goes() -> void:
	soldier.knock("blaster", Vector3(0, 0, 1))
	soldier.set_in_play(false)
	_frames(15)
	assert_true(soldier.visible, "still there, falling")
	assert_almost_eq(soldier.scale.x, soldier.scale.y, 0.0001, "not melting")
	_frames(int(SOLDIER.MELT_TIME * 30.0))
	assert_false(soldier.visible, "then gone")


func test_it_keeps_its_cap_and_sunglasses_whatever_hat_is_chosen() -> void:
	for hat: String in Looks.HATS:
		soldier.wear(Looks.pack("swiss", hat, "none"))
		_frames(2)
		assert_true(soldier._cap_mesh.visible, "its cap, with a %s hat chosen" % hat)
		assert_true(soldier._model.find_child("Glass_LP1").visible, "and its sunglasses")
		assert_null(soldier.get_node_or_null("Hat"), "and no %s hat" % hat)


func test_everywhere_a_player_is_shown_it_is_the_soldier() -> void:
	var player: Node3D = preload("res://player/player.tscn").instantiate()
	add_child_autofree(player)
	assert_eq(player.body.get_script(), SOLDIER, "in a match")
	var props: Node3D = preload("res://effects/warmup.gd").props()
	add_child_autofree(props)
	assert_eq(props.get_node("Character").get_script(), SOLDIER, "warming up the graphics")


func test_the_spine_turns_the_weapon_round_to_where_it_is_aimed() -> void:
	var aim := preload("res://player/spine_aim.gd")
	for now: Vector3 in [Vector3(-0.6, -0.1, -0.8), Vector3.FORWARD, Vector3(0.3, 0.5, -0.4)]:
		for at: Vector3 in [Vector3.FORWARD, Vector3(0.0, 0.5, -0.8)]:
			var turned: Vector3 = aim.turn(now, at) * now.normalized()
			assert_almost_eq(turned.distance_to(at.normalized()), 0.0, 0.0001, "%s round to %s" % [now, at])


func test_the_weapon_is_aimed_no_steeper_than_a_rifle_can_be_shouldered() -> void:
	var aim := preload("res://player/spine_aim.gd")
	var sky: Vector3 = aim.pitched_at_most(Vector3(0.0, 0.99, 0.1), aim.AIM_DOWN, aim.AIM_UP)
	assert_almost_eq(asin(sky.y), aim.AIM_UP, 0.0001, "looking at the sky, as high as it goes")
	assert_gt(sky.z, 0.0, "still facing the same way")
	var feet: Vector3 = aim.pitched_at_most(Vector3(0.1, -0.99, 0.0), aim.AIM_DOWN, aim.AIM_UP)
	assert_almost_eq(asin(feet.y), -aim.AIM_DOWN, 0.0001, "and at the feet, as low")
	var level: Vector3 = aim.pitched_at_most(Vector3(0.0, 0.3, -1.0).normalized(), aim.AIM_DOWN, aim.AIM_UP)
	assert_almost_eq(level.distance_to(Vector3(0.0, 0.3, -1.0).normalized()), 0.0, 0.0001, "anywhere between, just where it looks")


# Holds a weapon as the player does, for `frames` frames, looking up by
# `gaze`.
func _hold(gun: Node3D, model: Node3D, frames: int, gaze := 0.0) -> void:
	soldier.set_gaze(gaze)
	soldier.set_holding(true, false)
	for frame in frames:
		gun.position = Vector3(1, 2, 3)
		soldier.hold_weapon(gun)
		await wait_process_frames(1)


func test_it_holds_its_weapon_in_its_right_fist_pointing_where_it_looks() -> void:
	var gun := Node3D.new()
	add_child_autofree(gun)
	gun.scale = Vector3.ONE * BodyShape.SIZE * soldier.weapon_scale()
	var model := WeaponModels.build("blaster")
	gun.add_child(model)
	var skeleton: Skeleton3D = soldier._skeleton
	var fists := {}
	var note := func() -> void:
		fists.Right = soldier.to_local(skeleton.global_transform * soldier._fist("Right"))
	skeleton.skeleton_updated.connect(note)
	for gaze in [0.0, 0.6, 1.4]:
		await _hold(gun, model, 15, gaze)
		var ahead := (soldier.global_basis.inverse() * -gun.global_basis.z).normalized()
		var pitch := minf(gaze, preload("res://player/spine_aim.gd").AIM_UP)
		assert_almost_eq(ahead.distance_to(Vector3(0.0, sin(pitch), -cos(pitch))), 0.0, 0.01, "pointing where it looks, %s up" % gaze)
		var grip := soldier.to_local(model.to_global(WeaponModels.GRIP))
		assert_lt(grip.distance_to(fists.Right), 0.01, "the grip in the right fist")
	skeleton.skeleton_updated.disconnect(note)


func test_a_new_weapon_is_held_its_own_way_and_an_emote_puts_the_weapon_away() -> void:
	var gun := Node3D.new()
	add_child_autofree(gun)
	var first := WeaponModels.build("blaster")
	gun.add_child(first)
	await _hold(gun, first, 2)
	gun.remove_child(first)
	first.free()
	var second := WeaponModels.build("shotgun")
	gun.add_child(second)
	await _hold(gun, second, 2)
	assert_eq(soldier._weapon, "shotgun", "the new one in its hands")
	soldier.emote(Emotes.CHEER)
	gun.visible = true
	soldier.hold_weapon(gun)
	assert_false(gun.visible, "put away for the cheer")


func test_others_dont_see_the_arms_cut_for_its_players_own_view() -> void:
	for name: String in SOLDIER.FIRST_PERSON:
		assert_false(soldier._model.find_child(name).visible, name)


func test_the_roll_about_the_forearm_is_picked_out_of_a_turn() -> void:
	var twist := preload("res://player/forearm_twist.gd")
	var along := Vector3(0.3, 0.2, -0.9).normalized()
	var swing := Quaternion(along.cross(Vector3.UP).normalized(), 0.4)
	var picked: Quaternion = twist.roll(swing * Quaternion(along, 1.2), along)
	assert_almost_eq(picked.get_angle(), 1.2, 0.2, "most of the roll")
	assert_almost_eq(absf(picked.get_axis().dot(along)), 1.0, 0.0001, "and only about the forearm")
	assert_almost_eq(twist.roll(swing, along).get_angle(), 0.0, 0.01, "none from a turn that only swings")


func test_a_hands_roll_turns_the_forearms_twist_bones_part_of_the_way() -> void:
	# A forearm along +Y, with its two twist bones, and a hand at its end
	# rolled about it, as an animation would leave it.
	var skeleton := Skeleton3D.new()
	add_child_autofree(skeleton)
	for bone: Array in [["LeftLowerArm", -1, 0.0], ["CC_Base_L_ForearmTwist01", 0, 0.1], ["CC_Base_L_ForearmTwist02", 1, 0.1], ["LeftHand", 0, 0.3]]:
		var index := skeleton.add_bone(bone[0])
		skeleton.set_bone_parent(index, bone[1])
		skeleton.set_bone_rest(index, Transform3D(Basis(), Vector3(0, bone[2], 0)))
	skeleton.reset_bone_poses()
	skeleton.set_bone_pose_rotation(skeleton.find_bone("LeftHand"), Quaternion(Vector3.UP, 1.5))
	skeleton.add_child(preload("res://player/forearm_twist.gd").new())
	var rolled := {}
	var note := func() -> void:
		for name in ["CC_Base_L_ForearmTwist01", "CC_Base_L_ForearmTwist02", "LeftHand"]:
			rolled[name] = skeleton.get_bone_global_pose(skeleton.find_bone(name)).basis.get_rotation_quaternion().get_angle()
	skeleton.skeleton_updated.connect(note)
	await wait_process_frames(3)
	skeleton.skeleton_updated.disconnect(note)
	assert_almost_eq(rolled.LeftHand, 1.5, 0.001, "the hand as the animation has it")
	assert_almost_eq(rolled.CC_Base_L_ForearmTwist02, 1.0, 0.001, "two thirds of its roll near the wrist")
	assert_almost_eq(rolled.CC_Base_L_ForearmTwist01, 0.5, 0.001, "a third near the elbow")


func test_what_can_be_hit_is_where_it_is_drawn_standing_and_kneeling() -> void:
	for crouch in [0.0, 1.0]:
		soldier.set_crouch(crouch)
		await wait_process_frames(20)
		var skeleton: Skeleton3D = soldier._skeleton
		var at := func(bone: String) -> Vector3:
			return soldier.to_local(skeleton.global_transform * skeleton.get_bone_global_pose(skeleton.find_bone(bone)).origin)
		var eyes: Vector3 = (at.call("LeftEye") + at.call("RightEye")) / 2.0
		assert_almost_eq(eyes.y, BodyShape.head_height(crouch), 0.03, "its eyes where its player sees from, crouched %s" % crouch)
		var parts := BodyShape.parts(crouch)
		var head: AABB = parts[0][0]
		var torso: AABB = parts[1][0]
		assert_true(head.grow(0.02).has_point(at.call("Head")), "its head in the head, crouched %s" % crouch)
		assert_true(torso.grow(0.02).has_point(at.call("Chest")), "its chest in the torso, crouched %s" % crouch)
		if crouch == 0.0:
			for side in ["Left", "Right"]:
				var leg: AABB = (parts[2][0] as AABB).merge(parts[3][0])
				assert_true(leg.grow(0.05).has_point(at.call(side + "LowerLeg")), "%s knee in the legs" % side)


func test_a_pistol_is_held_out_in_both_hands_and_a_rifle_carried() -> void:
	assert_eq(SOLDIER.grip_of("pistol"), "pistol")
	assert_eq(SOLDIER.grip_of("blaster"), "rifle", "anything else as a rifle")
	assert_eq(WeaponModels.build("pistol").get_meta("weapon"), "pistol", "a weapon's model says which it is")
	var gun := Node3D.new()
	add_child_autofree(gun)
	var rifle := WeaponModels.build("blaster")
	gun.add_child(rifle)
	soldier.hold_weapon(gun)
	_frames(10)
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 0.0, 0.001, "a rifle carried")
	gun.remove_child(rifle)
	rifle.free()
	gun.add_child(WeaponModels.build("pistol"))
	soldier.hold_weapon(gun)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "pistol_aim", "a pistol held out")
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 1.0, 0.001)
	assert_eq(soldier._spine.in_hand, SOLDIER.in_hand("pistol"), "in the pistol's grip")


func test_a_hand_closes_round_each_weapons_own_grip() -> void:
	assert_eq(WeaponModels.held_at("blaster"), WeaponModels.GRIP, "most at the usual grip")
	assert_lt(WeaponModels.held_at("pistol").z, WeaponModels.GRIP.z, "a pistol's further forward, under its slide")
	assert_gt(WeaponModels.view_offset("pistol").z, 0.0, "and a pistol nearer the eye in its holder's view")
	assert_eq(WeaponModels.view_offset("blaster"), Vector3.ZERO)
	var skeleton: Skeleton3D = soldier._skeleton
	var fists := {}
	var note := func() -> void:
		for side in ["Right", "Left"]:
			fists[side] = soldier.to_local(skeleton.global_transform * soldier._fist(side))
	skeleton.skeleton_updated.connect(note)
	for weapon: String in ["pistol", "pistols"]:
		var gun := Node3D.new()
		add_child_autofree(gun)
		gun.scale = Vector3.ONE * BodyShape.SIZE * soldier.weapon_scale()
		var model := WeaponModels.build(weapon)
		gun.add_child(model)
		await _hold(gun, model, 15)
		var grip := soldier.to_local(model.to_global(WeaponModels.held_at(weapon)))
		# Near enough: the fingers move a little as the hold is worked out
		# from the first frame.
		assert_lt(grip.distance_to(fists.Right), 0.02, "%s's grip in the right fist" % weapon)
		var second := model.get_node_or_null("LeftPistol") as Node3D
		if second != null:
			var other := soldier.to_local(second.to_global(WeaponModels.held_at(weapon)))
			assert_lt(other.distance_to(fists.Left), 0.02, "the other's in the left")
	skeleton.skeleton_updated.disconnect(note)


func test_a_pair_of_pistols_is_carried_low_and_aimed_out_side_by_side() -> void:
	assert_eq(SOLDIER.grip_of("pistols"), "dual")
	assert_eq(SOLDIER.grip_of("burratas"), "dual", "the Dual Burratas too")
	assert_not_null(WeaponModels.build("pistols").get_node_or_null("LeftPistol"), "the second its own node, for the left hand")
	var gun := Node3D.new()
	add_child_autofree(gun)
	gun.scale = Vector3.ONE * BodyShape.SIZE * soldier.weapon_scale()
	var model := WeaponModels.build("pistols")
	gun.add_child(model)
	soldier.hold_weapon(gun)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "dual_carry", "carried")
	assert_eq(soldier._dual_aim, 0.0, "not reached out")
	var second := model.get_node("LeftPistol") as Node3D
	for gaze in [0.0, 0.5]:
		await _hold(gun, model, 15, gaze)
		assert_eq(soldier._dual_aim, 1.0, "aimed, reached out")
		assert_eq(soldier._spine.influence, 0.0, "by the arms alone")
		for pistol: Node3D in [model, second]:
			var ahead := (soldier.global_basis.inverse() * -pistol.global_basis.z).normalized()
			assert_almost_eq(ahead.distance_to(Vector3(0.0, sin(gaze), -cos(gaze))), 0.0, 0.05, "%s pointing where it looks, %s up" % [pistol.name, gaze])
		var right := soldier.to_local(model.global_position)
		var left := soldier.to_local(second.global_position)
		assert_gt(right.x - left.x, 0.05, "side by side, a little apart")
		assert_almost_eq(right.z, left.z, 0.05, "neither ahead of the other")
		assert_lt(right.z, -0.15, "out in front")


func test_a_pair_of_pistols_is_gripped_in_both_hands_as_the_right_grips_one() -> void:
	var from := Animation.new()
	var turn := Quaternion(Vector3(0.2, 0.3, 0.9).normalized(), 0.7)
	for finger: String in SOLDIER.FINGERS:
		var track := from.add_track(Animation.TYPE_ROTATION_3D)
		from.track_set_path(track, NodePath("%GeneralSkeleton:Right" + finger))
		from.rotation_track_insert_key(track, 0.0, turn)
	var carry := Animation.new()
	var loose := carry.add_track(Animation.TYPE_ROTATION_3D)
	carry.track_set_path(loose, NodePath("%GeneralSkeleton:LeftIndexProximal"))
	carry.rotation_track_insert_key(loose, 0.0, Quaternion.IDENTITY)
	carry.rotation_track_insert_key(loose, 1.0, Quaternion(Vector3.UP, 0.5))
	SOLDIER.gripping(carry, from)
	for finger: String in SOLDIER.FINGERS:
		var right := carry.find_track(NodePath("%GeneralSkeleton:Right" + finger), Animation.TYPE_ROTATION_3D)
		var left := carry.find_track(NodePath("%GeneralSkeleton:Left" + finger), Animation.TYPE_ROTATION_3D)
		assert_eq(carry.track_get_key_count(left), 1, "%s held still" % finger)
		assert_true(carry.rotation_track_interpolate(right, 0.5).is_equal_approx(turn), "the right as it grips")
		assert_true(carry.rotation_track_interpolate(left, 0.5).is_equal_approx(Quaternion(turn.x, -turn.y, -turn.z, turn.w)), "the left mirroring it")
