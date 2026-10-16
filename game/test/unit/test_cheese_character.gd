extends GutTest

const CHARACTER := preload("res://player/cheese_character.gd")
const PLAYER := preload("res://player/player.tscn")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


func _named(prefix: String) -> Array:
	return character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with(prefix))


func test_it_has_a_torso_a_head_two_legs_and_two_arms() -> void:
	assert_not_null(character.get_node_or_null("Torso"))
	assert_not_null(character.get_node_or_null("Head"))
	assert_eq(_named("Leg").size(), 2)
	assert_eq(_named("Arm").size(), 2)
	assert_eq(_named("Camo").size(), CHARACTER.CAMO.size())
	assert_eq(_named("Eye").size(), 2)


## How far `mesh` reaches out from the surface it lies on, which faces `normal`.
func _thickness(mesh: MeshInstance3D, normal: Vector3) -> float:
	var low := INF
	var high := -INF
	for corner in mesh.mesh.get_faces():
		var out: float = (mesh.transform * corner).dot(normal)
		low = minf(low, out)
		high = maxf(high, out)
	return high - low


func test_camouflage_is_flat_marks_on_the_torso_not_lumps() -> void:
	var half := BodyShape.TORSO_SIZE / 2.0
	for blotch: MeshInstance3D in _named("Camo"):
		var normal := Vector3.FORWARD if blotch.position.z < 0.0 else Vector3.BACK
		assert_lt(_thickness(blotch, normal), 0.002, blotch.name)
		assert_between(blotch.position.z * normal.z - half.z, 0.0, 0.01, "%s sits on the torso" % blotch.name)
		var bounds: AABB = blotch.transform * blotch.get_aabb()
		assert_lte(maxf(absf(bounds.position.x), absf(bounds.end.x)), half.x, "%s is on the torso, not off its side" % blotch.name)


func test_the_eyes_are_drawn_on_the_front_of_the_head() -> void:
	var front := -BodyShape.HEAD_SIZE.z / 2.0
	for eye: MeshInstance3D in _named("Eye"):
		assert_lt(_thickness(eye, Vector3.FORWARD), 0.002, "flat, not a ball")
		assert_between(front - eye.position.z, 0.0, 0.01, "on the surface")
		assert_almost_eq(eye.position.y, BodyShape.HEAD_HEIGHT, 0.001)


func test_the_eyes_follow_where_the_player_looks() -> void:
	character.set_gaze(0.8)
	character._process(0.016)
	var up: float = _named("Eye")[0].position.y
	character.set_gaze(-0.8)
	character._process(0.016)
	assert_gt(up, _named("Eye")[0].position.y)


func test_a_hurt_player_winces_for_a_moment() -> void:
	var calm: Node3D = character.get_node("Calm")
	var wince: Node3D = character.get_node("Wince")
	character._process(0.016)
	assert_true(calm.visible)
	assert_false(wince.visible)
	character.grimace()
	character._process(0.016)
	assert_true(character.is_grimacing())
	assert_false(calm.visible, "the smile goes")
	assert_true(wince.visible, "for gritted teeth")
	assert_lt(_named("Eye")[0].scale.z, 1.0, "and the eyes screw up")
	character._process(CHARACTER.GRIMACE_TIME)
	assert_false(character.is_grimacing())
	assert_true(calm.visible)
	assert_false(wince.visible)
	assert_eq(_named("Eye")[0].scale.z, 1.0)


func test_the_arms_are_slim_enough_to_pass_by() -> void:
	for limb: Node3D in _named("Arm"):
		var sleeve: MeshInstance3D = limb.find_children("*", "MeshInstance3D", true, false)[0]
		assert_lte((sleeve.mesh as CapsuleMesh).radius, BodyShape.LIMB_RADIUS + 0.0001, limb.name)


func test_the_weapon_arm_reaches_to_the_hand_on_the_weapon() -> void:
	var arm: Node3D = _named("Arm")[1]
	for hand: Vector3 in [Vector3(0.36, 0.6, -0.34), Vector3(0.3, 0.95, -0.5), Vector3(0.5, 0.5, 0.1)]:
		character.reach_for(hand)
		var length: float = BodyShape.ARMS[1][1] * arm.get_child(0).scale.y
		# The sleeve runs down the arm's own Y axis from the shoulder.
		var end := arm.position + arm.basis * Vector3(0, -length, 0)
		assert_almost_eq(end.distance_to(hand), 0.0, 0.001, str(hand))


func test_it_fits_inside_the_players_collision_capsule() -> void:
	var bounds := AABB()
	var first := true
	character._process(0.016)
	for mesh: MeshInstance3D in character.find_children("*", "MeshInstance3D", true, false):
		if not mesh.is_visible_in_tree():
			continue
		var box: AABB = mesh.global_transform * mesh.get_aabb()
		bounds = box if first else bounds.merge(box)
		first = false
	assert_gte(bounds.position.y, -0.01, "not below the feet")
	assert_lte(bounds.end.y, BodyShape.HEIGHT, "not above the head")
	assert_lte(maxf(absf(bounds.position.x), absf(bounds.end.x)), BodyShape.RADIUS + 0.05)
	assert_lte(maxf(absf(bounds.position.z), absf(bounds.end.z)), BodyShape.RADIUS + 0.05)
	# As tall as what can be hit, which leaves the eyes where they always were.
	assert_almost_eq(bounds.end.y, (BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y / 2.0) * BodyShape.SIZE, 0.05, "up to the top of the helmet")


func test_the_players_capsule_head_and_eyes_match_the_body_shots_are_tested_against() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "2"
	add_child_autofree(player)
	var shape: CapsuleShape3D = player.collider.shape
	assert_almost_eq(shape.radius, BodyShape.RADIUS, 0.001)
	assert_almost_eq(shape.height, BodyShape.HEIGHT, 0.001)
	assert_almost_eq(player.collider.position.y, BodyShape.HEIGHT / 2.0, 0.001, "standing on the feet")
	assert_between(player.head.position.y, BodyShape.HEAD_FROM * BodyShape.SIZE, BodyShape.HEIGHT, "seeing from the head")
	var eye: Node3D = player.body.get_node("Eye1")
	assert_almost_eq(eye.global_position.y - player.global_position.y, player.head.position.y, 0.001, "seeing from the level of the eyes")
	assert_gt(player.name_tag.position.y, BodyShape.HEIGHT, "name above the head")


func test_legs_do_not_swing_when_standing_still() -> void:
	assert_eq(CHARACTER.leg_swing(1.0, 0.0), 0.0)


func test_legs_swing_more_at_speed() -> void:
	assert_lt(absf(CHARACTER.leg_swing(PI / 2.0, 2.0)), absf(CHARACTER.leg_swing(PI / 2.0, 6.0)))
	assert_almost_eq(CHARACTER.leg_swing(PI / 2.0, CHARACTER.FULL_STRIDE_SPEED), CHARACTER.STRIDE, 0.0001)
	assert_almost_eq(CHARACTER.leg_swing(PI / 2.0, 20.0), CHARACTER.STRIDE, 0.0001, "no more than a stride")


func test_the_feet_keep_pace_with_the_ground() -> void:
	var leg := BodyShape.HIP_HEIGHT * BodyShape.SIZE
	for speed: float in [4.0, Movement.WALK_SPEED, Movement.WALK_SPEED * Movement.RUN_FACTOR]:
		# A foot planted on the ground sweeps from one end of its swing to the
		# other in a step, and there are two steps to a cycle.
		var step := 2.0 * leg * sin(CHARACTER.leg_swing(PI / 2.0, speed))
		assert_almost_eq(CHARACTER.cycle_length(speed), 2.0 * step, 0.001, "at %s m/s" % speed)
	assert_gt(CHARACTER.cycle_length(Movement.WALK_SPEED * Movement.RUN_FACTOR), CHARACTER.cycle_length(Movement.WALK_SPEED), "longer strides at a run")
	assert_gt(CHARACTER.cycle_length(0.2), 1.0, "and a shuffle doesn't patter")
	# So a walk goes through as many cycles as the ground it covers allows.
	character.set_motion(Movement.WALK_SPEED, false)
	character._phase = 0.0
	character._process(0.1)
	assert_almost_eq(character._phase, Movement.WALK_SPEED * 0.1 / CHARACTER.cycle_length(Movement.WALK_SPEED) * TAU, 0.001)


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
		assert_gt(leg.rotation.x, 0.0, "knees up")
		assert_lt(leg.get_node("Knee").rotation.x, 0.0, "and bent")


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


func test_a_hit_flashes_the_whole_body_for_an_instant() -> void:
	character.set_colour(Color.BLUE)
	character._process(0.016)
	assert_false(character.is_flashing())
	character.grimace()
	assert_true(character.is_flashing())
	assert_true(character._material.emission_enabled)
	assert_eq(character._material.emission, Color.WHITE)
	character._process(CHARACTER.FLASH_TIME + 0.01)
	assert_false(character.is_flashing(), "long before the wince is over")
	assert_true(character.is_grimacing())
	assert_false(character._material.emission_enabled)


func test_a_flash_gives_way_to_an_overloads_glow() -> void:
	character.set_glow(true)
	character.grimace()
	character._process(CHARACTER.FLASH_TIME + 0.01)
	assert_true(character._material.emission_enabled)
	assert_ne(character._material.emission, Color.WHITE)


func test_crouching_bends_the_knees_and_brings_everything_else_down() -> void:
	var torso: Node3D = character.get_node("Torso")
	var head: Node3D = character.get_node("Head")
	var eye: Node3D = _named("Eye")[0]
	var stood := torso.position.y
	var camo: Node3D = _named("Camo")[0]
	var camo_stood := camo.position.y
	character.set_crouch(1.0)
	character._process(0.016)
	assert_almost_eq(torso.position.y, stood - BodyShape.CROUCH_DROP, 0.0001)
	assert_almost_eq(head.position.y, -BodyShape.CROUCH_DROP, 0.0001)
	assert_almost_eq(camo.position.y, camo_stood - BodyShape.CROUCH_DROP, 0.0001, "the camouflage comes with the torso")
	assert_almost_eq(eye.position.y, BodyShape.head_height(1.0), 0.0001)
	for leg: Node3D in _named("Leg"):
		# Still from the bottom of the torso to the ground.
		assert_almost_eq(leg.position.y, BodyShape.HIP_HEIGHT - BodyShape.CROUCH_DROP, 0.0001)
		var knee: Node3D = leg.get_node("Knee")
		var boot: Node3D = knee.get_node("Boot")
		assert_lt(knee.global_position.z, leg.global_position.z - 0.3, "the knee out in front")
		assert_almost_eq(boot.global_position.y, 0.0, 0.001, "the foot on the ground")
		assert_almost_eq(boot.global_position.z, leg.global_position.z, 0.001, "under the hip")
		assert_almost_eq(boot.global_basis.y.normalized().z, 0.0, 0.02, "the boot not tipped forward or back")
	character.set_crouch(0.0)
	character._process(0.016)
	assert_almost_eq(torso.position.y, stood, 0.0001)
	assert_almost_eq(_named("Leg")[0].rotation.x, 0.0, 0.0001)
	assert_almost_eq(_named("Leg")[0].get_node("Knee").rotation.x, 0.0, 0.0001)


func test_the_face_shows_what_matters_most() -> void:
	assert_eq(character.expression(), CHARACTER.CALM)
	character.fire_face()
	assert_eq(character.expression(), CHARACTER.SET, "firing")
	character.cheer()
	assert_eq(character.expression(), CHARACTER.CHEER, "a knock-out outshines that")
	character.set_dizzy(true)
	assert_eq(character.expression(), CHARACTER.DIZZY, "being melted outdoes a cheer")
	character.grimace()
	assert_eq(character.expression(), CHARACTER.WINCE, "and being hurt comes before everything")
	character._process(CHARACTER.GRIMACE_TIME + 0.01)
	assert_eq(character.expression(), CHARACTER.DIZZY)
	character.set_dizzy(false)
	assert_eq(character.expression(), CHARACTER.CHEER)
	character._process(CHARACTER.CHEER_TIME)
	assert_eq(character.expression(), CHARACTER.CALM, "the set face ran out meanwhile")


func test_only_the_face_being_made_is_drawn() -> void:
	for face: String in [CHARACTER.CALM, CHARACTER.SET, CHARACTER.CHEER, CHARACTER.DIZZY, CHARACTER.WINCE]:
		character._cheer_left = 0.0
		character._set_left = 0.0
		character._grimace_left = 0.0
		character.set_dizzy(false)
		match face:
			CHARACTER.SET: character.fire_face()
			CHARACTER.CHEER: character.cheer()
			CHARACTER.DIZZY: character.set_dizzy(true)
			CHARACTER.WINCE: character.grimace()
		character._process(0.001)
		assert_eq(character.expression(), face)
		var showing := ["Calm", "Set", "Cheer", "Dizzy", "Wince"].filter(func(node: String) -> bool: return character.get_node(node).visible)
		assert_eq(showing, [face.capitalize()], face)
		assert_eq(character.get_node("Stars").visible, face == CHARACTER.DIZZY, "stars only when dizzy")


func test_every_face_is_drawn_flat_on_the_front_of_the_head_under_the_helmet() -> void:
	var front := -BodyShape.HEAD_SIZE.z / 2.0
	for face: String in ["Calm", "Set", "Cheer", "Dizzy", "Wince"]:
		for mark: MeshInstance3D in character.get_node(face).find_children("*", "MeshInstance3D", true, false):
			assert_between(front - mark.position.z, 0.0, 0.015, "%s: %s is on the surface" % [face, mark.name])
			var bounds: AABB = mark.transform * mark.get_aabb()
			assert_lt(maxf(absf(bounds.position.x), absf(bounds.end.x)), BodyShape.HEAD_SIZE.x / 2.0 - 0.02, "%s: %s is on the face" % [face, mark.name])
			assert_between(bounds.position.y, BodyShape.HEAD_FROM, BodyShape.HEAD_HEIGHT + 0.035, "%s: %s is under the helmet" % [face, mark.name])
			assert_between(bounds.end.y, BodyShape.HEAD_FROM, BodyShape.HEAD_HEIGHT + 0.035, "%s: %s is under the helmet" % [face, mark.name])


func test_the_eyes_blink_now_and_then() -> void:
	var blinks := 0
	var was := false
	for frame in 1200:
		character._process(1.0 / 60.0)
		if character.is_blinking() and not was:
			blinks += 1
		was = character.is_blinking()
	assert_between(blinks, 2, 12, "a few times in twenty seconds")
	character._blink_in = 0.0
	character._process(0.001)
	assert_true(character.is_blinking())
	assert_lt(_named("Eye")[0].scale.z, 0.3, "shut")
	character._process(CHARACTER.BLINK_TIME + 0.01)
	assert_eq(_named("Eye")[0].scale.z, 1.0, "and open again")


func test_the_eyes_do_not_blink_through_a_wince() -> void:
	character.grimace()
	character._blink_in = 0.0
	character._process(0.001)
	assert_false(character.is_blinking())


func test_the_eyes_turn_toward_someone_near() -> void:
	character.set_glance(1.0)
	character._process(0.016)
	var right: float = _named("Eye")[0].position.x
	character.set_glance(-1.0)
	character._process(0.016)
	assert_lt(_named("Eye")[0].position.x, right)
	character.set_glance(0.0)
	character._process(0.016)
	assert_almost_eq(_named("Eye")[0].position.x, BodyShape.eye_centres()[0].x, 0.0001)


func test_a_shot_throws_a_character_back_as_it_melts_and_a_burst_scatters_it() -> void:
	assert_eq(CHARACTER.ending_for("blaster"), CHARACTER.THROWN)
	assert_eq(CHARACTER.ending_for("pistol"), CHARACTER.THROWN)
	assert_eq(CHARACTER.ending_for(Melee.ID), CHARACTER.THROWN)
	assert_eq(CHARACTER.ending_for("launcher"), CHARACTER.SCATTERED)
	assert_eq(CHARACTER.ending_for(Weapons.GRENADE), CHARACTER.SCATTERED)


func test_a_thrown_character_goes_the_way_it_was_hit_tips_over_and_stops() -> void:
	character.knock("blaster", Vector3(0, 0.5, 1))
	character.set_in_play(false)
	character._process(CHARACTER.MELT_TIME / 2.0)
	assert_true(character.visible, "still there, melting")
	assert_gt(character.position.z, 0.3, "thrown back, away from the shot")
	assert_almost_eq(character.position.y, 0.0, 0.0001, "along the ground")
	assert_ne(character.rotation, Vector3.ZERO, "tipping over")
	var halfway := character.position.z
	character._process(CHARACTER.MELT_TIME)
	assert_false(character.visible)
	assert_gt(character.position.z, halfway)
	assert_lt(character.position.z, 4.0, "not far")


func test_a_scattered_character_is_gone_at_once() -> void:
	character.knock("launcher", Vector3.FORWARD)
	character.set_in_play(false)
	character._process(0.001)
	assert_eq(character.ending(), CHARACTER.SCATTERED)
	assert_false(character.visible, "nothing left to melt")


func test_coming_back_stands_a_thrown_character_up_where_the_player_is() -> void:
	character.knock("blaster", Vector3.RIGHT)
	character.set_in_play(false)
	character._process(CHARACTER.MELT_TIME * 2.0)
	character.set_in_play(true)
	character._process(CHARACTER.POP_TIME + 0.01)
	assert_eq(character.position, Vector3.ZERO)
	assert_eq(character.rotation, Vector3.ZERO)
	assert_true(character.visible)
	assert_eq(character.ending(), CHARACTER.THROWN, "a burst last time is forgotten")


func test_each_emote_moves_the_free_arm_and_ends() -> void:
	var arm: Node3D = _named("Arm")[0]
	for kind: int in Emotes.KINDS:
		character.emote(kind)
		assert_eq(character.emoting(), kind)
		character._process(Emotes.TIME / 2.0)
		assert_lt(arm.rotation.z, Emotes.ARM_REST - 0.4, "%s: the arm is up" % Emotes.NAMES[kind])
		character._process(Emotes.TIME)
		assert_eq(character.emoting(), -1, Emotes.NAMES[kind])
		character._process(0.016)
		assert_almost_eq(arm.rotation.z, Emotes.ARM_REST, 0.0001, "%s: and back down" % Emotes.NAMES[kind])
		assert_eq(character.position, Vector3.ZERO)
		assert_almost_eq(character.rotation.z, 0.0, 0.0001)


func test_a_dance_sways_and_hops_and_a_cheer_grins() -> void:
	var swayed := 0.0
	var hopped := 0.0
	character.emote(Emotes.DANCE)
	for frame in 60:
		character._process(1.0 / 60.0)
		swayed = maxf(swayed, absf(character.rotation.z))
		hopped = maxf(hopped, character.position.y)
	assert_gt(swayed, 0.1)
	assert_gt(hopped, 0.05)
	character._process(Emotes.TIME)
	character.emote(Emotes.CHEER)
	character._process(0.3)
	assert_eq(character.expression(), CHARACTER.CHEER)


func test_firing_or_being_hurt_cuts_an_emote_short() -> void:
	character.emote(Emotes.DANCE)
	character.fire_face()
	assert_eq(character.emoting(), -1)
	character.emote(Emotes.WAVE)
	character.grimace()
	assert_eq(character.emoting(), -1)
	character.emote(Emotes.WAVE)
	character.set_in_play(false)
	character._process(0.016)
	assert_eq(character.emoting(), -1, "and so does being knocked out")


func test_an_emote_holds_itself_gently_at_each_end() -> void:
	for kind: int in Emotes.KINDS:
		assert_almost_eq(Emotes.pose(kind, 0.0).arm, Emotes.ARM_REST, 0.0001, Emotes.NAMES[kind])
		assert_almost_eq(Emotes.pose(kind, Emotes.TIME).arm, Emotes.ARM_REST, 0.0001, Emotes.NAMES[kind])
		assert_eq(Emotes.pose(kind, Emotes.TIME).hop, 0.0)
		assert_true(Emotes.is_kind(kind))
		assert_true(Emotes.NAMES.has(kind))
	assert_false(Emotes.is_kind(99))
	assert_false(Emotes.is_kind("wave"))
	assert_eq(Emotes.next(Emotes.WAVE), Emotes.DANCE)
	assert_eq(Emotes.next(Emotes.CHEER), Emotes.WAVE, "and round again")
	assert_lt(Emotes.INTERVAL, Emotes.TIME + 0.7)
