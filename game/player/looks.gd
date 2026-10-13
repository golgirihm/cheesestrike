class_name Looks
## What a player has chosen their character to look like: which kind of
## cheese it is, the hat on its tip and what it holds in its free hand. None
## of it changes what can be hit, how the character moves or how it fights.
##
## A look travels as one whole number, each choice a few bits of it. With it
## go the title the player has chosen and their level, which are shown by
## their name: a badge, by their own machine's word, that nothing checks.

const CHEESES := ["swiss", "cheddar", "blue", "brie"]
const CHEESE_NAMES := {"swiss": "Swiss", "cheddar": "Cheddar", "blue": "Blue", "brie": "Brie"}
const HATS := ["none", "party", "top", "crown", "chef", "cowboy"]
const HAT_NAMES := {"none": "None", "party": "Party hat", "top": "Top hat", "crown": "Crown", "chef": "Chef's hat", "cowboy": "Cowboy hat"}
const HELD := ["none", "balloon", "flag", "lollipop"]
const HELD_NAMES := {"none": "Nothing", "balloon": "Balloon", "flag": "Flag", "lollipop": "Lollipop"}

## The titles a player can go by, with how each reads.
const TITLES := ["none", "fresh", "big_cheese", "mouser", "sharpshooter", "wheel_roller", "old_hand", "grand_fromage"]
const TITLE_NAMES := {"none": "No title", "fresh": "Fresh from the Dairy", "big_cheese": "The Big Cheese", "mouser": "Mouser", "sharpshooter": "Sharpshooter",
	"wheel_roller": "Wheel Roller", "old_hand": "Old Hand", "grand_fromage": "Le Grand Fromage"}
## The setting that holds the title, and the highest level a look carries.
const TITLE_SETTING := "look_title"
const TOP_LEVEL := 99

## The settings that hold each choice, with the choices and their names.
const SETTINGS := {
	"look_cheese": [CHEESES, CHEESE_NAMES, "Cheese"],
	"look_hat": [HATS, HAT_NAMES, "Hat"],
	"look_held": [HELD, HELD_NAMES, "Holding"],
	"look_title": [TITLES, TITLE_NAMES, "Title"],
}


## The look with these choices, as a number. Anything unknown is the first
## of its kind.
static func pack(cheese: String, hat: String, held: String, title := "none", level := 0) -> int:
	return maxi(CHEESES.find(cheese), 0) | maxi(HATS.find(hat), 0) << 4 | maxi(HELD.find(held), 0) << 8 | maxi(TITLES.find(title), 0) << 12 | clampi(level, 0, TOP_LEVEL) << 18


## The choices in `look`, as {cheese, hat, held}. Anything out of range is
## the first of its kind.
static func unpack(look: int) -> Dictionary:
	var cheese := look & 15
	var hat := look >> 4 & 15
	var held := look >> 8 & 15
	var title := look >> 12 & 63
	return {
		"title": TITLES[title] if title < TITLES.size() else TITLES[0],
		"level": clampi(look >> 18 & 127, 0, TOP_LEVEL),
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
	return pack(choices.cheese, choices.hat, choices.held, choices.title, choices.level)


## The look this player has chosen in their settings.
static func chosen() -> int:
	return pack(Settings.get_value("look_cheese"), Settings.get_value("look_hat"), Settings.get_value("look_held"), Settings.get_value(TITLE_SETTING), Progress.level())


## What is written under a player's name for `look`: their level and, if
## they have chosen one, their title. "" for a look with neither, as a bot's.
static func rank_text(look: int) -> String:
	var choices := unpack(look)
	if choices.level <= 0:
		return ""
	return "Level %d" % choices.level if choices.title == TITLES[0] else "%s  ·  Level %d" % [TITLE_NAMES[choices.title], choices.level]
