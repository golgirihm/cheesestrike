extends Node
## The player's own preferences, kept between visits in a settings file under
## user://, which a browser keeps in the page's IndexedDB storage.
##
## Each setting has a default here, and a value read from the file is only used
## if it has the default's type, so a damaged or outdated file can't break the
## game: the setting just goes back to its default.

signal changed(key: String)

const DEFAULT_PATH := "user://settings.cfg"
const SECTION := "settings"
const DEFAULTS := {
	"name": "",
}

## Where the settings are kept. Tests point their own instances elsewhere.
var path := DEFAULT_PATH

var _values := {}


func _init(at := DEFAULT_PATH) -> void:
	path = at
	load_settings()


func load_settings() -> void:
	_values = DEFAULTS.duplicate(true)
	var file := ConfigFile.new()
	if file.load(path) == OK:
		for key: String in DEFAULTS:
			var stored: Variant = file.get_value(SECTION, key, null)
			if _same_type(stored, DEFAULTS[key]):
				_values[key] = type_convert(stored, typeof(DEFAULTS[key]))
	if PlayerName.clean(_values.name).is_empty():
		set_value("name", PlayerName.random_default())


func get_value(key: String) -> Variant:
	return _values[key]


## Stores `value` for `key` and saves. Unknown keys and values of the wrong
## type are refused, with an error, so a typo shows up at once.
func set_value(key: String, value: Variant) -> void:
	if not DEFAULTS.has(key):
		push_error("no setting called %s" % key)
		return
	if not _same_type(value, DEFAULTS[key]):
		push_error("setting %s takes a %s, not %s" % [key, type_string(typeof(DEFAULTS[key])), value])
		return
	value = type_convert(value, typeof(DEFAULTS[key]))
	if _values[key] == value:
		return
	_values[key] = value
	_save()
	changed.emit(key)


func _save() -> void:
	var file := ConfigFile.new()
	for key: String in _values:
		file.set_value(SECTION, key, _values[key])
	file.save(path)


# Whole numbers are accepted for decimal settings, as a file written by hand
# may hold "1" where "1.0" was meant.
static func _same_type(value: Variant, default: Variant) -> bool:
	if typeof(default) == TYPE_FLOAT and typeof(value) == TYPE_INT:
		return true
	return typeof(value) == typeof(default)
