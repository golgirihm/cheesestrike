class_name Looks
## What a player has chosen their character to look like: which kind of
## cheese it is, the hat on its tip and what it holds in its free hand. None
## of it changes what can be hit, how the character moves or how it fights.
##
## A look travels as one whole number, each choice a few bits of it.

const CHEESES := ["swiss", "cheddar", "blue", "brie"]
const CHEESE_NAMES := {"swiss": "Swiss", "cheddar": "Cheddar", "blue": "Blue", "brie": "Brie"}
const HATS := ["none", "party", "top", "crown", "chef", "cowboy"]
const HAT_NAMES := {"none": "None", "party": "Party hat", "top": "Top hat", "crown": "Crown", "chef": "Chef's hat", "cowboy": "Cowboy hat"}
const HELD := ["none", "balloon", "flag", "lollipop"]
const HELD_NAMES := {"none": "Nothing", "balloon": "Balloon", "flag": "Flag", "lollipop": "Lollipop"}

## The settings that hold each choice, with the choices and their names.
const SETTINGS := {
	"look_cheese": [CHEESES, CHEESE_NAMES, "Cheese"],
	"look_hat": [HATS, HAT_NAMES, "Hat"],
	"look_held": [HELD, HELD_NAMES, "Holding"],
}


## The look with these choices, as a number. Anything unknown is the first
## of its kind.
static func pack(cheese: String, hat: String, held: String) -> int:
	return maxi(CHEESES.find(cheese), 0) | maxi(HATS.find(hat), 0) << 4 | maxi(HELD.find(held), 0) << 8


## The choices in `look`, as {cheese, hat, held}. Anything out of range is
## the first of its kind.
static func unpack(look: int) -> Dictionary:
	var cheese := look & 15
	var hat := look >> 4 & 15
	var held := look >> 8 & 15
	return {
		"cheese": CHEESES[cheese] if cheese < CHEESES.size() else CHEESES[0],
		"hat": HATS[hat] if hat < HATS.size() else HATS[0],
		"held": HELD[held] if held < HELD.size() else HELD[0],
	}


## `look` as sent by another machine, with anything it shouldn't hold put
## right.
static func clean(look: Variant) -> int:
	if not look is int:
		return 0
	var choices := unpack(look)
	return pack(choices.cheese, choices.hat, choices.held)


## The look this player has chosen in their settings.
static func chosen() -> int:
	return pack(Settings.get_value("look_cheese"), Settings.get_value("look_hat"), Settings.get_value("look_held"))
