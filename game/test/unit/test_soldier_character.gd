extends GutTest
## The realistic soldier (player/soldier_character.gd): how it paces its legs
## and keeps its animations in place, which hold anywhere, and, where its
## licensed model is checked out, that it shows what the player is doing.

const SOLDIER := preload("res://player/soldier_character.gd")

var soldier: Node3D


func before_each() -> void:
	soldier = null
	if SOLDIER.available():
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


func test_the_web_build_leaves_it_out() -> void:
	var presets := ConfigFile.new()
	assert_eq(presets.load("res://export_presets.cfg"), OK)
	var web := ""
	for section in presets.get_sections():
		if presets.get_value(section, "platform", "") == "Web":
			web = section
	assert_ne(web, "", "there is a web build")
	assert_has(str(presets.get_value(web, "exclude_filter", "")).split(", "), "licensed/*")


func test_its_legs_play_what_the_player_is_doing() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
	soldier.knock("blaster", Vector3(0, 0, 1))
	soldier.set_in_play(false)
	_frames(15)
	assert_true(soldier.visible, "still there, falling")
	assert_almost_eq(soldier.scale.x, soldier.scale.y, 0.0001, "not melting")
	_frames(int(SOLDIER.FALL_TIME * 30.0))
	assert_false(soldier.visible, "then gone")


func test_it_keeps_its_cap_and_sunglasses_whatever_hat_is_chosen() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
	for hat: String in Looks.HATS:
		soldier.wear(Looks.pack("swiss", hat, "none"))
		_frames(2)
		assert_true(soldier._cap_mesh.visible, "its cap, with a %s hat chosen" % hat)
		assert_true(soldier._model.find_child("Glass_LP1").visible, "and its sunglasses")
		if soldier._hat != null:
			assert_false(soldier._hat.visible, "and no %s hat" % hat)


func test_players_are_the_realistic_soldier_wherever_it_can_be_shown() -> void:
	Characters.realistic = true
	var body := Characters.make()
	Characters.realistic = false
	add_child_autofree(body)
	if SOLDIER.available():
		assert_eq(body.get_script(), SOLDIER)
	else:
		assert_eq(body.get_script(), Characters.DRAWN, "and the drawn one elsewhere")
	var drawn := Characters.make()
	add_child_autofree(drawn)
	assert_eq(drawn.get_script(), Characters.DRAWN, "unless it is turned off, as for the tests")


func test_a_player_built_drawn_is_given_the_realistic_body_and_nothing_of_the_drawn_one() -> void:
	if not SOLDIER.available():
		pending("The licensed assets aren't checked out.")
		return
	var player := Node3D.new()
	add_child_autofree(player)
	var drawn: Node3D = Characters.DRAWN.new()
	drawn.name = "Body"
	player.add_child(drawn)
	player.add_child(Node3D.new())
	Characters.realistic = true
	var body := Characters.swap(drawn)
	Characters.realistic = false
	assert_eq(body.get_script(), SOLDIER)
	assert_eq(player.get_node("Body"), body, "under the same name")
	assert_eq(body.get_index(), 0, "and in the same place")
	assert_null(body.find_child("Torso", false), "without the drawn soldier's torso")


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
		soldier.steady_with(soldier.to_local((model.get_node("OffHand") as Node3D).global_position))
		await wait_process_frames(1)


func test_it_holds_its_weapon_in_its_right_fist_pointing_where_it_looks() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
	assert_false((model.get_node("Hand") as Node3D).visible, "its own hand on the grip, not the drawn soldier's mitten")


func test_a_new_weapon_shows_no_mitten_and_an_emote_puts_the_weapon_away() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
	assert_false((second.get_node("Hand") as Node3D).visible, "no mitten on the new one either")
	soldier.emote(Emotes.CHEER)
	gun.visible = true
	soldier.hold_weapon(gun)
	assert_false(gun.visible, "put away for the cheer")


func test_others_dont_see_the_arms_cut_for_its_players_own_view() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
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
