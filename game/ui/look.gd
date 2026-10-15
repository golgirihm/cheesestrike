class_name Look
## The look of the interface: its colours, its lettering, the shapes of its
## panels and buttons, and how a screen arrives. One theme, made here in
## code and given to the whole game, so that every screen is of a piece.
##
## The colours are cheese and what goes round it: a dark rind for panels,
## cream for words, cheddar for what is chosen or pressed, and wax red for
## what matters.

const RIND := Color(0.13, 0.1, 0.08)
const RIND_LIGHT := Color(0.24, 0.19, 0.14)
const CREAM := Color(0.98, 0.95, 0.86)
const CHEDDAR := Color(1.0, 0.76, 0.2)
const WAX := Color(0.83, 0.2, 0.17)
const FADED := Color(0.98, 0.95, 0.86, 0.55)
## How round the corners of panels and of buttons are, in pixels.
const PANEL_CORNER := 14
const BUTTON_CORNER := 9
## Seconds a screen takes to arrive.
const ARRIVE_TIME := 0.16
## The sizes of lettering.
const TEXT_SIZE := 16
const HEADING_SIZE := 26
const TITLE_SIZE := 46

static var _theme: Theme
static var _heading: Font


## The theme of the whole interface.
static func theme() -> Theme:
	if _theme != null:
		return _theme
	var made := Theme.new()
	_theme = made
	_dress(made)
	return made


## Puts the chosen size of text and contrast into effect everywhere.
static func refresh() -> void:
	if _theme != null:
		_dress(_theme)


## The size menus' text is drawn at, as chosen.
static func text_size() -> int:
	return roundi(TEXT_SIZE * clampf(Settings.get_value(MENU_TEXT), 1.0, MOST_TEXT))


const MENU_TEXT := "menu_text"
const HIGH_CONTRAST := "high_contrast"
## The most menus' text can be scaled by, so that every screen still fits.
const MOST_TEXT := 1.25


static func _dress(made: Theme) -> void:
	var stark: bool = Settings.get_value(HIGH_CONTRAST)
	made.default_font_size = text_size()
	# With high contrast: solid black behind everything, and plain white
	# edges where there were faint ones.
	var panel := _box(Color.BLACK if stark else Color(RIND, 0.9), PANEL_CORNER, Color.WHITE if stark else Color(CHEDDAR, 0.35), 2)
	made.set_stylebox("panel", "PanelContainer", panel)
	made.set_stylebox("panel", "Panel", panel)
	for kind: String in ["Button", "CheckButton", "OptionButton"]:
		made.set_stylebox("normal", kind, _box(Color.BLACK if stark else Color(RIND_LIGHT, 0.95), BUTTON_CORNER, Color.WHITE if stark else Color(CREAM, 0.12), 2 if stark else 1, 10))
		made.set_stylebox("hover", kind, _box(RIND_LIGHT.lightened(0.12), BUTTON_CORNER, Color(CHEDDAR, 0.8), 2, 10))
		made.set_stylebox("pressed", kind, _box(CHEDDAR.darkened(0.25), BUTTON_CORNER, CHEDDAR, 2, 10))
		made.set_stylebox("focus", kind, _box(Color(0, 0, 0, 0), BUTTON_CORNER, CHEDDAR, 2, 10))
		made.set_stylebox("disabled", kind, _box(Color(RIND_LIGHT, 0.45), BUTTON_CORNER, Color(CREAM, 0.05), 1, 10))
		made.set_color("font_color", kind, CREAM)
		made.set_color("font_hover_color", kind, Color.WHITE)
		made.set_color("font_pressed_color", kind, RIND)
		made.set_color("font_focus_color", kind, CREAM)
		made.set_color("font_disabled_color", kind, Color(CREAM, 0.75) if stark else FADED)
	made.set_color("font_color", "Label", CREAM)
	made.set_stylebox("normal", "LineEdit", _box(Color(0, 0, 0, 0.45), BUTTON_CORNER, Color(CREAM, 0.2), 1, 10))
	made.set_stylebox("focus", "LineEdit", _box(Color(0, 0, 0, 0.45), BUTTON_CORNER, CHEDDAR, 2, 10))
	made.set_color("font_color", "LineEdit", CREAM)
	made.set_color("font_placeholder_color", "LineEdit", Color(CREAM, 0.75) if stark else FADED)
	made.set_color("caret_color", "LineEdit", CHEDDAR)
	# A slider: a thin dark groove, filled in cheddar up to where it is set.
	made.set_stylebox("slider", "HSlider", _box(Color(0, 0, 0, 0.5), 4, Color(0, 0, 0, 0), 0, 0, 4))
	made.set_stylebox("grabber_area", "HSlider", _box(CHEDDAR.darkened(0.15), 4, Color(0, 0, 0, 0), 0, 0, 4))
	made.set_stylebox("grabber_area_highlight", "HSlider", _box(CHEDDAR, 4, Color(0, 0, 0, 0), 0, 0, 4))
	made.set_stylebox("fill", "ProgressBar", _box(CHEDDAR, 6))
	made.set_stylebox("background", "ProgressBar", _box(Color(0, 0, 0, 0.5), 6))


## The lettering of headings and titles: the same face, heavier and set a
## little wider.
static func heading_font() -> Font:
	if _heading == null:
		var heavy := FontVariation.new()
		heavy.variation_embolden = 0.9
		heavy.spacing_glyph = 1
		_heading = heavy
	return _heading


## Makes `label` a heading of `size`, in cheddar.
static func heading(label: Label, size := HEADING_SIZE, colour := CHEDDAR) -> void:
	label.add_theme_font_override("font", heading_font())
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", colour)


## Marks `button` as the one that matters on its screen: wax red.
static func emphasise(button: Button) -> void:
	button.add_theme_stylebox_override("normal", _box(WAX.darkened(0.1), BUTTON_CORNER, Color(CREAM, 0.25), 1, 10))
	button.add_theme_stylebox_override("hover", _box(WAX.lightened(0.1), BUTTON_CORNER, CHEDDAR, 2, 10))
	button.add_theme_font_override("font", heading_font())


## Whether screens move as they change: not for someone who has turned the
## camera's motion right down.
static func moves() -> bool:
	return Settings.motion("camera_motion") > 0.0


## Shows `screen`, which fades in and settles into place. Returns the tween
## that does it, or null where nothing moves.
static func arrive(screen: Control) -> Tween:
	screen.show()
	if not moves() or not screen.is_inside_tree():
		screen.modulate.a = 1.0
		return null
	screen.modulate.a = 0.0
	var tween := screen.create_tween().set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tween.tween_property(screen, "modulate:a", 1.0, ARRIVE_TIME)
	return tween


static func _box(fill: Color, corner: int, edge := Color(0, 0, 0, 0), edge_width := 0, pad := 0, thick := 0) -> StyleBoxFlat:
	var box := StyleBoxFlat.new()
	box.bg_color = fill
	box.set_corner_radius_all(corner)
	box.border_color = edge
	box.set_border_width_all(edge_width)
	box.anti_aliasing = true
	if pad > 0:
		box.content_margin_left = pad
		box.content_margin_right = pad
		box.content_margin_top = pad * 0.4
		box.content_margin_bottom = pad * 0.4
	if thick > 0:
		box.content_margin_top = thick
		box.content_margin_bottom = thick
	return box
