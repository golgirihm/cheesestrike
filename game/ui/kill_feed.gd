extends VBoxContainer
## The last few knock-outs, in the top-right corner, each for a few seconds.

const MAX_ENTRIES := 5
const ENTRY_LIFE := 6.0

## Knock-outs announced since the session began, and the latest, for the
## browser tests.
var announced := 0
var latest := ""

# Each entry: {"text": String, "left": seconds}.
var _entries: Array[Dictionary] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	alignment = BoxContainer.ALIGNMENT_BEGIN


## How a knock-out reads in the feed.
static func entry_text(attacker: String, victim: String, weapon: String, head: bool) -> String:
	var stats := Weapons.find(weapon)
	var using := stats.display_name if stats != null else weapon
	return "%s  [%s%s]  %s" % [attacker, using, ", head" if head else "", victim]


func add(attacker: String, victim: String, weapon: String, head: bool) -> void:
	latest = entry_text(attacker, victim, weapon, head)
	announced += 1
	_entries.append({"text": latest, "left": ENTRY_LIFE})
	while _entries.size() > MAX_ENTRIES:
		_entries.pop_front()
	_rebuild()


func clear() -> void:
	_entries.clear()
	_rebuild()


## What the feed shows now, top to bottom.
func showing() -> Array[String]:
	var texts: Array[String] = []
	for entry in _entries:
		texts.append(entry.text)
	return texts


func _process(delta: float) -> void:
	var before := _entries.size()
	for entry in _entries:
		entry.left -= delta
	_entries = _entries.filter(func(entry: Dictionary) -> bool: return entry.left > 0.0)
	if _entries.size() != before:
		_rebuild()


func _rebuild() -> void:
	for child in get_children():
		child.queue_free()
	for entry in _entries:
		var label := Label.new()
		label.text = entry.text
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		label.add_theme_font_size_override("font_size", CombatHud.text_size(15))
		label.add_theme_color_override("font_outline_color", Color.BLACK)
		label.add_theme_constant_override("outline_size", 5)
		label.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(label)
