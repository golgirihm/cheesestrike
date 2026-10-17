extends GutTest
## What a character is doing, from how its player moves.

const CHARACTER := preload("res://player/soldier_character.gd")
const PLAYER := preload("res://player/player.tscn")
const M := preload("res://player/character_motion.gd")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


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


func test_the_arms_do_the_most_pressing_thing() -> void:
	assert_eq(M.action({}), M.NONE)
	assert_eq(M.action({M.AIM: true}), M.AIM)
	assert_eq(M.action({M.AIM: true, M.FIRE: true}), M.FIRE)
	assert_eq(M.action({M.AIM: true, M.RELOAD: true}), M.RELOAD)
	assert_eq(M.action({M.RELOAD: true, M.THROW: true}), M.THROW)
	assert_eq(M.action({M.THROW: true, M.HURT: true, M.AIM: true}), M.HURT)
	assert_eq(M.action({M.AIM: false, M.HURT: false}), M.NONE)
	for kind: String in M.ACTION_TIME:
		assert_has(M.ACTIONS, kind)


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


func test_aiming_or_reloading_lasts_while_it_is_so() -> void:
	character.set_holding(true, false)
	character._process(0.016)
	assert_eq(character.action(), M.AIM)
	character.set_holding(false, true)
	character._process(0.016)
	assert_eq(character.action(), M.RELOAD)
	character.set_holding(false, false)
	character._process(0.016)
	assert_eq(character.action(), M.NONE)


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
