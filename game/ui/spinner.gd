extends Control
## An indeterminate progress spinner: an arc circling a faint ring, drawn to fit
## the control's size.

const TURNS_PER_SECOND := 0.9
const ARC := TAU * 0.3
const THICKNESS := 4.0
const COLOR := Color(1.0, 0.82, 0.25)
const TRACK_COLOR := Color(1.0, 1.0, 1.0, 0.15)

var _angle := 0.0


func _process(delta: float) -> void:
	if is_visible_in_tree():
		_angle = fmod(_angle + TAU * TURNS_PER_SECOND * delta, TAU)
		queue_redraw()


func _draw() -> void:
	var center := size / 2.0
	var radius := minf(size.x, size.y) / 2.0 - THICKNESS / 2.0
	draw_arc(center, radius, 0.0, TAU, 48, TRACK_COLOR, THICKNESS, true)
	draw_arc(center, radius, _angle, _angle + ARC, 24, COLOR, THICKNESS, true)
