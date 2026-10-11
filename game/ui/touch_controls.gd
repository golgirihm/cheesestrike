extends Control
## The on-screen buttons a touch screen needs for what has no gesture of its
## own: fire, reload and the scoreboard. Moving, looking and jumping are gestures on the screen
## itself (see the player).
##
## The buttons show once the screen has been touched, or from the start on a
## device that reports a touch screen. The player asks button_at() whether a
## touch landed on one; this node only draws them.

## Each button's centre, as a point on the screen (`anchor`, 0 to 1 across and
## down) moved by `offset`, and its radius, in the interface's pixels.
const BUTTONS := {
	"fire": {"anchor": Vector2(1, 1), "offset": Vector2(-96.0, -150.0), "radius": 56.0, "label": "FIRE", "font_size": 18},
	"reload": {"anchor": Vector2(1, 1), "offset": Vector2(-200.0, -74.0), "radius": 34.0, "label": "R", "font_size": 18},
	"scores": {"anchor": Vector2(0.5, 0), "offset": Vector2(0.0, 44.0), "radius": 30.0, "label": "Scores", "font_size": 13},
}
const COLOR := Color(1.0, 1.0, 1.0, 0.22)
const HELD_COLOR := Color(1.0, 0.82, 0.25, 0.45)
const LABEL_COLOR := Color(1.0, 1.0, 1.0, 0.85)

var shown := false
## Names of the buttons being held, drawn lit.
var held: Array = []:
	set(value):
		held = value
		queue_redraw()


func _ready() -> void:
	add_to_group("touch_controls")
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	shown = DisplayServer.is_touchscreen_available()
	resized.connect(queue_redraw)


## A touch has been seen, so this is a touch screen whatever it reported.
func notice_touch() -> void:
	if not shown:
		shown = true
		queue_redraw()


## Where each button is on a screen of `screen` size: name -> {centre, radius}.
static func layout_for(screen: Vector2) -> Dictionary:
	var placed := {}
	for name: String in BUTTONS:
		placed[name] = {"centre": screen * BUTTONS[name].anchor + BUTTONS[name].offset, "radius": BUTTONS[name].radius}
	return placed


## The name of the button at `point`, or "" if none is there or none are showing.
func button_at(point: Vector2) -> String:
	if not shown or not is_visible_in_tree():
		return ""
	var placed := layout_for(size)
	for name: String in placed:
		if point.distance_to(placed[name].centre) <= placed[name].radius:
			return name
	return ""


func _draw() -> void:
	if not shown:
		return
	var font := get_theme_default_font()
	var placed := layout_for(size)
	for name: String in placed:
		var centre: Vector2 = placed[name].centre
		var radius: float = placed[name].radius
		draw_circle(centre, radius, HELD_COLOR if held.has(name) else COLOR)
		draw_arc(centre, radius, 0.0, TAU, 48, LABEL_COLOR, 2.0, true)
		var label: String = BUTTONS[name].label
		var font_size: int = BUTTONS[name].font_size
		var width := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
		draw_string(font, centre + Vector2(-width / 2.0, font_size * 0.35), label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, LABEL_COLOR)
