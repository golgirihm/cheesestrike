class_name FrontEnd
extends Control
## The title screen: the game's name over the arena, which is alive behind
## it, and the ways in. Play leads to hosting and joining; the rest are a
## press away: the practice range, how to play, the character and weapons,
## the arena editor, the settings, and who made the game.

## One of the ways in was chosen: one of CHOICES' names.
signal chosen(what: String)

## Each way in: its name, what its button says, the button's name, and its
## icon (see Icons).
const CHOICES := [
	["play", "Play", "PlayButton", "play"],
	["practice", "Practice range", "PracticeButton", "practice"],
	["learn", "How to play", "TutorialButton", "learn"],
	["profile", "Profile", "ProfileButton", "medal_head"],
	["customise", "Customise", "CustomiseButton", "customise"],
	["edit", "Arena editor", "EditorButton", "edit"],
	["settings", "Settings", "TitleSettingsButton", "settings"],
	["credits", "Credits", "CreditsButton", "credits"],
]
const TITLE := "CheeseStrike"
const SUBTITLE := "Gouda Overload"
const CREDITS := [
	"CheeseStrike: Gouda Overload",
	"",
	"Made with the Godot Engine, which is free and open, under the MIT licence: godotengine.org/license",
	"Physics by Jolt.",
	"Everything seen and heard is made in the game's own code: there are no pictures, models or recordings by anyone else in it.",
	"",
	"Thank you for playing.",
]
const BUTTON_SIZE := Vector2(250, 42)

## The panel of ways in, and the panel of credits that takes its place.
var ways: Control
var credits: Control
var _buttons := {}


func _init() -> void:
	name = "FrontEnd"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	theme = Look.theme()
	ways = _ways()
	add_child(ways)
	credits = _credits()
	credits.visible = false
	add_child(credits)
	flash_warning = _flash_warning()
	add_child(flash_warning)


## A word, until it is put away, that the game flashes, and what calms it.
var flash_warning: Control
const FLASH_WARNING := "This game has flashing lights and a moving view. Less motion, in the settings, calms them."
const FLASH_SEEN := "flash_warning_seen"


func _flash_warning() -> Control:
	var panel := PanelContainer.new()
	panel.name = "FlashWarning"
	panel.visible = not Settings.get_value(FLASH_SEEN)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	panel.add_child(row)
	var words := Label.new()
	words.name = "FlashWarningText"
	words.text = FLASH_WARNING
	words.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	words.custom_minimum_size = Vector2(420, 0)
	words.add_theme_font_size_override("font_size", 14)
	row.add_child(words)
	var done := Button.new()
	done.name = "FlashWarningOk"
	done.text = "Got it"
	done.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	done.pressed.connect(func() -> void:
		Settings.set_value(FLASH_SEEN, true)
		panel.hide())
	row.add_child(done)
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_BOTTOM, Control.PRESET_MODE_MINSIZE, 8)
	panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	return panel


## The button for the way in called `what`.
func button(what: String) -> Button:
	return _buttons.get(what)


## Shows the title screen, at its ways in.
func open() -> void:
	credits.hide()
	ways.show()
	Look.arrive(self)


## Shows who made the game, or goes back from that to the ways in.
func show_credits(on: bool) -> void:
	ways.visible = not on
	if on:
		Look.arrive(credits)
	else:
		credits.hide()


func _ways() -> Control:
	var centre := CenterContainer.new()
	centre.name = "Ways"
	centre.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	centre.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 8)
	centre.add_child(column)
	var logo := HBoxContainer.new()
	logo.alignment = BoxContainer.ALIGNMENT_CENTER
	logo.add_theme_constant_override("separation", 12)
	column.add_child(logo)
	var wedge := TextureRect.new()
	wedge.name = "Wedge"
	wedge.texture = Icons.texture("health", 64, Look.CHEDDAR)
	wedge.stretch_mode = TextureRect.STRETCH_KEEP_CENTERED
	logo.add_child(wedge)
	var words := VBoxContainer.new()
	words.add_theme_constant_override("separation", -6)
	logo.add_child(words)
	var title := Label.new()
	title.name = "Title"
	title.text = TITLE
	Look.heading(title, Look.TITLE_SIZE)
	title.add_theme_constant_override("outline_size", 10)
	title.add_theme_color_override("font_outline_color", Look.RIND)
	words.add_child(title)
	var subtitle := Label.new()
	subtitle.name = "Subtitle"
	subtitle.text = SUBTITLE
	Look.heading(subtitle, Look.HEADING_SIZE, Look.CREAM)
	subtitle.add_theme_constant_override("outline_size", 8)
	subtitle.add_theme_color_override("font_outline_color", Look.RIND)
	words.add_child(subtitle)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0, 10)
	column.add_child(gap)
	for choice: Array in CHOICES:
		var way := Button.new()
		way.name = choice[2]
		way.text = choice[1]
		way.icon = Icons.texture(choice[3], 26, Look.CREAM)
		way.custom_minimum_size = BUTTON_SIZE
		way.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		way.alignment = HORIZONTAL_ALIGNMENT_LEFT
		way.pressed.connect(func() -> void: chosen.emit(choice[0]))
		column.add_child(way)
		_buttons[choice[0]] = way
	Look.emphasise(_buttons.play)
	return centre


func _credits() -> Control:
	var centre := CenterContainer.new()
	centre.name = "Credits"
	centre.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(440, 0)
	centre.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 22)
	panel.add_child(margin)
	var rows := VBoxContainer.new()
	rows.add_theme_constant_override("separation", 10)
	margin.add_child(rows)
	var heading := Label.new()
	heading.text = "Credits"
	heading.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	Look.heading(heading)
	rows.add_child(heading)
	var text := Label.new()
	text.name = "Text"
	text.text = "\n".join(CREDITS)
	text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	text.custom_minimum_size = Vector2(396, 0)
	rows.add_child(text)
	var back := Button.new()
	back.name = "CreditsBackButton"
	back.text = "Back"
	back.icon = Icons.texture("back", 22, Look.CREAM)
	back.custom_minimum_size = Vector2(0, 44)
	back.pressed.connect(show_credits.bind(false))
	rows.add_child(back)
	return centre
