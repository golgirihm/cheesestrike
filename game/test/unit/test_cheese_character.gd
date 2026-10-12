extends GutTest

const CHARACTER := preload("res://player/cheese_character.gd")
const PLAYER := preload("res://player/player.tscn")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


func _named(prefix: String) -> Array:
	return character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with(prefix))


func test_it_has_a_body_two_legs_and_two_arms() -> void:
	assert_not_null(character.get_node_or_null("Wedge"))
	assert_eq(_named("Leg").size(), 2)
	assert_eq(_named("Arm").size(), 2)
	assert_gt(_named("Hole").size(), 0, "it's Swiss enough to have holes")


func test_it_fits_inside_the_players_collision_capsule() -> void:
	var bounds := AABB()
	var first := true
	for mesh: MeshInstance3D in character.find_children("*", "MeshInstance3D", true, false):
		var box: AABB = mesh.global_transform * mesh.get_aabb()
		bounds = box if first else bounds.merge(box)
		first = false
	assert_gte(bounds.position.y, -0.01, "not below the feet")
	assert_lte(bounds.end.y, BodyShape.HEIGHT, "not above the head")
	assert_lte(maxf(absf(bounds.position.x), absf(bounds.end.x)), BodyShape.RADIUS + 0.05)
	assert_lte(maxf(absf(bounds.position.z), absf(bounds.end.z)), BodyShape.RADIUS + 0.05)
	# Big enough to see and to match what can be hit.
	assert_gte(bounds.end.y, BodyShape.HEIGHT * 0.75, "fills most of the capsule's height")


func test_the_players_capsule_head_and_eyes_match_the_body_shots_are_tested_against() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "2"
	add_child_autofree(player)
	var shape: CapsuleShape3D = player.collider.shape
	assert_almost_eq(shape.radius, BodyShape.RADIUS, 0.001)
	assert_almost_eq(shape.height, BodyShape.HEIGHT, 0.001)
	assert_almost_eq(player.collider.position.y, BodyShape.HEIGHT / 2.0, 0.001, "standing on the feet")
	assert_between(player.head.position.y, BodyShape.HEAD_FROM * BodyShape.SIZE, BodyShape.HEIGHT, "seeing from the head")
	assert_between(player.visor.global_position.y - player.global_position.y, BodyShape.HEAD_FROM * BodyShape.SIZE, BodyShape.HEIGHT, "eyes on the head")
	assert_gt(player.name_tag.position.y, BodyShape.HEIGHT, "name above the head")


func test_legs_do_not_swing_when_standing_still() -> void:
	assert_eq(CHARACTER.leg_swing(1.0, 0.0), 0.0)


func test_legs_swing_more_at_speed() -> void:
	assert_lt(absf(CHARACTER.leg_swing(PI / 2.0, 2.0)), absf(CHARACTER.leg_swing(PI / 2.0, 6.0)))
	assert_almost_eq(CHARACTER.leg_swing(PI / 2.0, 6.0), CHARACTER.STRIDE, 0.0001)
	assert_almost_eq(CHARACTER.leg_swing(PI / 2.0, 20.0), CHARACTER.STRIDE, 0.0001, "no more than a stride")


func test_walking_swings_the_legs_opposite_ways() -> void:
	character.set_motion(6.0, false)
	character._process(0.1)
	var legs := _named("Leg")
	assert_ne(legs[0].rotation.x, 0.0)
	assert_almost_eq(legs[0].rotation.x, -legs[1].rotation.x, 0.0001)


func test_in_the_air_the_legs_tuck() -> void:
	character.set_motion(0.0, true)
	character._process(0.016)
	for leg in _named("Leg"):
		assert_lt(leg.rotation.x, 0.0)


func test_knocked_out_it_melts_then_disappears() -> void:
	character.set_in_play(false)
	character._process(CHARACTER.MELT_TIME / 2.0)
	assert_true(character.visible, "still melting")
	assert_lt(character.scale.y, BodyShape.SIZE)
	assert_gt(character.scale.x, BodyShape.SIZE)
	character._process(CHARACTER.MELT_TIME)
	assert_false(character.visible)


func test_coming_back_it_pops_up_to_full_size() -> void:
	character.set_in_play(false)
	character._process(CHARACTER.MELT_TIME * 2.0)
	character.set_in_play(true)
	character._process(0.01)
	assert_true(character.visible)
	assert_lt(character.scale.y, BodyShape.SIZE * 0.5)
	character._process(CHARACTER.POP_TIME)
	assert_almost_eq(character.scale.y, BodyShape.SIZE, 0.0001)


func test_a_character_not_shown_stays_hidden() -> void:
	character.shown = false
	character._process(0.016)
	assert_false(character.visible)


func test_colour_and_glow() -> void:
	character.set_colour(Color.RED)
	assert_eq(character.colour(), Color.RED)
	character.set_glow(true)
	character.set_glow(false)
	pass_test("glow switches without error")


func test_a_players_own_character_is_hidden_from_them_but_its_weapon_shows() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "1"
	add_child_autofree(player)
	await wait_process_frames(1)
	assert_false(player.body.visible)
	assert_false(player.visor.visible)
	assert_true(player.gun.visible)
	assert_not_null(player.muzzle)


func test_firing_kicks_the_weapon_back_and_it_settles() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "1"
	add_child_autofree(player)
	await wait_process_frames(1)
	var rest: Vector3 = player.gun.position
	player.kick()
	player._animate(0.016)
	assert_gt(player.gun.position.z, rest.z)
	player._animate(1.0)
	assert_almost_eq(player.gun.position.distance_to(rest), 0.0, 0.02)
