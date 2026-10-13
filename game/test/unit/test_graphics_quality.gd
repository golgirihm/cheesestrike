extends GutTest
## The parts of the graphics chosen one by one under the Low, Medium and High
## levels, and the render scale that drops to hold the frame rate.

const WORLD := preload("res://world/world.tscn")
const SETTINGS_MENU := preload("res://ui/settings_menu.gd")
const LOW := GraphicsQuality.LOW
const MEDIUM := GraphicsQuality.MEDIUM
const HIGH := GraphicsQuality.HIGH

var saved := {}


func before_each() -> void:
	for key: String in _keys():
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _keys() -> Array:
	var keys := ["quality", GraphicsQuality.DYNAMIC]
	for part: String in GraphicsQuality.PARTS:
		keys.append(GraphicsQuality.PARTS[part].setting)
	return keys


func test_every_part_is_a_setting_with_a_choice_for_each_level() -> void:
	assert_eq(GraphicsQuality.ORDER.size(), GraphicsQuality.PARTS.size())
	for part: String in GraphicsQuality.ORDER:
		var known: Dictionary = GraphicsQuality.PARTS[part]
		assert_eq(Settings.DEFAULTS[known.setting], "", "%s starts as the level has it" % part)
		assert_eq(Settings.CHOICES[known.setting], [""] + known.choices.keys(), "%s: the settings take just these" % part)
		assert_gte(known.choices.size(), 2, part)
		for level: String in GraphicsQuality.LEVELS:
			assert_has(known.choices, known.levels[level], "%s on %s" % [part, level])


func test_a_part_left_alone_is_as_the_level_has_it() -> void:
	assert_eq(GraphicsQuality.resolve(GraphicsQuality.SHADOWS, LOW, ""), "off")
	assert_eq(GraphicsQuality.resolve(GraphicsQuality.SHADOWS, MEDIUM, ""), "on")
	assert_eq(GraphicsQuality.resolve(GraphicsQuality.RENDER_SCALE_PART, LOW, ""), "60")
	assert_eq(GraphicsQuality.resolve(GraphicsQuality.ANTIALIASING, HIGH, ""), "2x")
	for part: String in GraphicsQuality.PARTS:
		assert_eq(GraphicsQuality.resolve(part, "nonsense", ""), GraphicsQuality.PARTS[part].levels[HIGH], part)
		assert_eq(GraphicsQuality.resolve(part, LOW, "nonsense"), GraphicsQuality.PARTS[part].levels[LOW], part)


func test_a_part_chosen_holds_whatever_the_level() -> void:
	for level: String in GraphicsQuality.LEVELS:
		assert_eq(GraphicsQuality.resolve(GraphicsQuality.SHADOWS, level, "on"), "on")
		assert_eq(GraphicsQuality.resolve(GraphicsQuality.SHADOWS, level, "off"), "off")
		assert_eq(GraphicsQuality.resolve(GraphicsQuality.PARTICLES, level, HIGH), HIGH)
		assert_eq(GraphicsQuality.render_scale(level, "70"), 0.7)


func test_lower_levels_never_ask_for_more() -> void:
	for part: String in GraphicsQuality.ORDER:
		var choices: Array = GraphicsQuality.PARTS[part].choices.keys()
		var levels: Dictionary = GraphicsQuality.PARTS[part].levels
		assert_lte(choices.find(levels[LOW]), choices.find(levels[MEDIUM]), part)
		assert_lte(choices.find(levels[MEDIUM]), choices.find(levels[HIGH]), part)
	for level: String in GraphicsQuality.LEVELS:
		assert_almost_eq(GraphicsQuality.render_scale(level), GraphicsQuality.RENDER_SCALE[level], 0.0001, level)


func test_the_button_says_what_is_chosen_and_when_it_is_the_levels() -> void:
	assert_eq(GraphicsQuality.part_text(GraphicsQuality.SHADOWS, MEDIUM, ""), "Shadows: On (as Medium)")
	assert_eq(GraphicsQuality.part_text(GraphicsQuality.SHADOWS, MEDIUM, "off"), "Shadows: Off")
	assert_eq(GraphicsQuality.part_text(GraphicsQuality.RENDER_SCALE_PART, LOW, ""), "Render scale: 60% (as Low)")
	assert_eq(GraphicsQuality.part_text(GraphicsQuality.VIEW_DISTANCE, HIGH, "near"), "View distance: Near")


func test_the_button_goes_through_every_choice_and_back_to_the_levels() -> void:
	for part: String in GraphicsQuality.ORDER:
		var seen := []
		var chosen := ""
		for press in GraphicsQuality.PARTS[part].choices.size() + 1:
			chosen = GraphicsQuality.next_choice(part, chosen)
			seen.append(chosen)
		assert_eq(seen, GraphicsQuality.PARTS[part].choices.keys() + [""], part)
	assert_eq(GraphicsQuality.next_choice(GraphicsQuality.SHADOWS, "nonsense"), "off")


func test_the_settings_screen_has_a_button_for_each_part() -> void:
	Settings.set_value("quality", MEDIUM)
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	for part: String in GraphicsQuality.ORDER:
		var setting: String = GraphicsQuality.PARTS[part].setting
		var button: Button = menu.find_child("Setting_" + setting, true, false)
		assert_not_null(button, part)
		assert_eq(button.text, GraphicsQuality.part_text(part, MEDIUM, ""))
		button.pressed.emit()
		var first: String = GraphicsQuality.PARTS[part].choices.keys()[0]
		assert_eq(Settings.get_value(setting), first)
		assert_eq(button.text, GraphicsQuality.part_text(part, MEDIUM, first), "and says so at once")
	# Changing the level changes what the parts left alone say.
	Settings.set_value("gfx_shadows", "")
	Settings.set_value("quality", LOW)
	assert_eq((menu.find_child("Setting_gfx_shadows", true, false) as Button).text, "Shadows: Off (as Low)")
	assert_not_null(menu.find_child("Setting_" + GraphicsQuality.DYNAMIC, true, false))


func test_each_part_is_put_into_effect_on_its_own() -> void:
	var viewport := SubViewport.new()
	var sun := DirectionalLight3D.new()
	add_child_autofree(viewport)
	add_child_autofree(sun)
	GraphicsQuality.apply(LOW, viewport, sun, {GraphicsQuality.SHADOWS: "on"})
	assert_true(sun.shadow_enabled, "shadows on Low")
	assert_almost_eq(viewport.scaling_3d_scale, 0.6, 0.0001, "and the rest still Low")
	assert_eq(viewport.msaa_3d, Viewport.MSAA_DISABLED)
	GraphicsQuality.apply(HIGH, viewport, sun, {GraphicsQuality.SHADOWS: "off", GraphicsQuality.RENDER_SCALE_PART: "50", GraphicsQuality.ANTIALIASING: "4x", GraphicsQuality.FILTERING: "off"})
	assert_false(sun.shadow_enabled)
	assert_almost_eq(viewport.scaling_3d_scale, 0.5, 0.0001)
	assert_eq(viewport.msaa_3d, Viewport.MSAA_4X)
	assert_eq(viewport.anisotropic_filtering_level, Viewport.ANISOTROPY_DISABLED)
	GraphicsQuality.apply(HIGH, viewport, sun)
	assert_true(sun.shadow_enabled)
	assert_eq(viewport.scaling_3d_scale, 1.0)
	assert_eq(viewport.msaa_3d, Viewport.MSAA_2X)
	assert_eq(viewport.anisotropic_filtering_level, Viewport.ANISOTROPY_16X)
	GraphicsQuality.apply(MEDIUM, viewport, sun)
	assert_eq(viewport.anisotropic_filtering_level, Viewport.ANISOTROPY_4X)


func test_the_arena_follows_each_setting_as_it_changes() -> void:
	Settings.set_value("quality", LOW)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var sun: DirectionalLight3D = world.get_node("Sun")
	assert_false(sun.shadow_enabled)
	Settings.set_value("gfx_shadows", "on")
	assert_true(sun.shadow_enabled)
	assert_true(GraphicsQuality.shadows_on())
	Settings.set_value("gfx_render_scale", "90")
	assert_almost_eq(get_viewport().scaling_3d_scale, 0.9, 0.0001)
	Settings.set_value("gfx_render_scale", "")
	assert_almost_eq(get_viewport().scaling_3d_scale, 0.6, 0.0001)
	# Particles and effects each on their own.
	world.load_map("box")
	await wait_physics_frames(2)
	world.effects.landing(Vector3.ZERO, 1.0)
	assert_eq(world.effects.specks_showing(), 0, "no dust on Low")
	Settings.set_value("gfx_particles", HIGH)
	world.effects.landing(Vector3.ZERO, 1.0)
	assert_gt(world.effects.specks_showing(), 0, "dust with particles turned up")
	world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)], Vector3.RIGHT)
	assert_eq(world.effects.casings_showing(), 0, "but effects are still Low: no casings")
	Settings.set_value("gfx_effects", HIGH)
	world.effects.show_shot(Vector3(0, 3, 0), [Vector3(0, 0, -4)], Vector3.RIGHT)
	assert_eq(world.effects.casings_showing(), 1)
	# The glow follows the effects.
	assert_true(world.get_node("WorldEnvironment").environment.glow_enabled)
	Settings.set_value("gfx_effects", LOW)
	assert_false(world.get_node("WorldEnvironment").environment.glow_enabled)


func test_the_view_reaches_as_far_as_is_chosen() -> void:
	assert_lt(GraphicsQuality.VIEW_METRES.near, GraphicsQuality.VIEW_METRES.middle)
	assert_lt(GraphicsQuality.VIEW_METRES.middle, GraphicsQuality.VIEW_METRES.far)
	# No level cuts off the scenery, which reaches this far past the middle
	# of the biggest arena: only a player choosing "Near" does.
	assert_gt(GraphicsQuality.VIEW_METRES.middle, Scenery.REACH + 60.0)
	for level: String in GraphicsQuality.LEVELS:
		assert_ne(GraphicsQuality.resolve(GraphicsQuality.VIEW_DISTANCE, level, ""), "near", level)
	Settings.set_value("quality", HIGH)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	await wait_seconds(0.4)
	var camera := get_viewport().get_camera_3d()
	assert_eq(camera.far, GraphicsQuality.VIEW_METRES.far)
	Settings.set_value("gfx_view_distance", "near")
	await wait_seconds(0.4)
	assert_eq(camera.far, GraphicsQuality.VIEW_METRES.near)


func test_dynamic_resolution_drops_when_frames_are_slow_and_comes_back() -> void:
	var target := 1.0 / GraphicsQuality.HOLD_FPS
	assert_almost_eq(GraphicsQuality.dynamic_scale(1.0, target * 1.5, 1.0), 1.0 - GraphicsQuality.DYNAMIC_STEP, 0.0001)
	assert_eq(GraphicsQuality.dynamic_scale(0.8, target, 1.0), 0.8, "holding: it stays")
	assert_eq(GraphicsQuality.dynamic_scale(0.8, target * 0.95, 1.0), 0.8, "a little room is not enough to go back up, or it would hunt")
	assert_almost_eq(GraphicsQuality.dynamic_scale(0.8, target * 0.5, 1.0), 0.85, 0.0001)
	# Never above what was chosen, nor below its floor.
	assert_eq(GraphicsQuality.dynamic_scale(1.0, target * 0.1, 1.0), 1.0)
	assert_eq(GraphicsQuality.dynamic_scale(0.8, target * 0.1, 0.8), 0.8)
	var scale := 0.8
	for check in 100:
		scale = GraphicsQuality.dynamic_scale(scale, 1.0, 0.8)
	assert_almost_eq(scale, 0.8 * GraphicsQuality.DYNAMIC_FLOOR, 0.0001)
	for check in 100:
		scale = GraphicsQuality.dynamic_scale(scale, 0.001, 0.8)
	assert_almost_eq(scale, 0.8, 0.0001)


func test_the_arena_lowers_its_render_scale_only_when_that_is_wanted() -> void:
	Settings.set_value("quality", HIGH)
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.set_process(false)
	# Frames taking a tenth of a second each.
	for frame in 12:
		world._hold_frame_rate(0.1)
	assert_eq(get_viewport().scaling_3d_scale, 1.0, "not unless asked")
	Settings.set_value(GraphicsQuality.DYNAMIC, true)
	for frame in 12:
		world._hold_frame_rate(0.1)
	assert_almost_eq(get_viewport().scaling_3d_scale, 1.0 - 2.0 * GraphicsQuality.DYNAMIC_STEP, 0.0001, "a step each half second")
	for frame in 600:
		world._hold_frame_rate(0.1)
	assert_almost_eq(get_viewport().scaling_3d_scale, GraphicsQuality.DYNAMIC_FLOOR, 0.0001)
	# Quick frames bring it back, to what was chosen and no further.
	for frame in 4000:
		world._hold_frame_rate(0.005)
	assert_eq(get_viewport().scaling_3d_scale, 1.0)
	# Turning it off, or choosing a scale, puts the chosen scale straight back.
	for frame in 12:
		world._hold_frame_rate(0.1)
	Settings.set_value(GraphicsQuality.DYNAMIC, false)
	assert_eq(get_viewport().scaling_3d_scale, 1.0)
