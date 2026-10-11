class_name GraphicsQuality
## How much the game asks of the graphics hardware. A phone may not keep up
## with shadows and full resolution, so it starts lower.

const LOW := "low"
const MEDIUM := "medium"
const HIGH := "high"
const LEVELS := [LOW, MEDIUM, HIGH]
const NAMES := {LOW: "Low", MEDIUM: "Medium", HIGH: "High"}

## Share of the screen's resolution the 3D view is drawn at.
const RENDER_SCALE := {LOW: 0.6, MEDIUM: 0.8, HIGH: 1.0}


## The level to start at when none has been chosen: low on touch screens.
static func default_for(touch_screen: bool) -> String:
	return LOW if touch_screen else HIGH


## The level in effect for the setting `chosen` ("" if never chosen).
static func level(chosen: String, touch_screen := DisplayServer.is_touchscreen_available()) -> String:
	return chosen if LEVELS.has(chosen) else default_for(touch_screen)


## Puts `level` into effect on `viewport` and on the arena's `sun`.
static func apply(level: String, viewport: Viewport, sun: DirectionalLight3D) -> void:
	if not LEVELS.has(level):
		level = HIGH
	viewport.scaling_3d_scale = RENDER_SCALE[level]
	viewport.msaa_3d = Viewport.MSAA_2X if level == HIGH else Viewport.MSAA_DISABLED
	if sun != null:
		sun.shadow_enabled = level != LOW
