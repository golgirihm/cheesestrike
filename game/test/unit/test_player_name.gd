extends GutTest


func test_an_ordinary_name_is_kept() -> void:
	assert_eq(PlayerName.clean("Big Wheel"), "Big Wheel")


func test_accented_and_other_scripts_are_kept() -> void:
	assert_eq(PlayerName.clean("Gruyère"), "Gruyère")
	assert_eq(PlayerName.clean("チーズ"), "チーズ")
	assert_eq(PlayerName.clean("Сыр"), "Сыр")


func test_surrounding_space_is_trimmed() -> void:
	assert_eq(PlayerName.clean("   Edam  "), "Edam")


func test_runs_of_white_space_become_one_space() -> void:
	assert_eq(PlayerName.clean("Blue \t\n  Cheese"), "Blue Cheese")
	assert_eq(PlayerName.clean("Wide" + char(0x3000) + "Space"), "Wide Space")
	assert_eq(PlayerName.clean("No" + char(0x00A0) + "Break"), "No Break")


func test_control_characters_are_removed() -> void:
	assert_eq(PlayerName.clean("Fe" + char(1) + "ta" + char(7)), "Feta")
	assert_eq(PlayerName.clean("Br" + char(0x7F) + "ie" + char(0x85)), "Brie")


func test_invisible_characters_are_removed() -> void:
	assert_eq(PlayerName.clean("Ga" + char(0x200B) + "me"), "Game", "zero-width space")
	assert_eq(PlayerName.clean(char(0x202E) + "Racer"), "Racer", "right-to-left override")
	assert_eq(PlayerName.clean(char(0xFEFF) + "Start"), "Start", "byte order mark")
	assert_eq(PlayerName.clean("Soft" + char(0x00AD) + "hyphen"), "Softhyphen")


func test_a_name_of_nothing_visible_is_empty() -> void:
	assert_eq(PlayerName.clean(""), "")
	assert_eq(PlayerName.clean("   "), "")
	assert_eq(PlayerName.clean(char(0x200B) + char(0x200C) + char(0x200D)), "")
	assert_eq(PlayerName.clean("\n\t"), "")


func test_long_names_are_cut_to_the_limit() -> void:
	var cleaned := PlayerName.clean("Abcdefghijklmnopqrstuvwxyz")
	assert_eq(cleaned.length(), PlayerName.MAX_LENGTH)
	assert_eq(cleaned, "Abcdefghijklmnop")


func test_a_cut_name_does_not_end_in_a_space() -> void:
	assert_eq(PlayerName.clean("Fifteen letters A b"), "Fifteen letters")


func test_invisible_characters_do_not_count_toward_the_limit() -> void:
	var padded := char(0x200B).repeat(40) + "Mozzarella"
	assert_eq(PlayerName.clean(padded), "Mozzarella")


func test_random_defaults_are_a_cheese_and_a_number() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	for i in 50:
		var name := PlayerName.random_default(rng)
		var parts := name.rsplit(" ", true, 1)
		assert_has(PlayerName.CHEESES, parts[0])
		assert_between(parts[1].to_int(), 10, 99)
		assert_eq(PlayerName.clean(name), name, "a default name should survive cleaning")


func test_every_cheese_fits_with_its_number() -> void:
	for cheese: String in PlayerName.CHEESES:
		assert_lte((cheese + " 99").length(), PlayerName.MAX_LENGTH, cheese)
