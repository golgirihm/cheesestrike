class_name PlayerName
## What a player is called. Names come from other players' machines, so the
## host cleans every name it is given before showing it to anyone.

const MAX_LENGTH := 16
const FALLBACK := "Player"
const CHEESES := [
	"Gouda", "Brie", "Cheddar", "Feta", "Edam", "Gruyère", "Manchego", "Halloumi",
	"Stilton", "Camembert", "Emmental", "Ricotta", "Paneer", "Mozzarella", "Comté", "Raclette",
]

# Characters that change how text around them is laid out without showing
# anything themselves: zero-width spaces and joiners, direction overrides and
# the byte order mark. A name made of these looks empty or spills its
# direction into the text next to it.
const _INVISIBLE := [0x00AD, 0x061C, 0x180E, 0xFEFF]
const _INVISIBLE_RANGES := [[0x200B, 0x200F], [0x202A, 0x202E], [0x2060, 0x206F]]


## `text` with control and invisible characters removed, runs of white space
## made one space, trimmed and cut to MAX_LENGTH. Empty if nothing is left.
static func clean(text: String) -> String:
	var kept := ""
	var pending_space := false
	for i in text.length():
		var code := text.unicode_at(i)
		if _is_space(code):
			pending_space = not kept.is_empty()
			continue
		if _is_hidden(code):
			continue
		if pending_space:
			kept += " "
			pending_space = false
		kept += char(code)
	return kept.left(MAX_LENGTH).strip_edges()


## A name for someone who hasn't chosen one, such as "Brie 42".
static func random_default(rng := RandomNumberGenerator.new()) -> String:
	return "%s %d" % [CHEESES[rng.randi_range(0, CHEESES.size() - 1)], rng.randi_range(10, 99)]


static func _is_space(code: int) -> bool:
	return code in [0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x20, 0x85, 0xA0, 0x1680, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000] \
		or (code >= 0x2000 and code <= 0x200A)


static func _is_hidden(code: int) -> bool:
	if code < 0x20 or (code >= 0x7F and code < 0xA0):
		return true
	if code in _INVISIBLE:
		return true
	for range_ in _INVISIBLE_RANGES:
		if code >= range_[0] and code <= range_[1]:
			return true
	return false
