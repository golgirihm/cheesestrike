extends GutTest

const SETTINGS := preload("res://settings/settings.gd")
const PATH := "user://test_settings.cfg"


func before_each() -> void:
	DirAccess.remove_absolute(PATH)


func after_all() -> void:
	DirAccess.remove_absolute(PATH)


func _fresh() -> Node:
	return autofree(SETTINGS.new(PATH))


func _write(values: Dictionary) -> void:
	var file := ConfigFile.new()
	for key: String in values:
		file.set_value(SETTINGS.SECTION, key, values[key])
	file.save(PATH)


func test_a_first_visit_gets_a_random_cheese_name() -> void:
	var settings := _fresh()
	var name: String = settings.get_value("name")
	assert_false(name.is_empty())
	assert_has(PlayerName.CHEESES, name.rsplit(" ", true, 1)[0])


func test_the_name_given_on_a_first_visit_is_kept() -> void:
	var first: String = _fresh().get_value("name")
	assert_eq(_fresh().get_value("name"), first)


func test_a_setting_survives_a_restart() -> void:
	_fresh().set_value("name", "Wensleydale")
	assert_eq(_fresh().get_value("name"), "Wensleydale")


func test_a_value_of_the_wrong_type_in_the_file_is_ignored() -> void:
	_write({"name": 42})
	var name: String = _fresh().get_value("name")
	assert_ne(name, "42")
	assert_false(name.is_empty())


func test_a_blank_stored_name_is_replaced() -> void:
	_write({"name": "   "})
	assert_false(PlayerName.clean(_fresh().get_value("name")).is_empty())


func test_a_damaged_file_gives_defaults() -> void:
	var file := FileAccess.open(PATH, FileAccess.WRITE)
	file.store_string("[settings\nname = = =")
	file.close()
	assert_false(_fresh().get_value("name").is_empty())
	assert_engine_error("ConfigFile parse error")


func test_unknown_keys_in_the_file_are_ignored() -> void:
	_write({"name": "Comté", "flying": true})
	assert_eq(_fresh().get_value("name"), "Comté")


func test_setting_an_unknown_key_is_refused() -> void:
	var settings := _fresh()
	settings.set_value("flying", true)
	assert_push_error("no setting called flying")


func test_setting_a_value_of_the_wrong_type_is_refused() -> void:
	var settings := _fresh()
	var before: String = settings.get_value("name")
	settings.set_value("name", 3)
	assert_push_error("takes a")
	assert_eq(settings.get_value("name"), before)


func test_a_change_is_announced_once() -> void:
	var settings := _fresh()
	watch_signals(settings)
	settings.set_value("name", "Stilton")
	settings.set_value("name", "Stilton")
	assert_signal_emit_count(settings, "changed", 1)
	assert_signal_emitted_with_parameters(settings, "changed", ["name"])
