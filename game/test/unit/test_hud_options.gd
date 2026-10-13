extends GutTest
## The HUD the player's way, the weapons bar and wheel, the numbers for the
## curious, and the photo mode.

const MAIN := preload("res://main.tscn")
const WORLD := preload("res://world/world.tscn")
const SETTINGS_MENU := preload("res://ui/settings_menu.gd")

var saved := {}


func before_each() -> void:
	for key: String in [HudLayout.SETTING, "seen_help"] + Readings.SETTINGS:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])
	Settings.set_value("seen_help", true)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _main_in_practice() -> Node:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	for frame in 2000:
		if not main.loading_cover.visible:
			break
		await wait_process_frames(1)
	main._on_way_in_chosen("practice")
	await wait_physics_frames(4)
	await wait_process_frames(3)
	return main


func test_a_part_is_as_usual_unless_the_layout_says_otherwise() -> void:
	assert_eq(HudLayout.part({}, "health"), HudLayout.USUAL)
	var layout := {"health": {"x": 0.2, "scale": 1.3, "on": false}}
	var health := HudLayout.part(layout, "health")
	assert_eq(health.x, 0.2)
	assert_eq(health.y, 0.0)
	assert_eq(health.scale, 1.3)
	assert_eq(health.on, false)
	assert_eq(HudLayout.part(layout, "weapon"), HudLayout.USUAL)
	# Out of bounds is brought in; what isn't a number is ignored.
	var wild := HudLayout.part({"feed": {"x": 9.0, "y": -9.0, "scale": 50.0, "opacity": 0.0, "on": "yes"}}, "feed")
	assert_eq(wild.x, HudLayout.RANGES.x[1])
	assert_eq(wild.y, HudLayout.RANGES.y[0])
	assert_eq(wild.scale, HudLayout.RANGES.scale[1])
	assert_eq(wild.opacity, HudLayout.RANGES.opacity[0], "never so faint it can't be found")
	assert_eq(wild.on, true)
	assert_eq(HudLayout.part({"feed": "nonsense"}, "feed"), HudLayout.USUAL)


func test_a_layout_is_kept_as_only_what_differs() -> void:
	assert_eq(HudLayout.tidy("nonsense"), {})
	assert_eq(HudLayout.tidy({"health": {"x": 0.0, "scale": 1.0}, "kazoo": {"x": 0.3}, "weapon": {"scale": 1.2}}), {"weapon": {"scale": 1.2}})
	var layout := HudLayout.with({}, "chat", HudLayout.ON, false)
	assert_eq(layout, {"chat": {"on": false}})
	layout = HudLayout.with(layout, "chat", HudLayout.OPACITY, 0.5)
	assert_eq(layout, {"chat": {"on": false, "opacity": 0.5}})
	layout = HudLayout.with(layout, "chat", HudLayout.ON, true)
	layout = HudLayout.with(layout, "chat", HudLayout.OPACITY, 1.0)
	assert_eq(layout, {}, "put back as usual, it is gone from the layout")
	assert_eq(HudLayout.with({}, "kazoo", HudLayout.X, 0.3), {})
	assert_eq(HudLayout.standard(), {})


func test_the_minimal_layout_keeps_what_is_needed_to_play() -> void:
	var minimal := HudLayout.minimal()
	assert_eq(HudLayout.tidy(minimal), minimal, "it is a layout")
	for part: String in ["health", "weapon", "objective", "captions"]:
		assert_true(HudLayout.part(minimal, part).on, part)
	for part: String in ["feed", "compass", "place", "score", "chat"]:
		assert_false(HudLayout.part(minimal, part).on, part)
	assert_lt(HudLayout.part(minimal, "health").scale, 1.0)
	for part: String in minimal:
		assert_has(HudLayout.PARTS, part)


func test_no_layout_puts_any_part_off_the_screen() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 11
	var screens := [Vector2(648, 648), Vector2(1280, 720), Vector2(360, 640), Vector2(2560, 1080)]
	for screen: Vector2 in screens:
		var whole := Rect2(Vector2.ZERO, screen)
		for attempt in 400:
			# A part somewhere on the screen, of some size, moved and scaled
			# as far as the layout lets it be, and further.
			var size := Vector2(rng.randf_range(20, 320), rng.randf_range(14, 180))
			var home := Rect2(Vector2(rng.randf_range(0, screen.x - size.x), rng.randf_range(0, screen.y - size.y)), size)
			var how := HudLayout.part({"health": {"x": rng.randf_range(-2, 2), "y": rng.randf_range(-2, 2), "scale": rng.randf_range(0, 3), "opacity": 1.0}}, "health")
			var placed := HudLayout.rect_for(home, how, screen)
			assert_true(whole.grow(0.01).encloses(placed), "%s at %s on %s" % [home, how, screen])
			assert_almost_eq(placed.size, home.size * HudLayout.fitting_scale(home.size, how.scale, screen), Vector2.ONE * 0.01)
	# A part is as big as asked, unless that is bigger than the screen.
	assert_eq(HudLayout.fitting_scale(Vector2(100, 40), 1.6, Vector2(1280, 720)), 1.6)
	assert_eq(HudLayout.fitting_scale(Vector2(300, 40), 1.6, Vector2(360, 640)), 1.2)
	assert_eq(HudLayout.fitting_scale(Vector2(0, 0), 1.6, Vector2(360, 640)), 1.6)
	# Left alone, a part stays exactly where it is.
	var still := Rect2(40, 500, 200, 80)
	assert_eq(HudLayout.rect_for(still, HudLayout.USUAL, Vector2(1280, 720)), still)


func test_a_part_scales_about_the_corner_it_sits_in() -> void:
	var screen := Vector2(1000, 600)
	var smaller := HudLayout.USUAL.duplicate()
	smaller.scale = 0.5
	# Bottom right: its right and bottom edges stay put.
	var corner := HudLayout.rect_for(Rect2(780, 480, 200, 100), smaller, screen)
	assert_almost_eq(corner.end, Vector2(980, 580), Vector2.ONE * 0.01)
	# Top left: its left and top edges do.
	var near := HudLayout.rect_for(Rect2(20, 20, 200, 100), smaller, screen)
	assert_almost_eq(near.position, Vector2(20, 20), Vector2.ONE * 0.01)
	# Top middle: it stays in the middle.
	var top := HudLayout.rect_for(Rect2(400, 10, 200, 40), smaller, screen)
	assert_almost_eq(top.get_center().x, 500.0, 0.01)


func test_the_real_hud_follows_the_layout_and_stays_on_the_screen() -> void:
	var main: Node = await _main_in_practice()
	var hud: Control = main.combat_hud
	var screen := Rect2(Vector2.ZERO, hud.size)
	assert_eq(hud.parts().keys(), HudLayout.PARTS.keys(), "a part for each name")
	var usual: Dictionary = hud.part_rects()
	for id: String in usual:
		assert_true(screen.grow(0.5).encloses(usual[id]), "%s as usual: %s" % [id, usual[id]])
	# Everything pushed as far as it will go one way, at its biggest.
	var far := {}
	for id: String in HudLayout.PARTS:
		far[id] = {"x": 0.5, "y": -0.5, "scale": 1.6}
	Settings.set_value(HudLayout.SETTING, far)
	hud._process(0.016)
	for id: String in hud.part_rects():
		assert_true(screen.grow(0.5).encloses(hud.part_rects()[id]), "%s pushed far: %s" % [id, hud.part_rects()[id]])
	assert_ne(hud.part_rects().health, usual.health, "and it did move")
	# The other way, at its smallest and faint.
	for id: String in far:
		far[id] = {"x": -0.5, "y": 0.5, "scale": 0.6, "opacity": 0.4}
	Settings.set_value(HudLayout.SETTING, far)
	hud._process(0.016)
	for id: String in hud.part_rects():
		assert_true(screen.grow(0.5).encloses(hud.part_rects()[id]), "%s pushed back: %s" % [id, hud.part_rects()[id]])
	assert_almost_eq(hud.parts().health.modulate.a, 0.4, 0.001)
	# Minimal: the parts turned off are not seen.
	Settings.set_value(HudLayout.SETTING, HudLayout.minimal())
	hud._process(0.016)
	assert_eq(hud.parts().feed.modulate.a, 0.0)
	assert_gt(hud.parts().health.modulate.a, 0.0)
	# Back as usual, each part is back where it was.
	Settings.set_value(HudLayout.SETTING, {})
	hud._process(0.016)
	for id: String in usual:
		assert_almost_eq(hud.part_rects()[id].position, usual[id].position, Vector2.ONE * 0.5, id)
		assert_eq(hud.parts()[id].modulate.a, 1.0, id)
	main.world.stop()


func test_the_settings_lay_the_hud_out_a_part_at_a_time() -> void:
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	menu.open()
	var part: Button = menu.find_child("HudPart", true, false)
	assert_eq(part.text, "HUD part: %s" % HudLayout.PARTS.health)
	(menu.find_child("Hud_scale", true, false) as HSlider).value = 1.4
	assert_eq(Settings.get_value(HudLayout.SETTING), {"health": {"scale": 1.4}})
	(menu.find_child("Hud_on", true, false) as CheckButton).toggled.emit(false)
	assert_eq(Settings.get_value(HudLayout.SETTING).health.on, false)
	part.pressed.emit()
	assert_eq(menu.hud_part, "weapon")
	assert_eq((menu.find_child("Hud_scale", true, false) as HSlider).value, 1.0, "the controls show the part chosen")
	assert_true((menu.find_child("Hud_on", true, false) as CheckButton).button_pressed)
	(menu.find_child("Hud_x", true, false) as HSlider).value = -0.25
	assert_eq(Settings.get_value(HudLayout.SETTING).weapon, {"x": -0.25})
	(menu.find_child("HudMinimal", true, false) as Button).pressed.emit()
	assert_eq(Settings.get_value(HudLayout.SETTING), HudLayout.minimal())
	(menu.find_child("HudStandard", true, false) as Button).pressed.emit()
	assert_eq(Settings.get_value(HudLayout.SETTING), {})
	# Round every part and back.
	for press in HudLayout.PARTS.size() - 1:
		part.pressed.emit()
	assert_eq(menu.hud_part, "health")


func test_the_bar_shows_each_weapon_carried_with_its_key() -> void:
	assert_eq(WeaponBar.entries(["pistol"], "pistol"), [{"weapon": "pistol", "key": 1, "held": true}])
	var carried := WeaponBar.entries(["launcher", "pistol", "blaster"], "blaster")
	assert_eq(carried.map(func(entry: Dictionary) -> String: return entry.weapon), ["pistol", "blaster", "launcher"], "in the order of the keys, not as picked up")
	assert_eq(carried.map(func(entry: Dictionary) -> int: return entry.key), [1, 2, 3])
	assert_eq(carried.map(func(entry: Dictionary) -> bool: return entry.held), [false, true, false])
	assert_eq(WeaponBar.entries([], ""), [])
	for entry: Dictionary in WeaponBar.entries(Weapons.ids(), ""):
		assert_true(Icons.has(entry.weapon), "an icon for %s" % entry.weapon)
		assert_eq(Weapons.at_key(Weapons.ids(), entry.key - 1), entry.weapon, "the key that takes it")


func test_the_wheel_offers_the_weapons_carried_and_takes_the_one_picked() -> void:
	var main: Node = await _main_in_practice()
	var hud: Control = main.combat_hud
	var own: Node3D = main.world.player_node(1)
	hud._process(0.016)
	assert_eq(hud.weapon_bar.shown().size(), own.arsenal.size())
	assert_true(hud.weapon_bar.shown().any(func(entry: Dictionary) -> bool: return entry.held and entry.weapon == own.weapon.stats.id))
	hud.open_weapon_wheel()
	assert_true(hud.weapon_wheel.visible)
	assert_eq(hud.weapon_wheel.choices.size(), own.arsenal.size())
	assert_eq(hud.weapon_wheel.icons, hud.weapon_bar.shown().map(func(entry: Dictionary) -> String: return entry.weapon))
	# Push toward the first, at the top, and let go.
	var first: String = hud.weapon_wheel.icons[0]
	hud.weapon_wheel.point(Vector2(0, -1))
	hud.close_weapon_wheel()
	assert_false(hud.weapon_wheel.visible)
	assert_eq(own._switch_to, first)
	# Let go in the middle, nothing changes.
	own._switch_to = ""
	hud.open_weapon_wheel()
	hud.close_weapon_wheel()
	assert_eq(own._switch_to, "")
	own.choose_weapon("kazoo")
	assert_eq(own._switch_to, "", "nor for a weapon that isn't carried")
	# Touching the bar opens it.
	hud.weapon_bar.touched.emit()
	assert_true(hud.weapon_wheel.visible)
	hud.close_weapon_wheel()
	main.world.stop()


func test_frames_a_second_are_counted_over_the_last_while() -> void:
	var readings := Readings.new()
	add_child_autofree(readings)
	readings.set_process(false)
	assert_eq(readings.fps(), 0)
	for frame in 30:
		readings.frame(1.0 / 60.0)
	assert_eq(readings.fps(), 60)
	for frame in Readings.FRAMES:
		readings.frame(1.0 / 30.0)
	assert_eq(readings.fps(), 30, "the older frames are forgotten")
	readings.frame(0.25)
	assert_almost_eq(readings.worst_ms(), 250.0, 0.01)
	readings.frame(0.0)
	readings.frame(-1.0)
	assert_eq(readings.bars().size(), Readings.FRAMES, "and only so many kept")
	# The graph's bars: a frame as long as the graph is tall fills it.
	assert_eq(readings.bars()[-1], 1.0)
	assert_almost_eq(readings.bars()[0], (1000.0 / 30.0) / Readings.GRAPH_TOP_MS, 0.001)


func test_what_goes_astray_is_told_from_the_gaps_in_what_arrives() -> void:
	var readings := Readings.new()
	add_child_autofree(readings)
	readings.set_process(false)
	assert_eq(readings.loss(), 0)
	for second in 10:
		readings.heard(100.0 + second, 1.0)
	assert_eq(readings.loss(), 0, "every one arrived")
	# Two went missing, then one arrived.
	readings.heard(112.0, 1.0)
	assert_eq(readings.loss(), roundi(100.0 * 2 / 13))
	# One that turns up late, out of turn, is still counted as missing.
	readings.heard(110.0, 1.0)
	assert_eq(readings.loss(), roundi(100.0 * 2 / 13))
	# After a long silence the loss is all there is, and it recovers.
	readings.heard(500.0, 1.0)
	assert_gt(readings.loss(), 90)
	for second in Readings.HEARD:
		readings.heard(501.0 + second, 1.0)
	assert_eq(readings.loss(), 0)
	readings.forget_host()
	assert_eq(readings.loss(), 0)


func test_only_the_numbers_asked_for_are_shown() -> void:
	var readings := Readings.new()
	add_child_autofree(readings)
	readings.set_process(false)
	for frame in 10:
		readings.frame(1.0 / 50.0)
	assert_eq(readings.lines({}, 40, false), [])
	assert_eq(readings.lines({Readings.FPS: true}, 40, false), ["50 fps"])
	assert_eq(readings.lines({Readings.FPS: true, Readings.PING: true, Readings.LOSS: true}, 40, false), ["50 fps", "Ping 40 ms", "Loss 0%"])
	assert_eq(readings.lines({Readings.PING: true, Readings.LOSS: true}, 0, true), ["Ping: host", "Loss: host"], "the host has no way to go")
	for key: String in Readings.SETTINGS:
		assert_eq(Settings.DEFAULTS[key], false, "%s is off unless asked for" % key)
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	for key: String in Readings.SETTINGS:
		assert_not_null(menu.find_child("Setting_" + key, true, false), key)


func test_the_hosts_messages_are_counted_as_they_arrive() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var readings := Readings.new(world)
	add_child_autofree(readings)
	Settings.set_value(Readings.LOSS, true)
	world.match_.heard_from_host.emit(10.0)
	world.match_.heard_from_host.emit(10.0 + Match.PING_INTERVAL * 3.0)
	assert_eq(readings.loss(), 50, "two arrived and two between them did not")


func test_a_free_camera_goes_anywhere_alone_and_stays_near_with_others() -> void:
	var eyes := Vector3(0, 2.7, 0)
	assert_eq(PhotoMode.allowed(Vector3(90, 40, -90), eyes, false), Vector3(90, 40, -90), "alone, anywhere")
	var held := PhotoMode.allowed(Vector3(90, 2.7, 0), eyes, true)
	assert_almost_eq(held.distance_to(eyes), PhotoMode.REACH, 0.001, "with others, no further than its reach")
	assert_eq(PhotoMode.allowed(Vector3(2, 3, 1), eyes, true), Vector3(2, 3, 1), "within reach, where it was asked")
	# A wall on the way stops it short.
	var stopped := PhotoMode.allowed(Vector3(5, 2.7, 0), eyes, true, 2.0)
	assert_lt(stopped.distance_to(eyes), 2.0)
	assert_eq(PhotoMode.allowed(eyes, eyes, true), eyes)
	assert_eq(PhotoMode.file_name(1700000000), "cheesestrike-1700000000.png")


func test_photo_mode_frees_the_camera_hides_the_hud_and_holds_the_player() -> void:
	var main: Node = await _main_in_practice()
	var photo: PhotoMode = main.world.photo_mode
	var own: Node3D = main.world.player_node(1)
	assert_false(photo.on)
	assert_true(main.combat_hud.visible)
	assert_false(own.body.shown, "a player doesn't see their own character")
	photo.toggle()
	assert_true(photo.on)
	assert_false(main.combat_hud.visible, "nothing over the picture")
	assert_false(main.hud.visible)
	assert_true(own._menu_open(), "the player stands where they are")
	assert_true(own.body.shown, "and is in the picture")
	var camera := get_viewport().get_camera_3d()
	assert_eq(camera.name, "PhotoCamera")
	assert_false(photo.tethered(), "alone, the camera is free")
	# Out again: the view and the HUD come back.
	photo.toggle()
	await wait_process_frames(2)
	assert_false(photo.on)
	assert_true(main.combat_hud.visible)
	assert_false(own.body.shown)
	assert_ne(get_viewport().get_camera_3d().name, "PhotoCamera")
	assert_false(own._menu_open())
	# Ending the session ends it.
	photo.toggle()
	main.world.stop()
	assert_false(photo.on)


func test_the_photo_key_is_only_a_letter_while_typing_and_on_the_menu() -> void:
	var main: Node = MAIN.instantiate()
	add_child_autofree(main)
	for frame in 2000:
		if not main.loading_cover.visible:
			break
		await wait_process_frames(1)
	main._on_way_in_chosen("play")
	await wait_process_frames(2)
	var key := InputEventKey.new()
	key.physical_keycode = (InputMap.action_get_events(PhotoMode.ACTION)[0] as InputEventKey).physical_keycode
	key.keycode = KEY_P
	key.unicode = "P".unicode_at(0)
	key.pressed = true
	# Typed into the box for a session's code, it is the letter.
	main.target_edit.grab_focus()
	main.target_edit.text = ""
	Input.parse_input_event(key)
	await wait_process_frames(2)
	assert_eq(main.target_edit.text, "P", "the letter arrives in the box")
	assert_false(main.world.photo_mode.on)
	# On the menu with nothing being typed, it does nothing and is not taken.
	main.target_edit.release_focus()
	assert_false(main.world.photo_mode.can_begin())
	main.world.photo_mode._input(key)
	assert_false(main.world.photo_mode.on)
	# In a session it is the photo mode, unless something is being typed.
	main._on_way_in_chosen("practice")
	await wait_physics_frames(4)
	assert_true(main.world.photo_mode.can_begin())
	main.world.photo_mode._input(key)
	assert_true(main.world.photo_mode.on)
	main.world.photo_mode._input(key)
	assert_false(main.world.photo_mode.on)
	main.world.stop()


func test_nobody_takes_a_picture_out_of_photo_mode_or_out_of_a_session() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.photo_mode.toggle()
	assert_false(world.photo_mode.on, "nobody to free the camera from")
	assert_eq(world.photo_mode.take(), 0)
	assert_eq(world.photo_mode.saved_count, 0)
	for action: String in ["photo", "weapon_wheel"]:
		assert_true(InputMap.has_action(action))
		assert_has(Controls.REBINDABLE, action)
