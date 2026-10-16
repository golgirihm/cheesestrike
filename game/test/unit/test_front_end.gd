extends GutTest
## The look of the interface, its icons, the title screen and the paths from
## it, the bar that fills as the game gets ready, and the screen shown while
## a session is joined.

const MAIN := preload("res://main.tscn")

var main: Node
var saved := {}
var base := Vector2(
	ProjectSettings.get_setting("display/window/size/viewport_width"),
	ProjectSettings.get_setting("display/window/size/viewport_height"),
)


func before_each() -> void:
	for key: String in ["seen_help", "camera_motion", "tips_seen"]:
		saved[key] = Settings.get_value(key)
	Settings.set_value("seen_help", true)
	main = MAIN.instantiate()
	add_child_autofree(main)
	for frame in 2000:
		if not main.loading_cover.visible:
			break
		await wait_process_frames(1)
	await wait_process_frames(2)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func test_every_screen_has_the_same_look() -> void:
	var look := Look.theme()
	assert_same(Look.theme(), look, "one theme, made once")
	for screen in main.get_node("UI").get_children():
		if screen is Control:
			assert_same(screen.theme, look, screen.name)
	# A button and a panel are drawn its way wherever they are.
	var button: Button = main.host_button
	assert_true(button.get_theme_stylebox("normal") is StyleBoxFlat)
	assert_true((main.menu.get_node("Panel") as PanelContainer).get_theme_stylebox("panel") is StyleBoxFlat)
	assert_eq(main.front_end.button("credits").get_theme_color("font_color"), Look.CREAM)
	for kind: String in ["Button", "CheckButton"]:
		for state: String in ["normal", "hover", "pressed", "focus", "disabled"]:
			assert_true(look.has_stylebox(state, kind), "%s %s" % [kind, state])
	for kind: String in ["PanelContainer", "LineEdit", "HSlider", "ProgressBar"]:
		assert_gt(look.get_stylebox_list(kind).size(), 0, kind)


func test_words_can_be_read_on_what_they_are_written_on() -> void:
	# Cream on the rind of a panel, and the rind's colour on a pressed button.
	var on_panel := absf(Look.CREAM.get_luminance() - Look.RIND.get_luminance())
	var on_pressed := absf(Look.RIND.get_luminance() - Look.CHEDDAR.darkened(0.25).get_luminance())
	assert_gt(on_panel, 0.6)
	assert_gt(on_pressed, 0.3)
	assert_gte(Look.TEXT_SIZE, 15, "not small")
	assert_gt(Look.TITLE_SIZE, Look.HEADING_SIZE)


func test_a_screen_fades_in_unless_motion_is_turned_off() -> void:
	Settings.set_value("camera_motion", 1.0)
	var screen := Control.new()
	add_child_autofree(screen)
	screen.hide()
	var motion := Look.arrive(screen)
	assert_not_null(motion)
	assert_true(screen.visible)
	assert_eq(screen.modulate.a, 0.0, "from nothing")
	await wait_seconds(Look.ARRIVE_TIME + 0.1)
	assert_eq(screen.modulate.a, 1.0)
	assert_lt(Look.ARRIVE_TIME, 0.3, "quick: nobody waits for a menu")
	Settings.set_value("camera_motion", 0.0)
	screen.hide()
	assert_null(Look.arrive(screen), "nothing moves")
	assert_true(screen.visible)
	assert_eq(screen.modulate.a, 1.0, "it is simply there")


func test_there_is_an_icon_for_every_weapon_pickup_and_way_in() -> void:
	for name: String in Icons.names():
		assert_true(Icons.has(name), name)
		# Enough of it to be seen, and not a filled square.
		assert_between(Icons.cover(name), 0.06, 0.7, name)
	for weapon: String in Weapons.ids():
		assert_has(Icons.names(), weapon)
	for choice: Array in FrontEnd.CHOICES:
		assert_true(Icons.has(choice[3]), choice[0])
	assert_false(Icons.has("kazoo"))
	assert_null(Icons.texture("kazoo"))
	# No two are the same picture.
	var seen := {}
	for name: String in Icons.names():
		var key := str(Icons._shapes(name))
		assert_false(seen.has(key), "%s is the same as %s" % [name, seen.get(key, "")])
		seen[key] = name


func test_an_icon_is_drawn_at_the_size_and_in_the_colour_asked() -> void:
	var small := Icons.texture("play", 24, Color.RED)
	assert_eq(small.get_size(), Vector2(24, 24))
	assert_same(Icons.texture("play", 24, Color.RED), small, "made once")
	var image := small.get_image()
	# A triangle pointing right: filled in the middle, empty in the corners.
	assert_eq(image.get_pixel(12, 12), Color.RED)
	assert_eq(image.get_pixel(1, 1).a, 0.0)
	assert_eq(image.get_pixel(22, 1).a, 0.0)
	assert_eq(Icons.texture("play").get_size(), Vector2(Icons.SIZE, Icons.SIZE))
	# Its edges are smooth: some pixels are partly covered.
	var soft := 0
	for y in 24:
		for x in 24:
			var alpha := image.get_pixel(x, y).a
			soft += 1 if alpha > 0.05 and alpha < 0.95 else 0
	assert_gt(soft, 6)


func test_where_two_shapes_of_an_icon_overlap_there_is_a_hole() -> void:
	var ring := [Icons._round(0.5, 0.5, 0.4), Icons._round(0.5, 0.5, 0.2)]
	assert_true(Icons.covers(ring, Vector2(0.5, 0.2)))
	assert_false(Icons.covers(ring, Vector2(0.5, 0.5)), "the middle of a ring")
	assert_false(Icons.covers(ring, Vector2(0.02, 0.02)))


func test_the_game_opens_on_its_title_with_every_way_in() -> void:
	assert_true(main.front_end.visible)
	assert_true(main.front_end.ways.visible)
	assert_false(main.front_end.credits.visible)
	assert_false(main.menu.visible, "hosting and joining are behind Play")
	var title: Label = main.front_end.find_child("Title", true, false)
	assert_eq(title.text, "CheeseStrike")
	assert_eq((main.front_end.find_child("Subtitle", true, false) as Label).text, "Gouda Overload")
	assert_not_null((main.front_end.find_child("Wedge", true, false) as TextureRect).texture)
	for choice: Array in FrontEnd.CHOICES:
		var button: Button = main.front_end.button(choice[0])
		assert_not_null(button, choice[0])
		assert_eq(button.name, choice[2])
		assert_eq(button.text, choice[1])
		assert_not_null(button.icon, "%s has its icon" % choice[0])
		assert_true(button.is_visible_in_tree())
	assert_eq(FrontEnd.CHOICES[0][0], "play", "Play comes first")
	assert_null(main.front_end.button("kazoo"))


func test_play_leads_to_hosting_and_joining_and_back() -> void:
	main.front_end.button("play").pressed.emit()
	assert_true(main.menu.visible)
	assert_false(main.front_end.visible)
	assert_true(main.host_button.is_visible_in_tree())
	var back: Button = main.menu.find_child("BackButton", true, false)
	assert_true(back.is_visible_in_tree())
	back.pressed.emit()
	assert_true(main.front_end.visible)
	assert_false(main.menu.visible)


func test_customise_shows_the_character_and_weapons_and_settings_the_rest() -> void:
	var settings: Control = main.settings_menu
	main.front_end.button("customise").pressed.emit()
	assert_true(settings.visible)
	assert_eq((settings.find_child("SettingsTitle", true, false) as Label).text, "Customise")
	var tabs := func() -> Array:
		return settings.TABS.map(func(tab: Array) -> String: return tab[0]).filter(func(id: String) -> bool:
			return (settings.find_child("SettingsTab_" + id, true, false) as Control).visible)
	assert_eq(tabs.call(), ["character", "weapons"])
	for key: String in Looks.SETTINGS.keys():
		assert_eq(settings.group_of(key), "character", key)
		assert_true((settings.find_child("Setting_" + key, true, false) as Control).is_visible_in_tree(), key)
	for key: String in settings.LOADOUT.keys():
		assert_eq(settings.group_of(key), "weapons", key)
	settings.close()
	assert_true(main.front_end.visible, "back at the title")
	main.front_end.button("settings").pressed.emit()
	assert_eq((settings.find_child("SettingsTitle", true, false) as Label).text, "Settings")
	assert_eq(tabs.call(), ["aim", "controls", "keys", "sound", "graphics", "hud", "access"])
	assert_true((settings.find_child("Setting_fov", true, false) as Control).is_visible_in_tree(), "the first tab's")
	for key: String in ["fov", "master_volume", "quality", "captions"]:
		assert_ne(settings.group_of(key), "", key)
	(settings.find_child("SettingsTab_keys", true, false) as Button).pressed.emit()
	assert_true((settings.find_child("Key_reload", true, false) as Control).is_visible_in_tree())
	assert_false((settings.find_child("Setting_fov", true, false) as Control).is_visible_in_tree(), "one tab at a time")
	settings.close()
	# In a session, where there is one button for it, everything is there.
	settings.open()
	assert_eq(tabs.call().size(), settings.TABS.size())


func test_credits_say_what_the_game_is_made_with_and_come_back() -> void:
	main.front_end.button("credits").pressed.emit()
	assert_true(main.front_end.credits.visible)
	assert_false(main.front_end.ways.visible)
	var text: String = (main.front_end.credits.find_child("Text", true, false) as Label).text
	assert_string_contains(text, "Godot")
	assert_string_contains(text, "MIT")
	(main.front_end.credits.find_child("CreditsBackButton", true, false) as Button).pressed.emit()
	assert_false(main.front_end.credits.visible)
	assert_true(main.front_end.ways.visible)


func test_the_arena_behind_the_title_is_alive() -> void:
	var view: Transform3D = main.world.menu_camera.transform
	main.world.drift_menu_view(12.0)
	var drifted: Transform3D = main.world.menu_camera.transform
	assert_ne(drifted, view, "the view has turned")
	# Round the middle of the arena: as far from it as it was, and as high.
	assert_almost_eq(drifted.origin.length(), view.origin.length(), 0.01)
	assert_almost_eq(drifted.origin.y, view.origin.y, 0.001)
	# A slow swing there and back, never far.
	var widest := 0.0
	for second in range(0, int(main.world.DRIFT_TIME) + 1):
		main.world.drift_menu_view(second)
		widest = maxf(widest, main.world.menu_camera.transform.basis.get_euler().y - main.world._menu_view.basis.get_euler().y)
	assert_almost_eq(widest, main.world.DRIFT, 0.01)
	main.world.drift_menu_view(main.world.DRIFT_TIME)
	assert_almost_eq(main.world.menu_camera.transform.origin, main.world._menu_view.origin, Vector3.ONE * 0.01, "and back where it began")
	# Not for someone who has turned motion off.
	Settings.set_value("camera_motion", 0.0)
	main.world.drift_menu_view(12.0)
	assert_almost_eq(main.world.menu_camera.transform.origin, main.world._menu_view.origin, Vector3.ONE * 0.01)


func test_every_screen_fits_inside_the_base_size() -> void:
	main.join_screen.open("Joining ABCD…", "kitchen")
	main.front_end.show_credits(true)
	await wait_process_frames(2)
	var panels := {
		"the title's ways in": main.front_end.ways.get_child(0),
		"the credits": main.front_end.credits.get_child(0),
		"the joining screen": main.join_screen.find_child("Panel", true, false),
		"hosting and joining": main.menu.get_node("Panel"),
	}
	for part: String in [main.settings_menu.CUSTOMISE, main.settings_menu.SETTINGS, main.settings_menu.EVERYTHING]:
		main.settings_menu.open_part(part)
		await wait_process_frames(1)
		var needed: Vector2 = main.settings_menu.get_child(0).get_combined_minimum_size()
		assert_lte(needed.x, base.x, "settings (%s) is too wide" % part)
		assert_lte(needed.y, base.y, "settings (%s) is too tall" % part)
	for name: String in panels:
		var needed: Vector2 = panels[name].get_combined_minimum_size()
		assert_lte(needed.x, base.x, "%s is too wide" % name)
		assert_lte(needed.y, base.y, "%s is too tall" % name)


func test_the_bar_fills_with_the_sounds_made_and_the_first_drawing() -> void:
	assert_eq(main.loading_progress(0.0, false), 0.0)
	assert_eq(main.loading_progress(1.0, true), 1.0)
	assert_lt(main.loading_progress(1.0, false), 1.0, "not full until one of everything has been drawn")
	assert_gt(main.loading_progress(0.5, false), main.loading_progress(0.25, false))
	assert_eq(main.loading_progress(7.0, true), 1.0)
	# As the game got ready just now, it only went forward, and ended full.
	assert_gt(main.loading_seen.size(), 0)
	var sorted: Array = main.loading_seen.duplicate()
	sorted.sort()
	assert_eq(main.loading_seen, sorted)
	assert_eq(main.loading_seen[-1], 1.0)
	assert_eq(SoundBank.made_share(), 1.0)
	assert_not_null(main.loading_cover.find_child("LoadingBar", true, false))


func test_joining_shows_the_arena_a_picture_and_a_tip() -> void:
	Settings.set_value("tips_seen", 0)
	var screen: JoinScreen = main.join_screen
	screen.open("Joining ABCD…")
	assert_true(screen.visible)
	assert_eq(screen.showing().arena, JoinScreen.NOT_KNOWN)
	assert_null((screen.find_child("Picture", true, false) as TextureRect).texture)
	assert_eq(screen.showing().tip, JoinScreen.TIPS[0])
	screen.set_arena("city")
	assert_eq(screen.showing().arena, "Curd City")
	assert_same((screen.find_child("Picture", true, false) as TextureRect).texture, Textures.picture(Textures.ASPHALT), "what the arena is made of")
	screen.set_arena("moon")
	assert_eq(screen.showing().arena, JoinScreen.NOT_KNOWN)
	# Each tip in its turn, and round again.
	var tips := []
	for turn in JoinScreen.TIPS.size() + 1:
		screen.open("Joining…", "box")
		tips.append(screen.showing().tip)
	assert_eq(tips.slice(0, -1), JoinScreen.TIPS.slice(1) + [JoinScreen.TIPS[0]])
	for tip: String in JoinScreen.TIPS:
		assert_lt(tip.length(), 110, "short enough to read while joining")
	for id: String in Maps.ORDER:
		assert_has(Textures.KINDS, JoinScreen.ground(id), id)


func test_hosting_shows_the_arena_until_it_is_there_and_then_goes() -> void:
	main.front_end.button("play").pressed.emit()
	Settings.set_value("host_map", "box")
	main._set_busy("Starting session…", "box")
	assert_true(main.join_screen.visible)
	assert_eq(main.join_screen.showing().arena, "Brie Box")
	# A session that fails takes it away.
	main._on_session_failed("No such session")
	assert_false(main.join_screen.visible)
	assert_true(main.menu.visible)


func test_a_screen_too_short_for_a_panel_scrolls_it_instead_of_cutting_it_off() -> void:
	var saved: float = Settings.get_value(Look.MENU_TEXT)
	Settings.set_value(Look.MENU_TEXT, Look.MOST_TEXT)
	Look.refresh()
	main._on_way_in_chosen("play")
	main.sessions_panel.show()
	# A long session list, more than any screen holds.
	var listed := []
	for index in 30:
		listed.append({"code": "AB%02d" % index, "players": 1, "settings": {}})
	main._show_sessions(listed)
	await wait_process_frames(3)
	var screen: Rect2 = main.menu.get_viewport_rect()
	var panel: Control = main.menu.get_node("Panel")
	var scroll: FitScroll = panel.find_children("*", "FitScroll", true, false)[0]
	assert_true(scroll.scrolls(), "the list scrolls")
	assert_lte(panel.size.y, screen.size.y, "and the panel fits on the screen")
	assert_true(screen.encloses(panel.get_global_rect()), "with nothing cut off at the top or bottom")
	Settings.set_value(Look.MENU_TEXT, 1.0)
	Look.refresh()
	main._show_sessions([])
	await wait_process_frames(3)
	assert_false(scroll.scrolls(), "a short list doesn't")
	Settings.set_value(Look.MENU_TEXT, saved)
	Look.refresh()


func test_nothing_in_a_box_touches_its_edge() -> void:
	var theme := Look.theme()
	for kind: Array in [["panel", "PanelContainer"], ["panel", "PopupMenu"], ["panel", "TooltipPanel"], ["normal", "LineEdit"], ["normal", "Button"]]:
		var box: StyleBox = theme.get_stylebox(kind[0], kind[1])
		for side in [SIDE_LEFT, SIDE_RIGHT]:
			assert_gte(box.get_content_margin(side), 8.0, "%s %s" % kind)
		for side in [SIDE_TOP, SIDE_BOTTOM]:
			assert_gte(box.get_content_margin(side), 4.0, "%s %s" % kind)
	# Nor what is written over the game: each line of the kill feed and the
	# chat are boxed too.
	var feed: VBoxContainer = preload("res://ui/kill_feed.gd").new()
	add_child_autofree(feed)
	feed.add("Brie", "Edam", "blaster", false)
	var entry: Node = feed.get_child(feed.get_child_count() - 1)
	assert_true(entry is PanelContainer)
	assert_gte((entry as PanelContainer).get_theme_stylebox("panel").get_content_margin(SIDE_LEFT), 8.0)
