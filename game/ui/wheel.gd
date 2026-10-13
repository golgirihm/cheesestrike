class_name Wheel
extends Control
## A wheel of choices, for a gamepad's stick or a thumb: the choices are set
## out in a ring, and pushing toward one picks it. Used for quick messages
## and for choosing a weapon.
##
## The first choice is at the top and the rest go round clockwise. A stick
## resting near its middle picks nothing.

## A choice was made: its place in the ring.
signal chosen(index: int)

## How far from its middle a stick must be pushed to pick anything.
const DEAD := 0.4
## How far out the choices are drawn, and how big the wheel is, in pixels.
const RADIUS := 130.0
const SIZE := 360.0

## What the choices say, and optionally an icon for each (see Icons).
var choices: Array = []
var icons: Array = []
## Which is picked: -1 for none.
var picked := -1


func _init() -> void:
	custom_minimum_size = Vector2(SIZE, SIZE)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false


## Which of `count` choices a push in `direction` picks (x to the right, y
## down, as a stick gives it): -1 if it is too slight to mean one.
static func choice(direction: Vector2, count: int) -> int:
	if count <= 0 or direction.length() < DEAD:
		return -1
	# Round from the top, clockwise, each choice in the middle of its slice.
	var angle := wrapf(atan2(direction.x, -direction.y), 0.0, TAU)
	return int(round(angle / (TAU / count))) % count


## Where choice `index` of `count` is drawn, from the middle of the wheel.
static func place(index: int, count: int, radius := RADIUS) -> Vector2:
	var angle := TAU * index / maxi(count, 1)
	return Vector2(sin(angle), -cos(angle)) * radius


## Opens the wheel on `what`, a list of what each choice says, with nothing
## picked.
func open(what: Array, with_icons: Array = []) -> void:
	choices = what
	icons = with_icons
	picked = -1
	mouse_filter = Control.MOUSE_FILTER_STOP
	show()
	queue_redraw()


## Points the wheel: the choice in that `direction` is picked.
func point(direction: Vector2) -> void:
	var now := choice(direction, choices.size())
	if now != picked:
		picked = now
		queue_redraw()


## Closes the wheel, making the choice that is picked, if one is. Returns
## which, or -1.
func close() -> int:
	var made := picked
	hide()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	if made >= 0:
		chosen.emit(made)
	picked = -1
	return made


# A thumb on a choice picks it and closes the wheel.
func _gui_input(event: InputEvent) -> void:
	if not visible:
		return
	var pressed: bool = (event is InputEventScreenTouch or event is InputEventMouseButton) and event.pressed
	if pressed:
		point((event.position - size / 2.0) / RADIUS)
		accept_event()
		close()


func _draw() -> void:
	var middle := size / 2.0
	draw_circle(middle, RADIUS + 60.0, Color(Look.RIND, 0.72))
	var font := get_theme_default_font()
	for index in choices.size():
		var at := middle + place(index, choices.size())
		var lit := index == picked
		draw_circle(at, 44.0, Color(Look.CHEDDAR, 0.9) if lit else Color(Look.RIND_LIGHT, 0.95))
		var colour := Look.RIND if lit else Look.CREAM
		var said := String(choices[index])
		if index < icons.size() and Icons.has(String(icons[index])):
			draw_texture(Icons.texture(String(icons[index]), 40, colour), at - Vector2(20, 26))
			draw_string(font, at + Vector2(-44, 32), said, HORIZONTAL_ALIGNMENT_CENTER, 88, 11, colour)
		else:
			draw_multiline_string(font, at + Vector2(-40, -4), said, HORIZONTAL_ALIGNMENT_CENTER, 80, 13, 3, colour)
