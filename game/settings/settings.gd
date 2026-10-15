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
	# How a gamepad's stick looks (see StickAim).
	"stick_dead_zone": 0.15,
	"stick_curve": "linear",
	"aim_assist": true,
	"rumble": true,
	"haptics": true,
	# Aiming by turning the device (see GyroAim).
	"gyro_aim": false,
	# One switch that calms everything that moves the view or flashes.
	"less_motion": false,
	# The colours of the teams and of a hit (see TeamColors).
	"colour_preset": "standard",
	# Whether the title screen's word about flashing has been put away.
	"flash_warning_seen": false,
	# Whether a ring round the crosshair shows where sounds come from.
	"sound_ring": false,
	# Holding or pressing once, for the rest of what can be held (see the
	# toggles for running, crouching and aiming).
	"toggle_lean": false,
	"toggle_talk": false,
	"toggle_scoreboard": false,
	"hold_to_fire": false,
	# How many frames a second are drawn at most (see FramePace).
	"frame_cap": "",
	"battery_saver": false,
	"colour_cheddar": "",
	"colour_brie": "",
	"colour_hit": "",
	"gyro_sensitivity": 1.0,
	"touch_sensitivity": 1.0,
	# The share of it while aiming, through a scope, and up and down against
	# across (see Sensitivity), and the game whose feel was last matched.
	"aim_sensitivity": 1.0,
	"scope_sensitivity": 1.0,
	"look_vertical": 1.0,
	"match_game": "source",
	"invert_look": false,
	"fov": 75.0,
	# Loudness from 0 (silent) to 1.
	"master_volume": 0.8,
	"music_volume": 0.6,
	"effects_volume": 0.8,
	## Each kind of effect on its own, under "effects_volume" (see Mix).
	"weapons_volume": 1.0,
	"steps_volume": 1.0,
	"voices_volume": 1.0,
	"world_volume": 1.0,
	"interface_volume": 1.0,
	## How wide the difference between loud and quiet is left (see Mix).
	"dynamic_range": "headphones",
	## Whether the player talks with teammates and hears them (see VoiceChat).
	"voice_chat": true,
	## How many tips the player has been shown, so that each comes in turn
	## (see JoinScreen).
	"tips_seen": 0,
	## How the HUD is laid out, where it differs from the usual (see
	## HudLayout).
	"hud_layout": {},
	## Numbers for the curious (see Readings).
	"show_fps": false,
	"show_ping": false,
	"show_loss": false,
	"show_graph": false,
	## The player's crosshair (see Crosshair).
	"crosshair_shape": "cross",
	"crosshair_colour": "white",
	"crosshair_size": 9.0,
	"crosshair_gap": 0.0,
	"crosshair_thickness": 2.0,
	"crosshair_outline": false,
	"crosshair_dot": true,
	## Whether text chat is shown and can be typed (see ChatBox).
	"text_chat": true,
	## Whether the announcer speaks (see Announcer).
	"announcer": true,
	## Whether left and right are made the same.
	"mono_sound": false,
	## What a hit sounds like (see SoundCues).
	"hit_sound": "tick",
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
	"look_title": Looks.TITLES[0],
	"look_finish": Unlocks.FINISHES[0],
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
	"stick_dead_zone": [0.0, 0.5],
	"gyro_sensitivity": [0.2, 3.0],
	"touch_sensitivity": [0.1, 4.0],
	"aim_sensitivity": [0.2, 2.0],
	"scope_sensitivity": [0.2, 2.0],
	"look_vertical": [0.2, 2.0],
	"fov": [60.0, 110.0],
	"master_volume": [0.0, 1.0],
	"music_volume": [0.0, 1.0],
	"effects_volume": [0.0, 1.0],
	"crosshair_size": [2.0, 24.0],
	"crosshair_gap": [0.0, 16.0],
	"crosshair_thickness": [1.0, 6.0],
	"weapons_volume": [0.0, 1.0],
	"steps_volume": [0.0, 1.0],
	"voices_volume": [0.0, 1.0],
	"world_volume": [0.0, 1.0],
	"interface_volume": [0.0, 1.0],
	"touch_size": [0.7, 1.5],
	"screen_shake": [0.0, 1.0],
	"camera_motion": [0.0, 1.0],
	"hud_scale": [0.75, 1.5],
}
## The allowed choices of a setting that is one of a few.
const CHOICES := {
	"quality": ["", "low", "medium", "high"],
	"dynamic_range": ["headphones", "speakers", "night"],
	"look_title": Looks.TITLES,
	"look_finish": Unlocks.FINISHES,
	"crosshair_shape": ["cross", "tee", "circle", "dot"],
	"crosshair_colour": ["white", "green", "yellow", "cyan", "magenta", "red"],
	"hit_sound": ["tick", "pop", "bell", "off"],
	"stick_curve": ["linear", "classic", "precise"],
	"frame_cap": ["", "30", "60", "90", "120", "144"],
	"colour_preset": ["standard", "protanopia", "deuteranopia", "tritanopia"],
	"colour_cheddar": ["", "red", "orange", "yellow", "green", "teal", "blue", "sky", "purple", "pink", "white"],
	"colour_brie": ["", "red", "orange", "yellow", "green", "teal", "blue", "sky", "purple", "pink", "white"],
	"colour_hit": ["", "red", "orange", "yellow", "green", "teal", "blue", "sky", "purple", "pink", "white"],
	"match_game": ["source", "valorant", "overwatch", "apex", "quake"],
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



## How much of the motion that `key` ("screen_shake" or "camera_motion")
## asks for there is: none at all with less motion asked for.
func motion(key: String) -> float:
	return 0.0 if get_value(LESS_MOTION) else float(get_value(key))


## How bright a flash is, as a share of its full brightness: dimmed with
## less motion asked for.
func flash() -> float:
	return CALM_FLASH if get_value(LESS_MOTION) else 1.0


const LESS_MOTION := "less_motion"
const CALM_FLASH := 0.25

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
