extends GutTest
## Shade in corners, glow on bright things, each arena's cast of colour and
## the darker corners of the view, and the settings that turn each off.

const WORLD := preload("res://world/world.tscn")
const KEYS := [Picture.BLOOM, Picture.GRADE, Picture.VIGNETTE]

var saved := {}


func before_each() -> void:
	for key: String in KEYS + ["quality"]:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])
	Settings.set_value("quality", GraphicsQuality.HIGH)


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _all(on: bool) -> Dictionary:
	return {Picture.BLOOM: on, Picture.GRADE: on, Picture.VIGNETTE: on}


func test_each_is_a_setting_that_starts_on_and_has_a_switch() -> void:
	var menu: Control = preload("res://ui/settings_menu.gd").new()
	add_child_autofree(menu)
	for key: String in KEYS:
		assert_eq(Settings.DEFAULTS[key], true, key)
		var toggle: CheckButton = menu.find_child("Setting_" + key, true, false)
		assert_not_null(toggle, key)
		assert_true(toggle.button_pressed, key)
		toggle.toggled.emit(false)
		assert_false(Settings.get_value(key), "switched off from the menu: %s" % key)
	assert_eq(Picture.wanted(), _all(false))


func test_every_arena_has_a_cast_that_leaves_the_picture_readable() -> void:
	for id: String in Maps.ORDER:
		assert_has(Picture.GRADES, id)
	for id: String in Picture.GRADES:
		assert_true(Maps.exists(id), id)
		var cast: Dictionary = Picture.GRADES[id]
		assert_between(cast.brightness, 0.9, 1.1, id)
		assert_between(cast.contrast, 0.95, 1.2, id)
		assert_between(cast.saturation, 0.8, 1.25, id)
		# A tint, not a colour wash: white stays nearly white.
		for part: float in [cast.tint.r, cast.tint.g, cast.tint.b]:
			assert_between(part, 0.85, 1.0, id)
	assert_eq(Picture.grade(Maps.RANGE), Picture.PLAIN, "one with none of its own is left alone")
	assert_eq(Picture.grade("moon"), Picture.PLAIN)
	# No two arenas next to each other in the list look the same.
	for index in Maps.ORDER.size() - 1:
		assert_ne(Picture.grade(Maps.ORDER[index]), Picture.grade(Maps.ORDER[index + 1]))


func test_low_graphics_has_no_glow_whatever_is_asked() -> void:
	assert_false(Picture.glows(GraphicsQuality.LOW, true))
	assert_true(Picture.glows(GraphicsQuality.MEDIUM, true))
	assert_true(Picture.glows(GraphicsQuality.HIGH, true))
	for level: String in GraphicsQuality.LEVELS:
		assert_false(Picture.glows(level, false), level)


func test_corners_are_shaded_on_all_but_low_graphics() -> void:
	assert_false(Picture.shades(GraphicsQuality.LOW))
	assert_true(Picture.shades(GraphicsQuality.MEDIUM))
	assert_true(Picture.shades(GraphicsQuality.HIGH))
	# A player is three times the height of a person, so the shade reaches
	# further than it would in a room, but not across one.
	assert_between(Picture.SHADE_RADIUS, 1.5, 4.0)


func test_the_desktop_draws_with_the_renderer_that_can_shade_and_the_web_with_the_one_it_can_run() -> void:
	assert_eq(ProjectSettings.get_setting("rendering/renderer/rendering_method"), "forward_plus")
	assert_eq(ProjectSettings.get_setting("rendering/renderer/rendering_method.web"), "gl_compatibility")


func test_the_picture_is_put_into_the_environment() -> void:
	var environment := Environment.new()
	Picture.apply(environment, "city", GraphicsQuality.HIGH, _all(true))
	assert_true(environment.ssao_enabled)
	assert_almost_eq(environment.ssao_radius, Picture.SHADE_RADIUS, 0.0001)
	assert_true(environment.glow_enabled)
	assert_eq(environment.glow_intensity, Picture.GLOW_INTENSITY)
	assert_true(environment.adjustment_enabled)
	assert_almost_eq(environment.adjustment_saturation, float(Picture.GRADES.city.saturation), 0.0001)
	assert_almost_eq(environment.adjustment_contrast, float(Picture.GRADES.city.contrast), 0.0001)
	var ramp: Gradient = (environment.adjustment_color_correction as GradientTexture1D).gradient
	assert_eq(ramp.get_color(0), Color.BLACK, "black stays black")
	assert_eq(ramp.get_color(1), Picture.GRADES.city.tint, "and white takes the tint")
	# With everything off the picture is as it was drawn.
	Picture.apply(environment, "city", GraphicsQuality.HIGH, _all(false))
	assert_false(environment.glow_enabled)
	assert_false(environment.adjustment_enabled)
	assert_null(environment.adjustment_color_correction)
	assert_eq(environment.adjustment_saturation, 1.0)
	# Each on its own.
	Picture.apply(environment, "city", GraphicsQuality.HIGH, {Picture.BLOOM: true, Picture.GRADE: false})
	assert_true(environment.glow_enabled)
	assert_false(environment.adjustment_enabled)
	Picture.apply(environment, "city", GraphicsQuality.LOW, _all(true))
	assert_false(environment.glow_enabled, "no glow on low")
	assert_false(environment.ssao_enabled, "nor shade in corners")
	assert_true(environment.adjustment_enabled, "but the cast costs nothing")
	Picture.apply(environment, Maps.RANGE, GraphicsQuality.HIGH, _all(true))
	assert_false(environment.adjustment_enabled, "nothing to do for a plain arena")


func test_the_arena_being_played_sets_the_cast_and_settings_change_it_at_once() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var environment: Environment = world.get_node("WorldEnvironment").environment
	assert_true(environment.glow_enabled)
	assert_almost_eq(environment.adjustment_saturation, float(Picture.GRADES[Maps.DEFAULT].saturation), 0.0001)
	world.load_map("cave")
	assert_almost_eq(environment.adjustment_saturation, float(Picture.GRADES.cave.saturation), 0.0001)
	assert_almost_eq(environment.adjustment_contrast, float(Picture.GRADES.cave.contrast), 0.0001)
	Settings.set_value(Picture.GRADE, false)
	assert_false(environment.adjustment_enabled)
	Settings.set_value(Picture.BLOOM, false)
	assert_false(environment.glow_enabled)
	Settings.set_value(Picture.BLOOM, true)
	assert_true(environment.glow_enabled)
	Settings.set_value("quality", GraphicsQuality.LOW)
	assert_false(environment.glow_enabled, "lowering the graphics takes the glow away")
	Settings.set_value("quality", GraphicsQuality.MEDIUM)
	assert_true(environment.glow_enabled)


func test_the_corners_are_darkened_by_a_layer_under_the_interface() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	await wait_process_frames(1)
	assert_true(world.vignette.visible)
	assert_lt(world.vignette.layer, 0, "under the interface, which is on the layers from 0")
	var shade: ColorRect = world.vignette.get_node("Shade")
	assert_eq(shade.mouse_filter, Control.MOUSE_FILTER_IGNORE, "it takes no clicks")
	assert_eq(shade.get_global_rect().size, get_viewport().get_visible_rect().size, "over the whole view")
	assert_eq((shade.material as ShaderMaterial).get_shader_parameter("strength"), Picture.VIGNETTE_STRENGTH)
	assert_lt(Picture.VIGNETTE_STRENGTH, 0.5, "a shade, not a tunnel")
	Settings.set_value(Picture.VIGNETTE, false)
	assert_false(world.vignette.visible)
	Settings.set_value(Picture.VIGNETTE, true)
	assert_true(world.vignette.visible)
