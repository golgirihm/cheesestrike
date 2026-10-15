class_name JoinScreen
extends Control
## What is shown while a session is being joined or started: the arena's
## name, a picture of what it is made of, and a tip, until the arena is
## there to stand in.

## Things worth knowing, one of which is shown each time.
const TIPS := [
	"Aim down the sights for a tighter shot. It is slower to move, so pick your moment.",
	"A crouched player treads softly: opponents hear running from across the arena.",
	"Dealing damage fills your Overload. Set it off when it is full.",
	"Cheese blocks can be shot away. Cover that is cheese is cover for a while.",
	"A minute in, giant mice come to eat the cheese. Shoot one to send it running, but it will give you away.",
	"Power-ups come back on a timer, and everyone is told when one is taken.",
	"Teammates are outlined through walls. Opponents never are.",
	"A hit to the head does more. A hit from far away does less.",
	"Hold the Talk key to speak to your team.",
	"The launcher's wheel bursts where it lands, and hurts whoever fired it too.",
	"Out of ammunition? Your sidearm never runs dry.",
	"Jump pads throw you the way they point. So do bursts, a little.",
	"The scoreboard shows everyone's ping. A high one is why shots seem late.",
]
## What is said before the arena is known.
const NOT_KNOWN := "Finding the session…"

var _what: Label
var _arena: Label
var _picture: TextureRect
var _tip: Label
var _shown_tip := ""


func _init() -> void:
	name = "JoinScreen"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	theme = Look.theme()
	visible = false
	add_to_group("touch_blockers")
	var shade := ColorRect.new()
	shade.color = Color(Look.RIND, 0.82)
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(shade)
	var centre := CenterContainer.new()
	centre.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(centre)
	var panel := PanelContainer.new()
	panel.name = "Panel"
	panel.custom_minimum_size = Vector2(440, 0)
	centre.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 10)
	panel.add_child(margin)
	var rows := VBoxContainer.new()
	rows.add_theme_constant_override("separation", 12)
	margin.add_child(rows)
	_what = Label.new()
	_what.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	rows.add_child(_what)
	_arena = Label.new()
	_arena.name = "Arena"
	_arena.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	Look.heading(_arena, Look.HEADING_SIZE + 6)
	rows.add_child(_arena)
	_picture = TextureRect.new()
	_picture.name = "Picture"
	_picture.custom_minimum_size = Vector2(396, 110)
	_picture.stretch_mode = TextureRect.STRETCH_TILE
	_picture.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	rows.add_child(_picture)
	_tip = Label.new()
	_tip.name = "Tip"
	_tip.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_tip.custom_minimum_size = Vector2(396, 0)
	_tip.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_tip.add_theme_color_override("font_color", Look.FADED)
	rows.add_child(_tip)


## The tip shown for the `turn`th time a session is joined: each in its
## turn, so that all are seen.
static func tip(turn: int) -> String:
	return TIPS[posmod(turn, TIPS.size())]


## What the floor of arena `map` is made of (see Textures), for its picture.
static func ground(map: String) -> String:
	return {"city": Textures.ASPHALT, "dairy": Textures.TILE, "kitchen": Textures.TILE, "market": Textures.COBBLE, "cave": Textures.ROCK,
		"duel": Textures.BREAD, "canyon": Textures.DIRT, "box": Textures.DIRT}.get(map, Textures.DIRT)


## Shows the screen, saying `what` is happening ("Joining ABCD…") and, if
## it is known already, which arena it is.
func open(what: String, map := "") -> void:
	_what.text = what
	_shown_tip = tip(Settings.get_value("tips_seen"))
	Settings.set_value("tips_seen", Settings.get_value("tips_seen") + 1)
	_tip.text = "Tip: " + _shown_tip
	set_arena(map)
	Look.arrive(self)


## Says which arena it is, once that is known: "" for not yet.
func set_arena(map: String) -> void:
	var known := Maps.exists(map) and not map.is_empty()
	_arena.text = Maps.NAMES[map] if known else NOT_KNOWN
	_picture.texture = Textures.picture(ground(map)) if known else null
	_picture.modulate = Picture.grade(map).tint.darkened(0.25) if known else Color.WHITE


## What the screen is showing: {"arena", "tip"}.
func showing() -> Dictionary:
	return {"arena": _arena.text, "tip": _shown_tip}
