extends GutTest
## What a player's body does (player/body.gd), whatever it is doing it with:
## tested on the soldier everyone sees (player/soldier_character.gd). How the
## soldier shows it is in test_soldier_character.gd.

const SOLDIER := preload("res://player/soldier_character.gd")
const PLAYER := preload("res://player/player.tscn")

var character: Node3D


func before_each() -> void:
	character = SOLDIER.new()
	add_child_autofree(character)


func test_a_hurt_player_winces_for_a_moment() -> void:
	character._process(0.016)
	assert_eq(character.expression(), SOLDIER.CALM)
	character.grimace()
	character._process(0.016)
	assert_true(character.is_grimacing())
	assert_eq(character.expression(), SOLDIER.WINCE)
	character._process(SOLDIER.GRIMACE_TIME)
	assert_false(character.is_grimacing())
	assert_eq(character.expression(), SOLDIER.CALM)


func test_the_players_capsule_head_and_eyes_match_the_body_shots_are_tested_against() -> void:
	var player: CharacterBody3D = PLAYER.instantiate()
	player.name = "2"
	add_child_autofree(player)
	var shape: CapsuleShape3D = player.collider.shape
	assert_almost_eq(shape.radius, BodyShape.RADIUS, 0.001)
	assert_almost_eq(shape.height, BodyShape.HEIGHT, 0.001)
	assert_almost_eq(player.collider.position.y, BodyShape.HEIGHT / 2.0, 0.001, "standing on the feet")
	assert_between(player.head.position.y, BodyShape.HEAD_FROM * BodyShape.SIZE, BodyShape.HEIGHT, "seeing from the head")
	assert_almost_eq(player.head.position.y, BodyShape.HEAD_HEIGHT * BodyShape.SIZE, 0.001, "at the level of the eyes")
	assert_gt(player.name_tag.position.y, BodyShape.HEIGHT, "name above the head")
	assert_eq(player.body.get_script(), SOLDIER, "seen as the soldier")


func test_knocked_out_it_falls_then_disappears() -> void:
	character.set_in_play(false)
	character._process(SOLDIER.MELT_TIME / 2.0)
	assert_true(character.visible, "still falling")
	character._process(SOLDIER.MELT_TIME)
	assert_false(character.visible)


func test_coming_back_it_pops_up_to_full_size() -> void:
	character.set_in_play(false)
	character._process(SOLDIER.MELT_TIME * 2.0)
	character.set_in_play(true)
	character._process(0.01)
	assert_true(character.visible)
	assert_lt(character.scale.y, BodyShape.SIZE * 0.5)
	character._process(SOLDIER.POP_TIME)
	assert_almost_eq(character.scale.y, BodyShape.SIZE, 0.0001)


func test_a_character_not_shown_stays_hidden() -> void:
	character.shown = false
	character._process(0.016)
	assert_false(character.visible)


func test_colour_and_glow() -> void:
	character.set_colour(Color.RED)
	assert_eq(character.colour(), Color.RED)
	character.set_glow(true)
	assert_true(character._glowing)
	character.set_glow(false)
	assert_false(character._glowing)


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
	character._process(0.016)
	assert_false(character.is_flashing())
	character.grimace()
	assert_true(character.is_flashing())
	character._process(SOLDIER.FLASH_TIME + 0.01)
	assert_false(character.is_flashing(), "long before the wince is over")
	assert_true(character.is_grimacing())


func test_the_face_shows_what_matters_most() -> void:
	assert_eq(character.expression(), SOLDIER.CALM)
	character.fire_face()
	assert_eq(character.expression(), SOLDIER.SET, "firing")
	character.cheer()
	assert_eq(character.expression(), SOLDIER.CHEER, "a knock-out outshines that")
	character.set_dizzy(true)
	assert_eq(character.expression(), SOLDIER.DIZZY, "being melted outdoes a cheer")
	character.grimace()
	assert_eq(character.expression(), SOLDIER.WINCE, "and being hurt comes before everything")
	character._process(SOLDIER.GRIMACE_TIME + 0.01)
	assert_eq(character.expression(), SOLDIER.DIZZY)
	character.set_dizzy(false)
	assert_eq(character.expression(), SOLDIER.CHEER)
	character._process(SOLDIER.CHEER_TIME)
	assert_eq(character.expression(), SOLDIER.CALM, "the set face ran out meanwhile")


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
	character._process(SOLDIER.BLINK_TIME + 0.01)
	assert_false(character.is_blinking(), "and open again")


func test_the_eyes_do_not_blink_through_a_wince() -> void:
	character.grimace()
	character._blink_in = 0.0
	character._process(0.001)
	assert_false(character.is_blinking())


func test_a_shot_throws_a_character_back_and_a_burst_scatters_it() -> void:
	assert_eq(SOLDIER.ending_for("blaster"), SOLDIER.THROWN)
	assert_eq(SOLDIER.ending_for("pistol"), SOLDIER.THROWN)
	assert_eq(SOLDIER.ending_for(Melee.ID), SOLDIER.THROWN)
	assert_eq(SOLDIER.ending_for("launcher"), SOLDIER.SCATTERED)
	assert_eq(SOLDIER.ending_for(Weapons.GRENADE), SOLDIER.SCATTERED)


func test_a_thrown_character_goes_the_way_it_was_hit_and_stops() -> void:
	character.knock("blaster", Vector3(0, 0.5, 1))
	character.set_in_play(false)
	for frame in int(SOLDIER.MELT_TIME / 2.0 * 30.0):
		character._process(1.0 / 30.0)
	assert_true(character.visible, "still there, falling")
	assert_gt(character.position.z, 0.3, "thrown back, away from the shot")
	assert_almost_eq(character.position.y, 0.0, 0.0001, "along the ground")
	var halfway := character.position.z
	for frame in int(SOLDIER.MELT_TIME * 30.0):
		character._process(1.0 / 30.0)
	assert_false(character.visible)
	assert_gt(character.position.z, halfway)
	assert_lt(character.position.z, 4.0, "not far")


func test_a_scattered_character_is_gone_at_once() -> void:
	character.knock("launcher", Vector3.FORWARD)
	character.set_in_play(false)
	character._process(0.001)
	assert_eq(character.ending(), SOLDIER.SCATTERED)
	assert_false(character.visible, "nothing left to fall")


func test_coming_back_stands_a_thrown_character_up_where_the_player_is() -> void:
	character.knock("blaster", Vector3.RIGHT)
	character.set_in_play(false)
	character._process(SOLDIER.MELT_TIME * 2.0)
	character.set_in_play(true)
	character._process(SOLDIER.POP_TIME + 0.01)
	assert_eq(character.position, Vector3.ZERO)
	assert_eq(character.rotation, Vector3.ZERO)
	assert_true(character.visible)
	assert_eq(character.ending(), SOLDIER.THROWN, "a burst last time is forgotten")


func test_each_emote_ends_and_leaves_it_standing() -> void:
	for kind: int in Emotes.KINDS:
		character.emote(kind)
		assert_eq(character.emoting(), kind)
		character._process(Emotes.TIME / 2.0)
		assert_eq(character.emoting(), kind, "%s: still going" % Emotes.NAMES[kind])
		character._process(Emotes.TIME)
		assert_eq(character.emoting(), -1, Emotes.NAMES[kind])
		character._process(0.016)
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
	assert_eq(character.expression(), SOLDIER.CHEER)


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
