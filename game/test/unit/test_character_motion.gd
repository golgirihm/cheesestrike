extends GutTest
## What a character is doing, from how its player moves, and how its limbs
## show it.

const CHARACTER := preload("res://player/cheese_character.gd")
const PLAYER := preload("res://player/player.tscn")
const M := preload("res://player/character_motion.gd")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


func _named(prefix: String) -> Array:
	return character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with(prefix))


## Lets the limbs settle into the pose they are heading for.
func _settle() -> void:
	for frame in 40:
		character._process(1.0 / 60.0)


func test_each_way_of_moving_is_its_own_state() -> void:
	var run := Movement.WALK_SPEED * Movement.RUN_FACTOR
	assert_eq(M.state(0.0, 0.0, false), M.IDLE)
	assert_eq(M.state(Movement.WALK_SPEED, 0.0, false), M.WALK)
	assert_eq(M.state(run, 0.0, false), M.RUN)
	assert_eq(M.state(0.0, 0.0, false, 1.0), M.CROUCH)
	assert_eq(M.state(Movement.WALK_SPEED * Movement.CROUCH_FACTOR, 0.0, false, 1.0), M.CROUCH_WALK)
	assert_eq(M.state(run, 0.0, false, 1.0, true), M.SLIDE)
	assert_eq(M.state(run, 5.0, true), M.JUMP)
	assert_eq(M.state(run, -5.0, true), M.FALL)
	assert_eq(M.state(0.0, 0.0, false, 0.0, false, 0.05), M.LAND)
	assert_eq(M.state(0.0, 0.0, false, 0.0, false, M.LAND_TIME + 0.01), M.IDLE, "a landing is over quickly")


func test_being_in_the_air_comes_before_everything_and_a_slide_before_a_crouch() -> void:
	assert_eq(M.state(9.0, 3.0, true, 1.0, true, 0.0), M.JUMP)
	assert_eq(M.state(9.0, 0.0, false, 1.0, true, 0.0), M.SLIDE)
	assert_eq(M.state(0.0, 0.0, false, 1.0, false, 0.0), M.LAND, "landing in a crouch still lands")


func test_every_state_has_a_pose_with_every_part() -> void:
	var parts: Array = M.POSES[M.IDLE].keys()
	for state: String in [M.IDLE, M.WALK, M.RUN, M.CROUCH, M.CROUCH_WALK, M.SLIDE, M.JUMP, M.FALL, M.LAND]:
		assert_true(M.POSES.has(state), state)
		assert_eq(M.POSES[state].keys(), parts, state)
	for part: String in parts:
		assert_eq(M.POSES[M.IDLE][part], 0.0, "standing still, %s is at rest" % part)


func test_the_free_arm_does_the_most_pressing_thing() -> void:
	assert_eq(M.action({}), M.NONE)
	assert_eq(M.action({M.AIM: true}), M.AIM)
	assert_eq(M.action({M.AIM: true, M.FIRE: true}), M.FIRE)
	assert_eq(M.action({M.AIM: true, M.RELOAD: true}), M.RELOAD)
	assert_eq(M.action({M.RELOAD: true, M.THROW: true}), M.THROW)
	assert_eq(M.action({M.THROW: true, M.HURT: true, M.AIM: true}), M.HURT)
	assert_eq(M.action({M.AIM: false, M.HURT: false}), M.NONE)
	for kind: String in M.ACTION_TIME:
		assert_has(M.ACTIONS, kind)


func test_limbs_blend_toward_a_pose_rather_than_snapping() -> void:
	assert_eq(M.blend(0.0), 0.0)
	assert_between(M.blend(1.0 / 60.0), 0.05, 0.5, "part of the way in one frame")
	assert_gt(M.blend(0.5), 0.99, "there within half a second")


func test_the_character_takes_up_the_state_its_motion_gives() -> void:
	var run := Movement.WALK_SPEED * Movement.RUN_FACTOR
	character.set_motion(0.0, false)
	character._process(0.016)
	assert_eq(character.state(), M.IDLE)
	character.set_motion(run, false)
	character._process(0.016)
	assert_eq(character.state(), M.RUN)
	character.set_motion(run, true, 6.0)
	character._process(0.016)
	assert_eq(character.state(), M.JUMP)
	character.set_motion(run, true, -6.0)
	character._process(0.016)
	assert_eq(character.state(), M.FALL)
	character.set_motion(run, false)
	character._process(0.016)
	assert_eq(character.state(), M.LAND, "touching down")
	character._process(M.LAND_TIME)
	assert_eq(character.state(), M.RUN, "and off again")
	character.set_crouch(1.0)
	character.set_motion(run, false, 0.0, true)
	character._process(0.016)
	assert_eq(character.state(), M.SLIDE)
	character.set_motion(0.0, false)
	character._process(0.016)
	assert_eq(character.state(), M.CROUCH)


func test_running_strides_further_and_pumps_the_arm_more_than_walking() -> void:
	var legs := _named("Leg")
	var arm: Node3D = _named("Arm")[0]
	var reach := func(speed: float) -> Vector2:
		character.set_motion(speed, false)
		_settle()
		var furthest := Vector2.ZERO
		for frame in 60:
			character._process(1.0 / 60.0)
			furthest = Vector2(maxf(furthest.x, absf(legs[0].rotation.x)), maxf(furthest.y, absf(arm.rotation.x)))
		return furthest
	var walking: Vector2 = reach.call(Movement.WALK_SPEED)
	var running: Vector2 = reach.call(Movement.WALK_SPEED * Movement.RUN_FACTOR)
	assert_gt(running.x, walking.x, "a longer stride")
	assert_gt(running.y, walking.y, "and the arm pumps")
	assert_gt(walking.x, 0.3)


func test_a_slide_throws_the_legs_forward() -> void:
	character.set_crouch(1.0)
	character.set_motion(9.0, false, 0.0, true)
	_settle()
	for leg: Node3D in _named("Leg"):
		assert_lt(leg.rotation.x, -1.0, "out in front")


func test_landing_and_crouching_spread_the_legs() -> void:
	var legs := _named("Leg")
	character.set_motion(0.0, true, -5.0)
	_settle()
	character.set_motion(0.0, false)
	character._process(0.05)
	assert_eq(character.state(), M.LAND)
	# Outward: the left leg to the left, the right to the right.
	assert_lt(legs[0].rotation.z, -0.05)
	assert_gt(legs[1].rotation.z, 0.05)
	_settle()
	assert_almost_eq(legs[0].rotation.z, 0.0, 0.01, "and together again")
	character.set_crouch(1.0)
	_settle()
	assert_lt(legs[0].rotation.z, -0.2)


func test_the_limbs_move_and_the_wedge_does_not() -> void:
	var wedge: MeshInstance3D = character.get_node("Wedge")
	var rest := wedge.global_transform
	var run := Movement.WALK_SPEED * Movement.RUN_FACTOR
	for motion: Array in [[run, false, 0.0, false], [run, true, 6.0, false], [run, true, -6.0, false], [0.0, false, 0.0, false]]:
		character.set_motion(motion[0], motion[1], motion[2], motion[3])
		_settle()
		assert_true(wedge.global_transform.is_equal_approx(rest), "%s: what is hit stays where it is tested" % [motion])


func test_the_free_arm_steadies_the_weapon_to_aim_or_reload() -> void:
	var arm: Node3D = _named("Arm")[0]
	_settle()
	assert_almost_eq(arm.rotation.x, 0.0, 0.01, "hanging at rest")
	character.set_holding(true, false)
	character._process(0.016)
	assert_eq(character.action(), M.AIM)
	_settle()
	assert_lt(arm.rotation.x, -1.0, "brought forward to the weapon")
	character.set_holding(false, true)
	character._process(0.016)
	assert_eq(character.action(), M.RELOAD)
	character.set_holding(false, false)
	_settle()
	assert_eq(character.action(), M.NONE)
	assert_almost_eq(arm.rotation.x, 0.0, 0.01)


func test_momentary_actions_pass() -> void:
	for kind: String in M.ACTION_TIME:
		character.act(kind)
		character._process(0.016)
		assert_eq(character.action(), kind)
		character._process(M.ACTION_TIME[kind] + 0.01)
		character._process(0.016)
		assert_eq(character.action(), M.NONE, "%s is over" % kind)
	character.act(M.AIM)
	character._process(0.016)
	assert_eq(character.action(), M.NONE, "aiming lasts while it is so, not for a moment")


func test_a_throw_takes_the_arm_back_then_through() -> void:
	var back := M.arm(M.THROW, 0.0, Vector3.ZERO)
	var through := M.arm(M.THROW, M.ACTION_TIME[M.THROW], Vector3.ZERO)
	assert_gt(back.x, 2.0, "wound up behind the shoulder")
	assert_lt(through.x, -0.5, "and followed through in front")
	var rest := Vector3(0.1, 0.0, -0.35)
	assert_eq(M.arm(M.NONE, 0.3, rest), rest)


func test_being_hurt_firing_and_striking_show_on_the_character() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "2"
	add_child_autofree(player)
	await wait_process_frames(1)
	player.kick()
	player.body._process(0.016)
	assert_eq(player.body.action(), M.FIRE)
	player.swing_knife()
	player.body._process(0.016)
	assert_eq(player.body.action(), M.MELEE)
	player.body.grimace()
	player.body._process(0.016)
	assert_eq(player.body.action(), M.HURT)


func test_what_others_are_doing_reaches_their_characters() -> void:
	var other: CharacterBody3D = PLAYER.instantiate()
	other.name = "2"
	add_child_autofree(other)
	await wait_process_frames(1)
	other.net_state = PackedFloat32Array([0, 0, 0, 0, 0, 0, other.POSE_AIMING])
	other._animate(0.05)
	other.body._process(0.016)
	assert_eq(other.body.action(), M.AIM)
	other.net_state = PackedFloat32Array([0, 0, 0, 0, 0, 0, other.POSE_RELOADING])
	other._animate(0.05)
	other.body._process(0.016)
	assert_eq(other.body.action(), M.RELOAD)
	other.net_state = PackedFloat32Array([0, 0, 0, 0, 0, 0, other.POSE_SLIDING | other.POSE_CROUCHING])
	other._animate(0.3)
	other._animate(0.05)
	other.body._process(0.016)
	assert_eq(other.body.state(), M.SLIDE)
