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
	# The match settings last used to host.
	"host_mode": MatchSettings.FREE_FOR_ALL,
	"host_kill_limit": 20,
	"host_bots": 0,
	"host_rules": "standard",
	## The arena the player laid out in the editor, as a code (see ArenaCode).
	"custom_arena": "",
	"host_sky": "noon",
	"host_bot_skill": "normal",
	"host_minutes": 8,
	"host_max_players": MatchSettings.MAX_PLAYERS,
	"host_map": Maps.DEFAULT,
	# Whether a hosted session is left out of the session list, for joining
	# only by its code or an invite link.
	"host_private": false,
	## Whether to join a session to watch it rather than to play.
	"join_watching": false,
	# How fast looking turns, as a share of the usual, for each kind of control.
	"mouse_sensitivity": 1.0,
	"stick_sensitivity": 1.0,
	"touch_sensitivity": 1.0,
	"invert_look": false,
	"fov": 75.0,
	# Loudness from 0 (silent) to 1.
	"master_volume": 0.8,
	"music_volume": 0.6,
	"effects_volume": 0.8,
	# "low", "medium" or "high"; "" until first chosen, when it depends on the
	# device (see GraphicsQuality).
	"quality": "",
	# Keys chosen for actions, by action: physical key codes.
	"bindings": {},
	# Touch screens: the size of the on-screen buttons, whether the buttons
	# and the movement stick swap sides, and whether the first touch makes the
	# game fill the screen, turned sideways.
	"touch_size": 1.0,
	"touch_left_handed": false,
	"touch_fullscreen": true,
	# Whether the controls have been shown, as they are on entering a first session.
	"seen_help": false,
	# Seeing and hearing: how much the view shakes (0 for not at all), the size
	# of the fight's text, and whether important sounds are written on screen.
	"screen_shake": 1.0,
	## How much the view bobs, rolls and dips as the player moves: 0 for none.
	"camera_motion": 1.0,
	## Whether run, crouch and aim are pressed once to turn on and again to
	## turn off, rather than held.
	"toggle_run": false,
	"toggle_crouch": false,
	"toggle_aim": false,
	"hud_scale": 1.0,
	"captions": false,
	## Whether the damage of each hit is written by the crosshair.
	"damage_numbers": false,
	## Whether to be shown how one was knocked out, and the play of the match.
	"replays": true,
	## What is done to the picture (see Picture): whether bright things glow,
	## whether each arena has its own cast of colour, and whether the corners
	## of the view are darkened.
	## Each part of the graphics chosen on its own (see GraphicsQuality): ""
	## for as the quality level has it.
	"gfx_shadows": "",
	"gfx_particles": "",
	"gfx_effects": "",
	"gfx_view_distance": "",
	"gfx_render_scale": "",
	"gfx_antialiasing": "",
	"gfx_filtering": "",
	## Whether the render scale drops to hold the frame rate.
	"dynamic_resolution": false,
	## Whether teammates and pickups close by are outlined (see Outlines).
	"outlines": true,
	"bloom": true,
	"colour_grade": true,
	"vignette": true,
	## What the player's character looks like to others (see Looks).
	"look_cheese": Looks.CHEESES[0],
	"look_hat": Looks.HATS[0],
	"look_held": Looks.HELD[0],
	## The weapons the player comes into play with (see Weapons.MAINS).
	"loadout_main": Weapons.DEFAULT_MAIN,
	"loadout_side": Weapons.STARTING,
	## What is fitted to the main weapon (see Attachments.IDS).
	"loadout_attachment": Attachments.IDS[0],
	## The kind of Overload the player sets off (see Overload.IDS).
	"loadout_overload": Overload.IDS[0],
}

## The allowed range of each number setting. A value outside is brought in.
const RANGES := {
	"mouse_sensitivity": [0.1, 4.0],
	"stick_sensitivity": [0.1, 4.0],
	"touch_sensitivity": [0.1, 4.0],
	"fov": [60.0, 110.0],
	"master_volume": [0.0, 1.0],
	"music_volume": [0.0, 1.0],
	"effects_volume": [0.0, 1.0],
	"touch_size": [0.7, 1.5],
	"screen_shake": [0.0, 1.0],
	"camera_motion": [0.0, 1.0],
	"hud_scale": [0.75, 1.5],
}
## The allowed choices of a setting that is one of a few.
const CHOICES := {
	"quality": ["", "low", "medium", "high"],
	"gfx_shadows": ["", "off", "on"],
	"gfx_particles": ["", "low", "medium", "high"],
	"gfx_effects": ["", "low", "medium", "high"],
	"gfx_view_distance": ["", "near", "middle", "far"],
	"gfx_render_scale": ["", "50", "60", "70", "80", "90", "100"],
	"gfx_antialiasing": ["", "off", "2x", "4x"],
	"gfx_filtering": ["", "off", "4x", "16x"],
	"look_cheese": Looks.CHEESES,
	"look_hat": Looks.HATS,
	"look_held": Looks.HELD,
	"loadout_main": Weapons.MAINS,
	"loadout_side": Weapons.SIDES,
	"loadout_attachment": Attachments.IDS,
	"loadout_overload": Overload.IDS,
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
			if not file.has_section_key(SECTION, key):
				continue
			var stored: Variant = file.get_value(SECTION, key)
			if _same_type(stored, DEFAULTS[key]) and _allowed(key, stored):
				_values[key] = _within(key, type_convert(stored, typeof(DEFAULTS[key])))
	if PlayerName.clean(_values.name).is_empty():
		set_value("name", PlayerName.random_default())


func get_value(key: String) -> Variant:
	var value: Variant = _values[key]
	# A copy, so that changing it means setting it.
	return value.duplicate() if value is Dictionary else value


## Stores `value` for `key` and saves. Unknown keys and values of the wrong
## type are refused, with an error, so a typo shows up at once.
func set_value(key: String, value: Variant) -> void:
	if not DEFAULTS.has(key):
		push_error("no setting called %s" % key)
		return
	if not _same_type(value, DEFAULTS[key]):
		push_error("setting %s takes a %s, not %s" % [key, type_string(typeof(DEFAULTS[key])), value])
		return
	if not _allowed(key, value):
		push_error("setting %s can't be %s" % [key, value])
		return
	value = _within(key, type_convert(value, typeof(DEFAULTS[key])))
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


static func _allowed(key: String, value: Variant) -> bool:
	return not CHOICES.has(key) or CHOICES[key].has(value)


static func _within(key: String, value: Variant) -> Variant:
	if RANGES.has(key) and (value is float or value is int):
		return clampf(value, RANGES[key][0], RANGES[key][1])
	return value


# Whole numbers are accepted for decimal settings, as a file written by hand
# may hold "1" where "1.0" was meant.
static func _same_type(value: Variant, default: Variant) -> bool:
	if typeof(default) == TYPE_FLOAT and typeof(value) == TYPE_INT:
		return true
	return typeof(value) == typeof(default)
