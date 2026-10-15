extends GutTest
## Seeing and hearing: hit marks that differ by shape, text that can be made
## bigger, captions for sounds, and a view shake that can be turned down.

const HUD := preload("res://ui/combat_hud.gd")
const CAPTIONS := preload("res://ui/captions.gd")
const MAIN := preload("res://main.tscn")

var saved := {}


func before_each() -> void:
	for key in ["hud_scale", "captions", "screen_shake", "seen_help"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("seen_help", true)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func test_each_kind_of_hit_has_its_own_mark() -> void:
	assert_eq(HUD.hit_kind_for(false, false), HUD.HIT)
	assert_eq(HUD.hit_kind_for(false, true), HUD.HEADSHOT)
	assert_eq(HUD.hit_kind_for(true, false), HUD.KNOCKOUT)
	assert_eq(HUD.hit_kind_for(true, true), HUD.KNOCKOUT)


func test_text_grows_with_the_setting() -> void:
	Settings.set_value("hud_scale", 1.0)
	assert_eq(HUD.text_size(20), 20)
	Settings.set_value("hud_scale", 1.5)
	assert_eq(HUD.text_size(20), 30)
	Settings.set_value("hud_scale", 9.0)
	assert_eq(HUD.text_size(20), 30, "kept within its range")


func _main_in_a_session() -> Node:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	main.world.match_.start()
	main.world.start()
	main.combat_hud.show()
	await wait_process_frames(2)
	return main


func test_the_fights_text_resizes_at_once() -> void:
	Settings.set_value("hud_scale", 1.0)
	var main: Node = await _main_in_a_session()
	var ammo: Label = main.combat_hud.find_child("Ammo", true, false)
	assert_eq(ammo.get_theme_font_size("font_size"), 22)
	Settings.set_value("hud_scale", 1.5)
	assert_eq(ammo.get_theme_font_size("font_size"), 33)
	main.world.stop()


func test_the_hud_records_the_kind_of_mark() -> void:
	var main: Node = await _main_in_a_session()
	main.world.match_.hit_confirmed.emit(2, 100, false, true)
	assert_eq(main.combat_hud.hit_kind, HUD.HEADSHOT)
	main.world.match_.hit_confirmed.emit(2, 100, true, false)
	assert_eq(main.combat_hud.hit_kind, HUD.KNOCKOUT)
	main.world.stop()


func test_a_caption_says_what_and_where() -> void:
	assert_eq(CAPTIONS.caption("step", Vector3(0, 0, -10), Vector3.ZERO, 0.0), "Footsteps, ahead")
	assert_eq(CAPTIONS.caption("shotgun", Vector3(10, 0, 0), Vector3.ZERO, 0.0), "Shotgun blast, right")
	assert_eq(CAPTIONS.caption("burst", Vector3(-10, 0, 0), Vector3.ZERO, 0.0), "Cheese burst, left")
	assert_eq(CAPTIONS.caption("blaster", Vector3(0, 0, 10), Vector3.ZERO, 0.0), "Blaster fire, behind")
	# Turned to face +x, what is at +x is ahead.
	assert_eq(CAPTIONS.caption("step", Vector3(10, 0, 0), Vector3.ZERO, -PI / 2.0), "Footsteps, ahead")


func test_own_sounds_and_sounds_without_a_place_get_no_caption() -> void:
	assert_eq(CAPTIONS.caption("step", Vector3(0.5, 0, 0), Vector3.ZERO, 0.0), "")
	assert_eq(CAPTIONS.caption("overload", null, Vector3.ZERO, 0.0), "")
	assert_eq(CAPTIONS.caption("reload", Vector3(0, 0, -10), Vector3.ZERO, 0.0), "", "the screen shows reloading")


func test_captions_repeat_once_fade_and_keep_to_a_few() -> void:
	var captions: VBoxContainer = CAPTIONS.new()
	add_child_autofree(captions)
	captions.add("Footsteps, left")
	captions.add("Footsteps, left")
	assert_eq(captions.showing(), ["Footsteps, left"] as Array[String])
	for index in 6:
		captions.add("Sound %d" % index)
	assert_eq(captions.showing().size(), CAPTIONS.MOST)
	assert_eq(captions.showing()[-1], "Sound 5")
	captions._process(CAPTIONS.LIFE + 0.1)
	assert_eq(captions.showing().size(), 0)


func test_captions_appear_only_when_turned_on() -> void:
	var main: Node = await _main_in_a_session()
	var own: Node3D = main.world.player_node(1)
	var away: Vector3 = own.global_position + Vector3(8, 0, 0)
	Settings.set_value("captions", false)
	Sounds.played.emit("step", away)
	assert_eq(main.combat_hud.captions.showing().size(), 0)
	Settings.set_value("captions", true)
	Sounds.played.emit("step", away)
	assert_eq(main.combat_hud.captions.showing().size(), 1)
	main.world.stop()


func test_a_jolt_shakes_the_view_then_dies_away() -> void:
	var shake := Shake.new()
	assert_eq(shake.tick(0.016, 1.0), Vector2.ZERO)
	shake.add(0.6)
	var biggest := 0.0
	for i in 10:
		biggest = maxf(biggest, shake.tick(0.016, 1.0).length())
	assert_gt(biggest, 0.0)
	assert_lte(biggest, Vector2(Shake.MAX_PITCH, Shake.MAX_ROLL).length())
	for i in 100:
		shake.tick(0.016, 1.0)
	assert_eq(shake.tick(0.016, 1.0), Vector2.ZERO)


func test_turned_off_the_view_does_not_shake() -> void:
	var shake := Shake.new()
	shake.add(1.0)
	for i in 10:
		assert_eq(shake.tick(0.016, 0.0), Vector2.ZERO)


func test_harder_jolts_shake_more() -> void:
	assert_gt(Shake.from_damage(50), Shake.from_damage(10))
	assert_gt(Shake.from_burst(1.0, 8.0), Shake.from_burst(6.0, 8.0))
	assert_eq(Shake.from_burst(9.0, 8.0), 0.0)


func test_being_hurt_shakes_the_view_but_not_the_aim() -> void:
	Settings.set_value("screen_shake", 1.0)
	var main: Node = await _main_in_a_session()
	var own: Node3D = main.world.player_node(1)
	main.world.match_.hurt.emit(2, own.global_position + Vector3(5, 0, 0), 40)
	await wait_process_frames(3)
	assert_ne(own.camera.rotation, Vector3.ZERO)
	assert_eq(own.head.rotation, Vector3.ZERO)
	main.world.stop()


func test_the_damage_of_a_hit_is_written_only_if_asked_for() -> void:
	var main: Node = await _main_in_a_session()
	var hud: Control = main.combat_hud
	Settings.set_value("damage_numbers", false)
	main.world.match_.hit_confirmed.emit(2, 20, false, false)
	assert_eq(hud.damage_numbers_shown(), [] as Array[String])
	Settings.set_value("damage_numbers", true)
	main.world.match_.hit_confirmed.emit(2, 20, false, false)
	main.world.match_.hit_confirmed.emit(2, 35, false, true)
	assert_eq(hud.damage_numbers_shown(), ["20", "35"] as Array[String])
	await wait_seconds(HUD.DAMAGE_NUMBER_LIFE + 0.1)
	assert_eq(hud.damage_numbers_shown(), [] as Array[String], "each fades")
	for hit in 20:
		main.world.match_.hit_confirmed.emit(2, hit, false, false)
	assert_lte(hud.damage_numbers_shown().size(), 6, "and only a few show at once")
	Settings.set_value("damage_numbers", false)
	main.world.stop()


func test_the_crosshair_gives_way_to_the_sights_when_aiming() -> void:
	var main: Node = await _main_in_a_session()
	var own: Node3D = main.world.player_node(1)
	assert_not_null(own)
	assert_true(main.combat_hud.crosshair_shown())
	own._aim = 1.0
	assert_false(main.combat_hud.crosshair_shown())
	own._aim = 0.5
	assert_true(main.combat_hud.crosshair_shown(), "still there while the weapon is on its way up")
	main.world.stop()


func _colours_back() -> void:
	for key: String in [TeamColors.OWN_CHEDDAR, TeamColors.OWN_BRIE, TeamColors.OWN_HIT]:
		Settings.set_value(key, "")
	Settings.set_value(TeamColors.PRESET, "standard")


func test_each_colour_set_keeps_the_teams_apart() -> void:
	for preset: String in TeamColors.PRESETS:
		var pair: Array = TeamColors.PRESETS[preset]
		assert_gt(TeamColors.apart(pair[0], pair[1]), 0.5, preset)
		assert_true(TeamColors.PRESET_NAMES.has(preset), preset)
		assert_true(Settings.CHOICES[TeamColors.PRESET].has(preset), preset)


func test_a_set_chosen_colours_the_teams() -> void:
	Settings.set_value(TeamColors.PRESET, "tritanopia")
	assert_eq(TeamColors.of(MatchState.CHEDDAR), TeamColors.PRESETS.tritanopia[0])
	assert_eq(TeamColors.of(MatchState.BRIE), TeamColors.PRESETS.tritanopia[1])
	assert_eq(TeamColors.of(MatchState.NO_TEAM), Color.WHITE)
	_colours_back()
	assert_eq(TeamColors.of(MatchState.CHEDDAR), TeamColors.CHEDDAR, "the standard set is as it always was")


func test_a_colour_of_ones_own_wins_over_the_set() -> void:
	Settings.set_value(TeamColors.PRESET, "protanopia")
	Settings.set_value(TeamColors.OWN_BRIE, "purple")
	Settings.set_value(TeamColors.OWN_HIT, "yellow")
	assert_eq(TeamColors.of(MatchState.BRIE), TeamColors.OWN.purple)
	assert_eq(TeamColors.of(MatchState.CHEDDAR), TeamColors.PRESETS.protanopia[0], "the other keeps the set's")
	assert_eq(TeamColors.hit(), TeamColors.OWN.yellow)
	_colours_back()
	assert_eq(TeamColors.hit(), Color.WHITE)


func test_every_colour_of_ones_own_has_a_name_and_can_be_chosen() -> void:
	for own: String in TeamColors.OWN:
		assert_true(TeamColors.OWN_NAMES.has(own), own)
		for key: String in [TeamColors.OWN_CHEDDAR, TeamColors.OWN_BRIE, TeamColors.OWN_HIT]:
			assert_true(Settings.CHOICES[key].has(own), "%s for %s" % [own, key])
