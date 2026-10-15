class_name TeamColors
## The teams' colours, from a palette that people with the common kinds of
## colour blindness can tell apart: Cheddar a red orange and Brie blue.
## Cheddar's is not the yellow orange of cheese itself, which is kept for
## cheese that can be eaten. Teams also
## differ by the shape of the badge over each player, so colour is never the
## only clue.
##
## A player can choose a set made for their own kind of colour blindness,
## and then any colour of their own for each team and for the mark of a hit.

const CHEDDAR := Color(0.835, 0.369, 0.0)
const BRIE := Color(0.337, 0.706, 0.914)
const PRESET := "colour_preset"
const OWN_CHEDDAR := "colour_cheddar"
const OWN_BRIE := "colour_brie"
const OWN_HIT := "colour_hit"
## The sets, each [Cheddar, Brie], with what each is called. They are taken
## from Okabe and Ito's palette, chosen to stay apart for each kind.
const PRESETS := {
	"standard": [CHEDDAR, BRIE],
	"protanopia": [Color(0.902, 0.624, 0.0), Color(0.0, 0.447, 0.698)],
	"deuteranopia": [Color(0.941, 0.894, 0.259), Color(0.0, 0.447, 0.698)],
	"tritanopia": [Color(0.835, 0.369, 0.0), Color(0.0, 0.62, 0.451)],
}
const PRESET_NAMES := {"standard": "Standard", "protanopia": "Red-blind (protanopia)", "deuteranopia": "Green-blind (deuteranopia)",
	"tritanopia": "Blue-blind (tritanopia)"}
## The colours a player can choose for themselves; "" keeps the set's.
const OWN := {"": Color.TRANSPARENT, "red": Color(0.9, 0.2, 0.2), "orange": Color(0.902, 0.624, 0.0), "yellow": Color(0.941, 0.894, 0.259),
	"green": Color(0.3, 0.85, 0.35), "teal": Color(0.0, 0.62, 0.451), "blue": Color(0.0, 0.447, 0.698), "sky": Color(0.337, 0.706, 0.914),
	"purple": Color(0.6, 0.35, 0.85), "pink": Color(0.8, 0.475, 0.655), "white": Color.WHITE}
const OWN_NAMES := {"": "As the set", "red": "Red", "orange": "Orange", "yellow": "Yellow", "green": "Green", "teal": "Teal", "blue": "Blue",
	"sky": "Sky blue", "purple": "Purple", "pink": "Pink", "white": "White"}


static func of(team: int) -> Color:
	match team:
		MatchState.CHEDDAR:
			return chosen(Settings.get_value(PRESET), Settings.get_value(OWN_CHEDDAR), 0)
		MatchState.BRIE:
			return chosen(Settings.get_value(PRESET), Settings.get_value(OWN_BRIE), 1)
	return Color.WHITE


## The colour of a mark for a plain hit, as chosen.
static func hit() -> Color:
	var own: String = Settings.get_value(OWN_HIT)
	return OWN[own] if own != "" and OWN.has(own) else Color.WHITE


## The colour of the team at `index` (0 Cheddar, 1 Brie) in the set
## `preset`, unless `own` chooses another.
static func chosen(preset: String, own: String, index: int) -> Color:
	if own != "" and OWN.has(own):
		return OWN[own]
	return PRESETS.get(preset, PRESETS.standard)[index]


## How far apart two colours look, roughly, from 0 to about 1.7: their
## distance in a space where equal steps look about equal.
static func apart(a: Color, b: Color) -> float:
	return absf(a.ok_hsl_l - b.ok_hsl_l) + Vector2.from_angle(a.ok_hsl_h * TAU).distance_to(Vector2.from_angle(b.ok_hsl_h * TAU)) * minf(a.ok_hsl_s, b.ok_hsl_s)
