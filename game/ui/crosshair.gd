class_name Crosshair
## The player's own crosshair: its shape, how long its lines are, how far
## apart, how thick, what colour, whether it has a dark edge and a dot in
## the middle. Each is a setting, and the whole can be written as a short
## code to give to someone else.
##
## The gap between its lines still opens and closes with the spread of the
## weapon in hand: the gap chosen here is added to that.

const CROSS := "cross"
const TEE := "tee"
const CIRCLE := "circle"
const DOT := "dot"
const SHAPES := {CROSS: "Cross", TEE: "T", CIRCLE: "Circle", DOT: "Dot"}
const COLOURS := {"white": Color(1, 1, 1), "green": Color(0.3, 1.0, 0.35), "yellow": Color(1.0, 0.92, 0.25), "cyan": Color(0.3, 0.95, 1.0),
	"magenta": Color(1.0, 0.35, 0.95), "red": Color(1.0, 0.3, 0.25)}
const COLOUR_NAMES := {"white": "White", "green": "Green", "yellow": "Yellow", "cyan": "Cyan", "magenta": "Magenta", "red": "Red"}
## The settings that make up a crosshair, with the least and most each
## number can be, in pixels.
const SHAPE := "crosshair_shape"
const COLOUR := "crosshair_colour"
const SIZE := "crosshair_size"
const GAP := "crosshair_gap"
const THICKNESS := "crosshair_thickness"
const OUTLINE := "crosshair_outline"
const CENTRE_DOT := "crosshair_dot"
const KEYS := [SHAPE, COLOUR, SIZE, GAP, THICKNESS, OUTLINE, CENTRE_DOT]
const RANGES := {SIZE: [2.0, 24.0], GAP: [0.0, 16.0], THICKNESS: [1.0, 6.0]}
## What a code begins with, which says how the rest is to be read.
const PREFIX := "CS1"
## How see-through it is drawn, and how wide its dark edge is.
const ALPHA := 0.9
const EDGE := 1.5


## The crosshair as the settings have it: {setting: value}.
static func chosen() -> Dictionary:
	var style := {}
	for key: String in KEYS:
		style[key] = Settings.get_value(key)
	return style


## The crosshair the game starts with.
static func standard() -> Dictionary:
	var style := {}
	for key: String in KEYS:
		style[key] = Settings.DEFAULTS[key]
	return style


## `style` with everything in it made something a crosshair can be: an
## unknown shape or colour the standard one, and each number within bounds.
static func tidy(style: Dictionary) -> Dictionary:
	var usual := standard()
	var tidied := {}
	tidied[SHAPE] = style.get(SHAPE) if SHAPES.has(style.get(SHAPE)) else usual[SHAPE]
	tidied[COLOUR] = style.get(COLOUR) if COLOURS.has(style.get(COLOUR)) else usual[COLOUR]
	for key: String in RANGES:
		var value: Variant = style.get(key)
		tidied[key] = clampf(float(value), RANGES[key][0], RANGES[key][1]) if value is float or value is int else usual[key]
	tidied[OUTLINE] = style.get(OUTLINE) if style.get(OUTLINE) is bool else usual[OUTLINE]
	tidied[CENTRE_DOT] = style.get(CENTRE_DOT) if style.get(CENTRE_DOT) is bool else usual[CENTRE_DOT]
	return tidied


## `style` as a code to share, such as "CS1-cross-green-9-2-2-1-1".
static func encode(style: Dictionary) -> String:
	var tidied := tidy(style)
	return "-".join([PREFIX, tidied[SHAPE], tidied[COLOUR], str(roundi(tidied[SIZE])), str(roundi(tidied[GAP])), str(roundi(tidied[THICKNESS])),
		"1" if tidied[OUTLINE] else "0", "1" if tidied[CENTRE_DOT] else "0"])


## The crosshair `code` describes, or {} if it isn't a code for one.
static func decode(code: Variant) -> Dictionary:
	if not code is String or code.length() > 60:
		return {}
	var parts: PackedStringArray = (code as String).strip_edges().split("-")
	if parts.size() != 8 or parts[0].to_upper() != PREFIX:
		return {}
	var shape := parts[1].to_lower()
	var colour := parts[2].to_lower()
	if not SHAPES.has(shape) or not COLOURS.has(colour):
		return {}
	for index in [3, 4, 5]:
		if not parts[index].is_valid_int():
			return {}
	if not parts[6] in ["0", "1"] or not parts[7] in ["0", "1"]:
		return {}
	return tidy({SHAPE: shape, COLOUR: colour, SIZE: parts[3].to_int(), GAP: parts[4].to_int(), THICKNESS: parts[5].to_int(), OUTLINE: parts[6] == "1", CENTRE_DOT: parts[7] == "1"})


## Makes `style` the player's crosshair.
static func apply(style: Dictionary) -> void:
	var tidied := tidy(style)
	for key: String in KEYS:
		Settings.set_value(key, tidied[key])


## What is drawn for `style` with its lines `spread` pixels from the middle
## for the weapon's spread, about the middle of the view: [{"kind": "line",
## "from", "to", "width"}, {"kind": "disc", "radius"}, {"kind": "ring",
## "radius", "width"}], each from the middle.
static func shapes(style: Dictionary, spread: float) -> Array[Dictionary]:
	var tidied := tidy(style)
	var drawn: Array[Dictionary] = []
	var gap: float = spread + tidied[GAP]
	var length: float = tidied[SIZE]
	var width: float = tidied[THICKNESS]
	match tidied[SHAPE]:
		CROSS, TEE:
			var ways := [Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT] if tidied[SHAPE] == TEE else [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]
			for way: Vector2 in ways:
				drawn.append({"kind": "line", "from": way * gap, "to": way * (gap + length), "width": width})
		CIRCLE:
			drawn.append({"kind": "ring", "radius": gap + length * 0.5, "width": width})
	if tidied[CENTRE_DOT] or tidied[SHAPE] == DOT:
		drawn.append({"kind": "disc", "radius": maxf(width * 0.75, 1.5) if tidied[SHAPE] != DOT else maxf(length * 0.25, 1.5)})
	return drawn


## Draws `style` on `canvas` about `centre`, with its lines `spread` pixels
## out for the weapon's spread.
static func draw(canvas: CanvasItem, centre: Vector2, style: Dictionary, spread: float) -> void:
	var tidied := tidy(style)
	var colour := Color(COLOURS[tidied[COLOUR]], ALPHA)
	var edge := Color(0, 0, 0, ALPHA * 0.8)
	var drawn := shapes(tidied, spread)
	# The dark edge first, a little fatter, under everything.
	if tidied[OUTLINE]:
		for shape in drawn:
			_draw_one(canvas, centre, shape, edge, EDGE)
	for shape in drawn:
		_draw_one(canvas, centre, shape, colour, 0.0)


static func _draw_one(canvas: CanvasItem, centre: Vector2, shape: Dictionary, colour: Color, fatter: float) -> void:
	match shape.kind:
		"line":
			var along: Vector2 = (shape.to - shape.from).normalized()
			canvas.draw_line(centre + shape.from - along * fatter, centre + shape.to + along * fatter, colour, shape.width + fatter * 2.0, true)
		"ring":
			canvas.draw_arc(centre, shape.radius, 0.0, TAU, 40, colour, shape.width + fatter * 2.0, true)
		"disc":
			canvas.draw_circle(centre, shape.radius + fatter, colour)
