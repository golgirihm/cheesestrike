class_name GraphicsQuality
## How much the game asks of the graphics hardware. It starts on High, and a
## player whose machine can't keep up turns it down.
##
## There are three levels, Low, Medium and High, and each sets every part of
## the picture at once. Under them, each part can be chosen on its own:
## shadows, particles, effects, view distance, render scale, anti-aliasing
## and texture filtering. A part left alone is as the level has it.

const LOW := "low"
const MEDIUM := "medium"
const HIGH := "high"
const LEVELS := [LOW, MEDIUM, HIGH]
const NAMES := {LOW: "Low", MEDIUM: "Medium", HIGH: "High"}

## The parts that can be chosen one by one.
const SHADOWS := "shadows"
const PARTICLES := "particles"
const EFFECTS := "effects"
const VIEW_DISTANCE := "view_distance"
const RENDER_SCALE_PART := "render_scale"
const ANTIALIASING := "antialiasing"
const FILTERING := "filtering"
## Each part: the setting that holds the choice ("" for as the level has
## it), what it is called, the choices in order with what each is called,
## and what each level has.
const PARTS := {
	SHADOWS: {"setting": "gfx_shadows", "name": "Shadows", "choices": {"off": "Off", "on": "On"},
		"levels": {LOW: "off", MEDIUM: "on", HIGH: "on"}},
	PARTICLES: {"setting": "gfx_particles", "name": "Particles", "choices": {LOW: "Few", MEDIUM: "Some", HIGH: "Many"},
		"levels": {LOW: LOW, MEDIUM: MEDIUM, HIGH: HIGH}},
	EFFECTS: {"setting": "gfx_effects", "name": "Effects", "choices": {LOW: "Plain", MEDIUM: "Some", HIGH: "All"},
		"levels": {LOW: LOW, MEDIUM: MEDIUM, HIGH: HIGH}},
	VIEW_DISTANCE: {"setting": "gfx_view_distance", "name": "View distance", "choices": {"near": "Near", "middle": "Middle", "far": "Far"},
		"levels": {LOW: "middle", MEDIUM: "far", HIGH: "far"}},
	RENDER_SCALE_PART: {"setting": "gfx_render_scale", "name": "Render scale", "choices": {"50": "50%", "60": "60%", "70": "70%", "80": "80%", "90": "90%", "100": "100%"},
		"levels": {LOW: "60", MEDIUM: "80", HIGH: "100"}},
	ANTIALIASING: {"setting": "gfx_antialiasing", "name": "Smooth edges", "choices": {"off": "Off", "2x": "2×", "4x": "4×"},
		"levels": {LOW: "off", MEDIUM: "off", HIGH: "2x"}},
	FILTERING: {"setting": "gfx_filtering", "name": "Sharp far surfaces", "choices": {"off": "Off", "4x": "4×", "16x": "16×"},
		"levels": {LOW: "off", MEDIUM: "4x", HIGH: "16x"}},
}
## The order the parts are listed in.
const ORDER := [SHADOWS, PARTICLES, EFFECTS, VIEW_DISTANCE, RENDER_SCALE_PART, ANTIALIASING, FILTERING]

## Share of the screen's resolution the 3D view is drawn at.
const RENDER_SCALE := {LOW: 0.6, MEDIUM: 0.8, HIGH: 1.0}
## How far the view reaches, in metres, for each choice of view distance.
## "Middle" is past the furthest scenery; "near" cuts some of it off.
const VIEW_METRES := {"near": 150.0, "middle": 320.0, "far": 4000.0}
const _MSAA := {"off": Viewport.MSAA_DISABLED, "2x": Viewport.MSAA_2X, "4x": Viewport.MSAA_4X}
const _ANISOTROPY := {"off": Viewport.ANISOTROPY_DISABLED, "4x": Viewport.ANISOTROPY_4X, "16x": Viewport.ANISOTROPY_16X}

## The setting that lets the render scale drop to hold the frame rate.
const DYNAMIC := "dynamic_resolution"
## The frame rate dynamic resolution holds, how far below the chosen render
## scale it may go (as a share of it), and how much it moves at a time.
const HOLD_FPS := 50.0
const DYNAMIC_FLOOR := 0.6
const DYNAMIC_STEP := 0.05


## The level to start at when none has been chosen.
const DEFAULT := HIGH


## The level in effect for the setting `chosen` ("" if never chosen).
static func level(chosen: String) -> String:
	return chosen if LEVELS.has(chosen) else DEFAULT


## What `part` is at on `level` when the player has `chosen` for it: their
## choice, or what the level has if they have left it alone or it isn't one.
static func resolve(part: String, level: String, chosen: String) -> String:
	var known: Dictionary = PARTS[part]
	if known.choices.has(chosen):
		return chosen
	return known.levels.get(level, known.levels[HIGH])


## What `part` is at, as the settings have it.
static func choice(part: String) -> String:
	return resolve(part, level(Settings.get_value("quality")), Settings.get_value(PARTS[part].setting))


## What the button for `part` says, such as "Shadows: On" for a choice made
## or "Shadows: On (as Medium)" for one left to the level.
static func part_text(part: String, level: String, chosen: String) -> String:
	var known: Dictionary = PARTS[part]
	var shown: String = known.choices[resolve(part, level, chosen)]
	if known.choices.has(chosen):
		return "%s: %s" % [known.name, shown]
	return "%s: %s (as %s)" % [known.name, shown, NAMES.get(level, NAMES[HIGH])]


## The choice for `part` after `chosen`: through each in turn and back to
## leaving it to the level.
static func next_choice(part: String, chosen: String) -> String:
	var choices: Array = [""] + PARTS[part].choices.keys()
	return choices[(maxi(choices.find(chosen), 0) + 1) % choices.size()]


## Whether `level` draws real shadows. Where it doesn't, each character gets
## a soft dark blob under it instead, so that it still looks stood on the
## ground.
static func real_shadows(level: String) -> bool:
	return PARTS[SHADOWS].levels.get(level, "on") == "on"


## Whether real shadows are drawn, as the settings have it.
static func shadows_on() -> bool:
	return choice(SHADOWS) == "on"


## Puts `level` into effect on `viewport` and on the arena's `sun`, with
## each part as `chosen` ({part: choice}) has it, or as the level does.
static func apply(level: String, viewport: Viewport, sun: DirectionalLight3D, chosen := {}) -> void:
	if not LEVELS.has(level):
		level = HIGH
	viewport.scaling_3d_scale = render_scale(level, chosen.get(RENDER_SCALE_PART, ""))
	viewport.msaa_3d = _MSAA[resolve(ANTIALIASING, level, chosen.get(ANTIALIASING, ""))]
	viewport.anisotropic_filtering_level = _ANISOTROPY[resolve(FILTERING, level, chosen.get(FILTERING, ""))]
	if sun != null:
		sun.shadow_enabled = resolve(SHADOWS, level, chosen.get(SHADOWS, "")) == "on"


## What the settings have chosen for each part: {part: choice}.
static func chosen() -> Dictionary:
	var parts := {}
	for part: String in PARTS:
		parts[part] = Settings.get_value(PARTS[part].setting)
	return parts


## The share of the screen's resolution the 3D view is drawn at on `level`
## when the player has `chosen` a render scale.
static func render_scale(level: String, chosen := "") -> float:
	return float(resolve(RENDER_SCALE_PART, level, chosen)) / 100.0


## How far the view reaches, in metres, as the settings have it.
static func view_metres() -> float:
	return VIEW_METRES[choice(VIEW_DISTANCE)]


## The render scale dynamic resolution moves to from `scale`, when frames
## have lately taken `frame_seconds` each and the player chose `most`: down
## a step when the frame rate is under what it holds, up a step when there
## is room to spare, and never above what was chosen or below its floor.
static func dynamic_scale(scale: float, frame_seconds: float, most: float) -> float:
	var least := most * DYNAMIC_FLOOR
	var target := 1.0 / HOLD_FPS
	if frame_seconds > target * 1.08:
		scale -= DYNAMIC_STEP
	elif frame_seconds < target * 0.8:
		scale += DYNAMIC_STEP
	return clampf(scale, least, most)
