extends CanvasLayer
## TEMPORARY, not for merging: a panel for trying the picture's values live,
## in a match or behind the menu. F8 shows or hides it. "Before" and "After"
## snap every value to how it was before and after the grounded-picture
## change, for the arena and sky showing; "Copy values" puts what is set on
## the clipboard as JSON.
##
## The game sets these values itself when the arena, the sky or a picture
## setting changes, which undoes what was tried here: press "Re-read" to see
## what it set.

## How the picture was before the grounded-picture change.
const BEFORE_GRADES := {
	"canyon": {"brightness": 1.0, "contrast": 1.06, "saturation": 1.12, "tint": Color(1.0, 0.97, 0.9)},
	"box": {"brightness": 1.02, "contrast": 1.04, "saturation": 1.08, "tint": Color(1.0, 0.99, 0.95)},
	"city": {"brightness": 1.0, "contrast": 1.1, "saturation": 0.9, "tint": Color(0.94, 0.97, 1.0)},
	"dairy": {"brightness": 1.03, "contrast": 1.03, "saturation": 1.0, "tint": Color(0.97, 0.99, 1.0)},
	"kitchen": {"brightness": 1.0, "contrast": 1.05, "saturation": 1.1, "tint": Color(1.0, 0.96, 0.88)},
	"market": {"brightness": 1.0, "contrast": 1.05, "saturation": 1.18, "tint": Color(1.0, 0.98, 0.93)},
	"cave": {"brightness": 0.98, "contrast": 1.12, "saturation": 0.92, "tint": Color(0.9, 0.96, 1.0)},
	"duel": {"brightness": 1.0, "contrast": 1.1, "saturation": 1.0, "tint": Color(1.0, 0.97, 0.94)},
}
const BEFORE_LOOKS := {
	"noon": {"top": Color(0.2, 0.46, 0.9), "horizon": Color(0.72, 0.87, 0.98), "cloud": Color(1, 1, 1), "shade": Color(0.76, 0.81, 0.9), "clear": 0.5,
		"sun": Color(1, 0.98, 0.94), "energy": 1.0, "ambient": Color(0.82, 0.88, 1), "ambient_energy": 0.5, "fog": 0.0, "fog_colour": Color(0.8, 0.88, 0.98)},
	"dawn": {"top": Color(0.32, 0.45, 0.78), "horizon": Color(1.0, 0.72, 0.58), "cloud": Color(1.0, 0.86, 0.8), "shade": Color(0.72, 0.6, 0.7), "clear": 0.45,
		"sun": Color(1.0, 0.78, 0.62), "energy": 0.8, "ambient": Color(0.92, 0.8, 0.82), "ambient_energy": 0.45, "fog": 0.006, "fog_colour": Color(1.0, 0.8, 0.72)},
	"dusk": {"top": Color(0.2, 0.22, 0.5), "horizon": Color(0.98, 0.5, 0.3), "cloud": Color(1.0, 0.66, 0.5), "shade": Color(0.48, 0.36, 0.52), "clear": 0.5,
		"sun": Color(1.0, 0.6, 0.38), "energy": 0.75, "ambient": Color(0.78, 0.66, 0.8), "ambient_energy": 0.42, "fog": 0.004, "fog_colour": Color(0.9, 0.55, 0.42)},
	"night": {"top": Color(0.02, 0.03, 0.09), "horizon": Color(0.09, 0.12, 0.24), "cloud": Color(0.3, 0.34, 0.46), "shade": Color(0.12, 0.14, 0.22), "clear": 0.62,
		"sun": Color(0.62, 0.72, 1.0), "energy": 0.42, "ambient": Color(0.42, 0.5, 0.78), "ambient_energy": 0.5, "fog": 0.003, "fog_colour": Color(0.06, 0.08, 0.16)},
	"rain": {"top": Color(0.4, 0.45, 0.52), "horizon": Color(0.62, 0.66, 0.7), "cloud": Color(0.72, 0.74, 0.78), "shade": Color(0.45, 0.48, 0.54), "clear": 0.12,
		"sun": Color(0.82, 0.86, 0.92), "energy": 0.55, "ambient": Color(0.72, 0.78, 0.86), "ambient_energy": 0.6, "fog": 0.012, "fog_colour": Color(0.6, 0.64, 0.7)},
	"dust": {"top": Color(0.62, 0.5, 0.36), "horizon": Color(0.8, 0.64, 0.46), "cloud": Color(0.86, 0.74, 0.58), "shade": Color(0.62, 0.5, 0.38), "clear": 0.3,
		"sun": Color(1.0, 0.84, 0.62), "energy": 0.7, "ambient": Color(0.9, 0.78, 0.62), "ambient_energy": 0.55, "fog": 0.022, "fog_colour": Color(0.78, 0.62, 0.44)},
}
const BEFORE_FILL := Color(0.62, 0.74, 1.0)
const BEFORE_FILL_ENERGY := 0.28
const LIGHTING := preload("res://world/lighting.gd")
## The engine's tone mappers, in the order of its enum.
const MAPPERS := ["Linear", "Reinhard", "Filmic", "ACES", "AgX"]

var world: Node3D
var _panel: PanelContainer
var _where: Label
var _knobs := {}
var _setting := false


func _init(on: Node3D) -> void:
	world = on
	name = "PictureTuner"
	layer = 60


func _ready() -> void:
	_panel = PanelContainer.new()
	_panel.theme = Look.theme()
	_panel.anchor_left = 1.0
	_panel.anchor_right = 1.0
	_panel.anchor_bottom = 1.0
	_panel.offset_left = -400
	_panel.visible = false
	# Keeps the player from taking the mouse back while it is open.
	_panel.add_to_group("pause_menu")
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.08, 0.08, 0.09, 0.88)
	style.content_margin_left = 10
	style.content_margin_right = 10
	style.content_margin_top = 8
	style.content_margin_bottom = 8
	_panel.add_theme_stylebox_override("panel", style)
	add_child(_panel)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_panel.add_child(scroll)
	var rows := VBoxContainer.new()
	rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(rows)
	var title := Label.new()
	title.text = "Picture tuner (F8 to hide)"
	rows.add_child(title)
	_where = Label.new()
	rows.add_child(_where)
	var buttons := HBoxContainer.new()
	rows.add_child(buttons)
	for pair: Array in [["Before", func() -> void: _apply(_before())], ["After", func() -> void: _apply(_after())],
			["Copy values", _copy], ["Re-read", _refresh]]:
		var button := Button.new()
		button.text = pair[0]
		button.pressed.connect(pair[1])
		buttons.add_child(button)
	_build(rows)


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_F8:
		_panel.visible = not _panel.visible
		if _panel.visible:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
			_refresh()
		get_viewport().set_input_as_handled()


func _environment() -> Environment:
	return world.get_node("WorldEnvironment").environment


func _sun() -> DirectionalLight3D:
	return world.get_node("Sun")


func _fill() -> DirectionalLight3D:
	return world.lighting.get_node("Fill") if world.lighting != null else null


func _sky() -> ShaderMaterial:
	var environment := _environment()
	return environment.sky.sky_material if environment.sky != null else null


func _shade() -> ShaderMaterial:
	return world.vignette.get_node("Shade").material if world.vignette != null else null


func _build(rows: VBoxContainer) -> void:
	_heading(rows, "Tone mapping")
	_mapper(rows, "tonemap", func() -> int: return _environment().tonemap_mode,
		func(value: int) -> void: _environment().tonemap_mode = value)
	_range(rows, "exposure", 0.2, 3.0, func() -> float: return _environment().tonemap_exposure,
		func(value: float) -> void: _environment().tonemap_exposure = value)
	_range(rows, "white", 0.5, 12.0, func() -> float: return _environment().tonemap_white,
		func(value: float) -> void: _environment().tonemap_white = value)
	_heading(rows, "Glow")
	_check(rows, "glow_on", func() -> bool: return _environment().glow_enabled,
		func(value: bool) -> void: _environment().glow_enabled = value)
	_range(rows, "glow_intensity", 0.0, 2.0, func() -> float: return _environment().glow_intensity,
		func(value: float) -> void: _environment().glow_intensity = value)
	_range(rows, "glow_threshold", 0.0, 2.0, func() -> float: return _environment().glow_hdr_threshold,
		func(value: float) -> void: _environment().glow_hdr_threshold = value)
	_heading(rows, "Cast of colour")
	for key: String in ["brightness", "contrast", "saturation"]:
		_range(rows, key, 0.0, 2.0, func() -> float: return _environment().get("adjustment_" + key),
			func(value: float) -> void:
				_environment().adjustment_enabled = true
				_environment().set("adjustment_" + key, value))
	_colour(rows, "tint", func() -> Color:
		var ramp := _environment().adjustment_color_correction as GradientTexture1D
		return ramp.gradient.get_color(1) if ramp != null else Color.WHITE,
		func(value: Color) -> void:
			_environment().adjustment_enabled = true
			_environment().adjustment_color_correction = Picture._tint(value))
	_range(rows, "vignette", 0.0, 1.0, func() -> float: return _shade().get_shader_parameter("strength") if _shade() != null else 0.0,
		func(value: float) -> void:
			if _shade() != null:
				_shade().set_shader_parameter("strength", value))
	_heading(rows, "Light")
	_colour(rows, "sun", func() -> Color: return _sun().light_color, func(value: Color) -> void: _sun().light_color = value)
	_range(rows, "sun_energy", 0.0, 3.0, func() -> float: return _sun().light_energy, func(value: float) -> void: _sun().light_energy = value)
	_colour(rows, "ambient", func() -> Color: return _environment().ambient_light_color,
		func(value: Color) -> void: _environment().ambient_light_color = value)
	_range(rows, "ambient_energy", 0.0, 1.5, func() -> float: return _environment().ambient_light_energy,
		func(value: float) -> void: _environment().ambient_light_energy = value)
	_colour(rows, "fill", func() -> Color: return _fill().light_color if _fill() != null else Color.WHITE,
		func(value: Color) -> void:
			if _fill() != null:
				_fill().light_color = value)
	_range(rows, "fill_energy", 0.0, 1.0, func() -> float: return _fill().light_energy if _fill() != null else 0.0,
		func(value: float) -> void:
			if _fill() != null:
				_fill().light_energy = value)
	_heading(rows, "Haze")
	_range(rows, "fog", 0.0, 0.03, func() -> float: return _environment().fog_density if _environment().fog_enabled else 0.0,
		func(value: float) -> void:
			_environment().fog_enabled = value > 0.0
			_environment().fog_density = value)
	_colour(rows, "fog_colour", func() -> Color: return _environment().fog_light_color,
		func(value: Color) -> void: _environment().fog_light_color = value)
	_heading(rows, "Sky")
	for pair: Array in [["top", "top_color"], ["horizon", "horizon_color"], ["cloud", "cloud_color"], ["cloud_shade", "cloud_shade"]]:
		_colour(rows, pair[0], func() -> Color: return _sky().get_shader_parameter(pair[1]) if _sky() != null else Color.WHITE,
			func(value: Color) -> void:
				if _sky() != null:
					_sky().set_shader_parameter(pair[1], value))
	_range(rows, "clear", 0.0, 1.0, func() -> float: return _sky().get_shader_parameter("clear") if _sky() != null else 0.0,
		func(value: float) -> void:
			if _sky() != null:
				_sky().set_shader_parameter("clear", value))


func _heading(rows: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_color_override("font_color", Look.CHEDDAR)
	rows.add_child(label)


func _row(rows: VBoxContainer, key: String) -> HBoxContainer:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = key
	label.custom_minimum_size.x = 130
	row.add_child(label)
	rows.add_child(row)
	return row


func _range(rows: VBoxContainer, key: String, low: float, high: float, read: Callable, write: Callable) -> void:
	var row := _row(rows, key)
	var slider := HSlider.new()
	slider.min_value = low
	slider.max_value = high
	slider.step = (high - low) / 1000.0
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(slider)
	var shown := Label.new()
	shown.custom_minimum_size.x = 60
	row.add_child(shown)
	slider.value_changed.connect(func(value: float) -> void:
		shown.text = _number(value)
		if not _setting:
			write.call(value))
	_knobs[key] = {"read": read, "write": write, "show": func(value: Variant) -> void:
		slider.set_value_no_signal(value)
		shown.text = _number(value)}


func _colour(rows: VBoxContainer, key: String, read: Callable, write: Callable) -> void:
	var row := _row(rows, key)
	var button := ColorPickerButton.new()
	button.edit_alpha = false
	button.custom_minimum_size = Vector2(0, 26)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.color_changed.connect(func(value: Color) -> void:
		if not _setting:
			write.call(value))
	row.add_child(button)
	_knobs[key] = {"read": read, "write": write, "show": func(value: Variant) -> void: button.color = value}


func _mapper(rows: VBoxContainer, key: String, read: Callable, write: Callable) -> void:
	var row := _row(rows, key)
	var choice := OptionButton.new()
	for name: String in MAPPERS:
		choice.add_item(name)
	choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	choice.item_selected.connect(func(index: int) -> void:
		if not _setting:
			write.call(index))
	row.add_child(choice)
	_knobs[key] = {"read": read, "write": write, "show": func(value: Variant) -> void: choice.select(value)}


func _check(rows: VBoxContainer, key: String, read: Callable, write: Callable) -> void:
	var row := _row(rows, key)
	var box := CheckBox.new()
	box.toggled.connect(func(value: bool) -> void:
		if not _setting:
			write.call(value))
	row.add_child(box)
	_knobs[key] = {"read": read, "write": write, "show": func(value: Variant) -> void: box.set_pressed_no_signal(value)}


## Shows on every control what the game is showing now.
func _refresh() -> void:
	_setting = true
	for key: String in _knobs:
		_knobs[key].show.call(_knobs[key].read.call())
	_setting = false
	_where.text = "Arena: %s   Sky: %s" % [world.map_id, world.atmosphere.showing if world.atmosphere != null else "?"]


func _apply(values: Dictionary) -> void:
	for key: String in values:
		_knobs[key].write.call(values[key])
	_refresh()


func _sky_id() -> String:
	return world.atmosphere.showing if world.atmosphere != null and world.atmosphere.showing != "" else Atmosphere.NOON


func _values(grade: Dictionary, look: Dictionary, mapper: int, glow: Array, fill: Color, fill_energy: float) -> Dictionary:
	return {"tonemap": mapper, "exposure": 1.0, "white": 1.0, "glow_on": true, "glow_intensity": glow[0], "glow_threshold": glow[1],
		"brightness": grade.brightness, "contrast": grade.contrast, "saturation": grade.saturation, "tint": grade.tint,
		"vignette": Picture.VIGNETTE_STRENGTH, "sun": look.sun, "sun_energy": look.energy, "ambient": look.ambient,
		"ambient_energy": look.ambient_energy, "fill": fill, "fill_energy": fill_energy, "fog": look.fog, "fog_colour": look.fog_colour,
		"top": look.top, "horizon": look.horizon, "cloud": look.cloud, "cloud_shade": look.shade, "clear": look.clear}


func _before() -> Dictionary:
	return _values(BEFORE_GRADES.get(world.map_id, Picture.PLAIN), BEFORE_LOOKS[_sky_id()], Environment.TONE_MAPPER_LINEAR,
		[0.5, 0.92], BEFORE_FILL, BEFORE_FILL_ENERGY)


func _after() -> Dictionary:
	var values := _values(Picture.grade(world.map_id), Atmosphere.LOOKS[_sky_id()], Picture.TONE_MAPPER,
		[Picture.GLOW_INTENSITY, Picture.GLOW_THRESHOLD], LIGHTING.FILL, LIGHTING.FILL_ENERGY)
	values.exposure = Picture.EXPOSURE
	return values


func _copy() -> void:
	var out := {"arena": world.map_id, "sky": _sky_id()}
	for key: String in _knobs:
		var value: Variant = _knobs[key].read.call()
		if value is Color:
			value = [snappedf(value.r, 0.001), snappedf(value.g, 0.001), snappedf(value.b, 0.001)]
		elif value is float:
			value = snappedf(value, 0.0001)
		elif key == "tonemap":
			value = MAPPERS[value]
		out[key] = value
	var text := JSON.stringify(out, "  ", false)
	DisplayServer.clipboard_set(text)
	print(text)


static func _number(value: float) -> String:
	return "%.4f" % value if absf(value) < 0.1 else "%.3f" % value
