class_name WeaponBar
extends Control
## The weapons a player carries, as a row of icons, each with the number key
## that takes it in hand, and the one in hand lit. Touching it opens the
## wheel for choosing one.

signal touched

const ICON := 30.0
const GAP := 8.0

var _shown: Array[Dictionary] = []


func _init() -> void:
	name = "WeaponBar"
	mouse_filter = Control.MOUSE_FILTER_PASS
	custom_minimum_size = Vector2(0, ICON + 16.0)


## What the bar shows for someone carrying `carried` with `in_hand` in
## their hands: [{"weapon", "key": the number that takes it, "held"}], in
## the order of the keys.
static func entries(carried: Array, in_hand: String) -> Array[Dictionary]:
	var shown: Array[Dictionary] = []
	for index in Weapons.KEYS:
		var weapon := Weapons.at_key(carried, index)
		if not weapon.is_empty():
			shown.append({"weapon": weapon, "key": index + 1, "held": weapon == in_hand})
	return shown


## Shows the bar for `carried` and `in_hand`.
func show_weapons(carried: Array, in_hand: String) -> void:
	var now := entries(carried, in_hand)
	if now != _shown:
		_shown = now
		custom_minimum_size.x = _shown.size() * (ICON + GAP)
		queue_redraw()


## What is showing (see entries).
func shown() -> Array[Dictionary]:
	return _shown


func _gui_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch and event.pressed:
		touched.emit()
		accept_event()


func _draw() -> void:
	var font := get_theme_default_font()
	# From the right, as the bar sits in the bottom right corner.
	var left := size.x - _shown.size() * (ICON + GAP)
	for index in _shown.size():
		var entry: Dictionary = _shown[index]
		var at := Vector2(left + index * (ICON + GAP), 0.0)
		var colour := Look.CHEDDAR if entry.held else Color(1, 1, 1, 0.6)
		if entry.held:
			draw_rect(Rect2(at - Vector2(3, 2), Vector2(ICON + 6, ICON + 4)), Color(0, 0, 0, 0.35))
		draw_texture(Icons.texture(entry.weapon, int(ICON), colour), at)
		draw_string_outline(font, at + Vector2(ICON / 2.0 - 4.0, ICON + 14.0), str(entry.key), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, 4, Color.BLACK)
		draw_string(font, at + Vector2(ICON / 2.0 - 4.0, ICON + 14.0), str(entry.key), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, colour)
