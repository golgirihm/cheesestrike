extends GutTest
## The player's own crosshair: its settings, what is drawn for them, and the
## code that shares it.

const SETTINGS_MENU := preload("res://ui/settings_menu.gd")

var saved := {}


func before_each() -> void:
	for key: String in Crosshair.KEYS:
		saved[key] = Settings.get_value(key)
		Settings.set_value(key, Settings.DEFAULTS[key])


func after_each() -> void:
	for key: String in saved:
		Settings.set_value(key, saved[key])


func _lines(shapes: Array) -> Array:
	return shapes.filter(func(shape: Dictionary) -> bool: return shape.kind == "line")


func test_every_part_of_it_is_a_setting_kept_within_bounds() -> void:
	for key: String in Crosshair.KEYS:
		assert_has(Settings.DEFAULTS, key)
	assert_eq(Settings.CHOICES[Crosshair.SHAPE], Crosshair.SHAPES.keys())
	assert_eq(Settings.CHOICES[Crosshair.COLOUR], Crosshair.COLOURS.keys())
	for key: String in Crosshair.RANGES:
		assert_eq(Settings.RANGES[key], Crosshair.RANGES[key], key)
		Settings.set_value(key, 999.0)
		assert_eq(Settings.get_value(key), Crosshair.RANGES[key][1], key)
		Settings.set_value(key, -5.0)
		assert_eq(Settings.get_value(key), Crosshair.RANGES[key][0], key)
	for colour: String in Crosshair.COLOURS:
		assert_has(Crosshair.COLOUR_NAMES, colour)
	assert_eq(Crosshair.chosen()[Crosshair.SHAPE], Settings.get_value(Crosshair.SHAPE))


func test_the_standard_one_is_what_the_game_always_had() -> void:
	var drawn := Crosshair.shapes(Crosshair.standard(), 5.0)
	assert_eq(_lines(drawn).size(), 4, "four lines")
	assert_eq(drawn.filter(func(shape: Dictionary) -> bool: return shape.kind == "disc").size(), 1, "and a dot")
	var up: Dictionary = _lines(drawn)[0]
	assert_eq(up.from, Vector2(0, -5.0), "beginning at the spread")
	assert_eq(up.to, Vector2(0, -14.0), "and nine long")
	assert_eq(up.width, 2.0)


func test_each_shape_is_drawn_its_own_way() -> void:
	var style := Crosshair.standard()
	style[Crosshair.CENTRE_DOT] = false
	style[Crosshair.SHAPE] = Crosshair.TEE
	var tee := Crosshair.shapes(style, 5.0)
	assert_eq(tee.size(), 3, "no line above")
	assert_false(tee.any(func(shape: Dictionary) -> bool: return shape.to.y < 0.0))
	style[Crosshair.SHAPE] = Crosshair.CIRCLE
	var circle := Crosshair.shapes(style, 5.0)
	assert_eq(circle.size(), 1)
	assert_eq(circle[0].kind, "ring")
	assert_gt(circle[0].radius, 5.0)
	style[Crosshair.SHAPE] = Crosshair.DOT
	var dot := Crosshair.shapes(style, 5.0)
	assert_eq(dot.size(), 1, "a dot is a dot, whether or not the middle one is wanted")
	assert_eq(dot[0].kind, "disc")
	style[Crosshair.SHAPE] = Crosshair.CROSS
	assert_eq(Crosshair.shapes(style, 5.0).size(), 4, "with no dot in the middle")


func test_its_lines_open_with_the_weapons_spread_and_the_gap_chosen() -> void:
	var style := Crosshair.standard()
	var tight: Dictionary = _lines(Crosshair.shapes(style, 5.0))[0]
	var wide: Dictionary = _lines(Crosshair.shapes(style, 40.0))[0]
	assert_eq(wide.from.y, -40.0)
	assert_eq(wide.to.y - wide.from.y, tight.to.y - tight.from.y, "as long either way")
	style[Crosshair.GAP] = 6.0
	assert_eq(_lines(Crosshair.shapes(style, 5.0))[0].from.y, -11.0, "the gap chosen is added to the spread")
	style[Crosshair.SIZE] = 20.0
	style[Crosshair.THICKNESS] = 4.0
	var big: Dictionary = _lines(Crosshair.shapes(style, 5.0))[0]
	assert_eq(big.to.y - big.from.y, -20.0)
	assert_eq(big.width, 4.0)


func test_a_code_gives_the_same_crosshair_back() -> void:
	assert_eq(Crosshair.encode(Crosshair.standard()), "CS1-cross-white-9-0-2-0-1")
	for shape: String in Crosshair.SHAPES:
		for colour: String in Crosshair.COLOURS:
			var style := {Crosshair.SHAPE: shape, Crosshair.COLOUR: colour, Crosshair.SIZE: 14.0, Crosshair.GAP: 3.0, Crosshair.THICKNESS: 5.0,
				Crosshair.OUTLINE: true, Crosshair.CENTRE_DOT: false}
			assert_eq(Crosshair.decode(Crosshair.encode(style)), style, "%s %s" % [shape, colour])
	# As someone might type it.
	assert_eq(Crosshair.decode("  cs1-CROSS-Green-9-2-2-1-1 \n")[Crosshair.COLOUR], "green")


func test_what_is_not_a_code_gives_nothing_and_a_wild_one_is_brought_in() -> void:
	for junk: Variant in ["", "hello", 7, null, "CS1-cross-white-9-0-2-0", "CS2-cross-white-9-0-2-0-1", "CS1-star-white-9-0-2-0-1", "CS1-cross-pink-9-0-2-0-1",
			"CS1-cross-white-x-0-2-0-1", "CS1-cross-white-9-0-2-2-1", "CS1-cross-white-9-0-2-0-yes", "CS1-cross-white-9-0-2-0-1-1", "x".repeat(200)]:
		assert_eq(Crosshair.decode(junk), {}, str(junk).left(30))
	var wild := Crosshair.decode("CS1-cross-white-999-999-999-0-1")
	assert_eq(wild[Crosshair.SIZE], Crosshair.RANGES[Crosshair.SIZE][1])
	assert_eq(wild[Crosshair.GAP], Crosshair.RANGES[Crosshair.GAP][1])
	assert_eq(wild[Crosshair.THICKNESS], Crosshair.RANGES[Crosshair.THICKNESS][1])
	assert_eq(Crosshair.decode("CS1-cross-white--5-0-2-0-1"), {}, "nor a number below nothing")
	# Anything at all is tidied into a crosshair.
	assert_eq(Crosshair.tidy({}), Crosshair.standard())
	assert_eq(Crosshair.tidy({Crosshair.SHAPE: 3, Crosshair.SIZE: "big", Crosshair.OUTLINE: "yes"}), Crosshair.standard())


func test_a_code_put_in_becomes_the_players_crosshair() -> void:
	var menu: Control = SETTINGS_MENU.new()
	add_child_autofree(menu)
	menu.open()
	var box: LineEdit = menu.find_child("CrosshairCode", true, false)
	assert_eq(box.text, "CS1-cross-white-9-0-2-0-1", "it shows the code of the one chosen")
	assert_true(menu.use_crosshair_code("CS1-circle-cyan-12-4-3-1-0"))
	assert_eq(Settings.get_value(Crosshair.SHAPE), "circle")
	assert_eq(Settings.get_value(Crosshair.COLOUR), "cyan")
	assert_eq(Settings.get_value(Crosshair.SIZE), 12.0)
	assert_eq(Settings.get_value(Crosshair.OUTLINE), true)
	assert_eq(Settings.get_value(Crosshair.CENTRE_DOT), false)
	assert_eq(box.text, "CS1-circle-cyan-12-4-3-1-0")
	# One that isn't a code changes nothing.
	assert_false(menu.use_crosshair_code("nonsense"))
	assert_eq(Settings.get_value(Crosshair.SHAPE), "circle")
	# Changing a setting changes the code shown.
	menu.choose("Setting_" + Crosshair.SHAPE, "dot")
	assert_eq(Settings.get_value(Crosshair.SHAPE), "dot")
	assert_string_starts_with(box.text, "CS1-dot-cyan")
	for key: String in Crosshair.KEYS:
		assert_not_null(menu.find_child("Setting_" + key, true, false), key)
	assert_not_null(menu.find_child("CrosshairView", true, false))
	assert_not_null(menu.find_child("CrosshairCopy", true, false))
