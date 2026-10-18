class_name Picture
## What is done to the picture after the arena is drawn: corners and the
## foot of things are shaded where light would struggle to reach, bright
## things glow, each arena has its own cast of colour, and the corners of the
## view are a little darker than its middle. The glow, the cast and the
## corners can be turned off in the settings, and the glow and the shading,
## which cost the most, are left off on low graphics. The shading is only
## drawn by the desktop renderer: the web build's has no way to.

## The settings that turn each on.
const BLOOM := "bloom"
const GRADE := "colour_grade"
const VIGNETTE := "vignette"

## Each arena's cast: how bright, how contrasty and how colourful the
## picture is made (1 leaves it alone), and the colour white becomes.
const GRADES := {
	"canyon": {"brightness": 1.0, "contrast": 1.06, "saturation": 1.12, "tint": Color(1.0, 0.97, 0.9)},
	"box": {"brightness": 1.02, "contrast": 1.04, "saturation": 1.08, "tint": Color(1.0, 0.99, 0.95)},
	"city": {"brightness": 1.0, "contrast": 1.1, "saturation": 0.9, "tint": Color(0.94, 0.97, 1.0)},
	"dairy": {"brightness": 1.03, "contrast": 1.03, "saturation": 1.0, "tint": Color(0.97, 0.99, 1.0)},
	"kitchen": {"brightness": 1.0, "contrast": 1.05, "saturation": 1.1, "tint": Color(1.0, 0.96, 0.88)},
	"market": {"brightness": 1.0, "contrast": 1.05, "saturation": 1.18, "tint": Color(1.0, 0.98, 0.93)},
	"cave": {"brightness": 0.98, "contrast": 1.12, "saturation": 0.92, "tint": Color(0.9, 0.96, 1.0)},
	"duel": {"brightness": 1.0, "contrast": 1.1, "saturation": 1.0, "tint": Color(1.0, 0.97, 0.94)},
}
const PLAIN := {"brightness": 1.0, "contrast": 1.0, "saturation": 1.0, "tint": Color.WHITE}

## How strongly bright things glow, and how bright a thing must be to.
const GLOW_INTENSITY := 0.5
const GLOW_THRESHOLD := 0.92
## How far the shading in corners reaches, in metres (a player is 5.4 m
## tall, so further than it would in a room), and how dark it goes.
const SHADE_RADIUS := 2.5
const SHADE_INTENSITY := 2.5
## How dark the corners of the view get.
const VIGNETTE_STRENGTH := 0.3

const _VIGNETTE_SHADER := """
shader_type canvas_item;
uniform float strength = 0.3;
void fragment() {
	// Clear in the middle, darkening from two thirds of the way to a corner.
	float out_from_middle = distance(UV, vec2(0.5)) / 0.7071;
	COLOR = vec4(0.0, 0.0, 0.0, strength * smoothstep(0.55, 1.05, out_from_middle));
}
"""


## The cast of arena `map`: plain for one that has none of its own.
static func grade(map: String) -> Dictionary:
	return GRADES.get(map, PLAIN)


## Whether bright things glow on graphics quality `level` when the player
## `wants` them to: never on low.
static func glows(level: String, wants: bool) -> bool:
	return wants and level != GraphicsQuality.LOW


## Whether corners are shaded on graphics quality `level`: not on low.
static func shades(level: String) -> bool:
	return level != GraphicsQuality.LOW


## Puts the picture for arena `map` on graphics quality `level` into
## `environment`, as the settings in `wanted` ({BLOOM, GRADE, VIGNETTE:
## bool}) have it.
static func apply(environment: Environment, map: String, level: String, wanted: Dictionary) -> void:
	environment.ssao_enabled = shades(level)
	environment.ssao_radius = SHADE_RADIUS
	environment.ssao_intensity = SHADE_INTENSITY
	environment.glow_enabled = glows(level, wanted.get(BLOOM, true))
	environment.glow_intensity = GLOW_INTENSITY
	environment.glow_hdr_threshold = GLOW_THRESHOLD
	environment.glow_blend_mode = Environment.GLOW_BLEND_MODE_SCREEN
	var cast: Dictionary = grade(map) if wanted.get(GRADE, true) else PLAIN
	environment.adjustment_enabled = cast != PLAIN
	environment.adjustment_brightness = cast.brightness
	environment.adjustment_contrast = cast.contrast
	environment.adjustment_saturation = cast.saturation
	environment.adjustment_color_correction = _tint(cast.tint)


## What the settings ask for.
static func wanted() -> Dictionary:
	return {BLOOM: Settings.get_value(BLOOM), GRADE: Settings.get_value(GRADE), VIGNETTE: Settings.get_value(VIGNETTE)}


## A layer over the arena and under the interface that darkens the corners
## of the view.
static func vignette() -> CanvasLayer:
	var layer := CanvasLayer.new()
	layer.name = "Vignette"
	layer.layer = -1
	var shade := ColorRect.new()
	shade.name = "Shade"
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var shader := Shader.new()
	shader.code = _VIGNETTE_SHADER
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter("strength", VIGNETTE_STRENGTH)
	shade.material = material
	layer.add_child(shade)
	return layer


static var _tints := {}


# What turns white into `tint` and leaves black alone, or nothing for white.
static func _tint(tint: Color) -> Texture:
	if tint == Color.WHITE:
		return null
	if not _tints.has(tint):
		var ramp := Gradient.new()
		ramp.set_color(0, Color.BLACK)
		ramp.set_color(1, tint)
		var texture := GradientTexture1D.new()
		texture.gradient = ramp
		texture.width = 64
		_tints[tint] = texture
	return _tints[tint]
