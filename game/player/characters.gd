class_name Characters
## Builds the body players are seen as: the realistic soldier where its
## licensed model is to hand (see soldier_character.gd), and the drawn one
## everywhere else.

const DRAWN := preload("res://player/cheese_character.gd")
const REALISTIC := preload("res://player/soldier_character.gd")

## Whether to show the realistic soldier wherever it can be. The tests turn
## it off (test/pre_run.gd): most are about what players do rather than how
## they look, and the drawn soldier is far quicker to build. Those about the
## realistic one build it themselves.
static var realistic := true


## Whether bodies made now are the realistic soldier.
static func shows_realistic() -> bool:
	return realistic and REALISTIC.available()


## A new body, of whichever kind can be shown here.
static func make() -> Node3D:
	return REALISTIC.new() if shows_realistic() else DRAWN.new()


## Puts a new body in place of `drawn`, the drawn one a scene was built
## with, if the realistic one can be shown here; returns the body to use.
static func swap(drawn: Node3D) -> Node3D:
	if not shows_realistic():
		return drawn
	var body := make()
	body.name = drawn.name
	body.transform = drawn.transform
	drawn.replace_by(body)
	drawn.free()
	return body
