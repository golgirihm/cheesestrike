extends GutTest


func after_each() -> void:
	Settings.set_value(FramePace.CAP, "")
	Settings.set_value(FramePace.BATTERY_SAVER, false)
	FramePace.apply()


func test_no_cap_means_as_many_as_the_screen_allows() -> void:
	assert_eq(FramePace.most("", false), 0)
	assert_eq(FramePace.most("nonsense", false), 0)


func test_a_cap_is_kept_to() -> void:
	assert_eq(FramePace.most("60", false), 60)
	assert_eq(FramePace.most("144", false), 144)


func test_the_battery_saver_keeps_to_its_own_or_the_lower() -> void:
	assert_eq(FramePace.most("", true), FramePace.BATTERY)
	assert_eq(FramePace.most("120", true), FramePace.BATTERY)
	assert_eq(FramePace.most("30", true), 30)


func test_every_cap_can_be_chosen_and_has_a_name() -> void:
	for cap: String in FramePace.CAPS:
		assert_true(FramePace.CAP_NAMES.has(cap), cap)
		assert_true(Settings.CHOICES[FramePace.CAP].has(cap), cap)


func test_choosing_puts_it_into_effect() -> void:
	Settings.set_value(FramePace.CAP, "90")
	FramePace.apply()
	assert_eq(Engine.max_fps, 90)
	Settings.set_value(FramePace.BATTERY_SAVER, true)
	FramePace.apply()
	assert_eq(Engine.max_fps, FramePace.BATTERY)
