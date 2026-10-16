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


func test_its_legs_blend_from_standing_through_walking_to_running() -> void:
	assert_eq(SOLDIER.stride(0.0), Vector2(0.0, 1.0), "standing still")
	var walk_speed := SOLDIER.WALK_PACE * BodyShape.SIZE * SOLDIER.model_scale()
	var run_speed := SOLDIER.RUN_PACE * BodyShape.SIZE * SOLDIER.model_scale()
	assert_almost_eq(SOLDIER.stride(walk_speed).x, 1.0, 0.001, "walking at the walk's own pace")
	assert_almost_eq(SOLDIER.stride(run_speed).x, 2.0, 0.001, "running at the run's own pace")
	var last := -1.0
	for speed in range(0, 20):
		var gait: Vector2 = SOLDIER.stride(speed * 0.5)
		assert_gte(gait.x, last, "further along at %s m/s" % (speed * 0.5))
		last = gait.x


func test_faster_than_its_run_it_runs_faster_so_its_feet_keep_pace() -> void:
	var run_speed := SOLDIER.RUN_PACE * BodyShape.SIZE * SOLDIER.model_scale()
	assert_almost_eq(SOLDIER.stride(Movement.WALK_SPEED * Movement.RUN_FACTOR).y, Movement.WALK_SPEED * Movement.RUN_FACTOR / run_speed, 0.001)
	assert_eq(SOLDIER.stride(run_speed * 0.5).y, 1.0, "and no faster below it")


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
	assert_almost_eq((soldier._tree.get("parameters/ground/blend_position") as Vector2).y, 2.0, 0.001, "running ahead")
	soldier.set_heading(Vector2.LEFT)
	_frames(1)
	assert_almost_eq((soldier._tree.get("parameters/ground/blend_position") as Vector2).x, -2.0, 0.001, "and to the left")
	soldier.set_motion(0.0, true, 5.0)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/legs/current_state"), "jump", "and in the air")


func test_its_upper_body_acts_out_what_the_arms_do() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
	_frames(10)
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 0.0, 0.001, "nothing but holding the weapon")
	soldier.set_holding(false, true)
	_frames(10)
	assert_eq(soldier._tree.get("parameters/acts/current_state"), "reload")
	assert_almost_eq(soldier._tree.get("parameters/upper/blend_amount"), 1.0, 0.001)


func test_its_hands_go_to_the_weapon() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
	var grip := Vector3(0.3, 0.6, -0.3)
	soldier.reach_for(grip)
	soldier.steady_with(grip + Vector3(-0.1, 0.0, -0.2))
	_frames(10)
	assert_eq(soldier._grip.position, grip)
	assert_almost_eq(soldier._right_arm.influence, 1.0, 0.001, "the weapon hand on the grip")
	assert_almost_eq(soldier._left_arm.influence, 1.0, 0.001, "and the other steadying it")
	soldier.act(CharacterMotion.THROW)
	_frames(6)
	assert_almost_eq(soldier._left_arm.influence, 0.0, 0.001, "until it has something else to do")


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


func test_a_hat_takes_the_place_of_its_cap() -> void:
	if soldier == null:
		pending("The licensed assets aren't checked out.")
		return
	soldier.wear(Looks.pack("swiss", "crown", "none"))
	_frames(2)
	assert_false(soldier._cap_mesh.visible)
	assert_gt(soldier._hat.position.y, BodyShape.HEAD_HEIGHT, "on top of the head")
	soldier.wear(Looks.pack("swiss", "none", "none"))
	assert_true(soldier._cap_mesh.visible)


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
