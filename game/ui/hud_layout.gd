class_name HudLayout
## The HUD the player's way: each part of it can be moved, made bigger or
## smaller, faded, or turned off, and there is a minimal one ready-made.
##
## A layout is kept as what differs from the usual: {part: {"x", "y",
## "scale", "opacity", "on"}}, where x and y are how far the part is moved,
## as shares of the screen's width and height. However a part is moved and
## scaled, it is kept wholly on the screen.

const SETTING := "hud_layout"
## The parts, with what each is called.
const PARTS := {
	"health": "Health and Overload", "weapon": "Weapon and ammunition", "score": "Score and time", "objective": "Objective",
	"feed": "Knock-outs", "compass": "Compass", "chat": "Chat", "place": "Where you are", "captions": "Captions",
}
const X := "x"
const Y := "y"
const SCALE := "scale"
const OPACITY := "opacity"
const ON := "on"
## The least and most each number can be.
const RANGES := {X: [-0.5, 0.5], Y: [-0.5, 0.5], SCALE: [0.6, 1.6], OPACITY: [0.2, 1.0]}
## A part as it usually is.
const USUAL := {X: 0.0, Y: 0.0, SCALE: 1.0, OPACITY: 1.0, ON: true}


## The usual layout: nothing differs.
static func standard() -> Dictionary:
	return {}


## A layout with only what is needed to play: health, the weapon and the
## objective, smaller and fainter, and captions for those who have them on.
static func minimal() -> Dictionary:
	var layout := {}
	for part: String in ["score", "feed", "compass", "place", "chat"]:
		layout[part] = {ON: false}
	for part: String in ["health", "weapon"]:
		layout[part] = {SCALE: 0.8, OPACITY: 0.75}
	layout["objective"] = {OPACITY: 0.75}
	return layout


## How `part` is in `layout`: every number and whether it is on, with the
## usual for whatever the layout doesn't say.
static func part(layout: Dictionary, id: String) -> Dictionary:
	var said: Variant = layout.get(id, {})
	var whole := USUAL.duplicate()
	if said is Dictionary:
		for key: String in RANGES:
			var value: Variant = said.get(key)
			if value is float or value is int:
				whole[key] = clampf(float(value), RANGES[key][0], RANGES[key][1])
		if said.get(ON) is bool:
			whole[ON] = said[ON]
	return whole


## `layout` with nothing in it that isn't a layout: unknown parts dropped,
## numbers within bounds, and anything that is as usual left out.
static func tidy(layout: Variant) -> Dictionary:
	var tidied := {}
	if not layout is Dictionary:
		return tidied
	for id: String in PARTS:
		if not layout.has(id):
			continue
		var whole := part(layout, id)
		var differs := {}
		for key: String in whole:
			if whole[key] != USUAL[key]:
				differs[key] = whole[key]
		if not differs.is_empty():
			tidied[id] = differs
	return tidied


## `layout` with `key` of `id` changed to `value`.
static func with(layout: Dictionary, id: String, key: String, value: Variant) -> Dictionary:
	var changed := layout.duplicate(true)
	if not PARTS.has(id):
		return tidy(changed)
	var said: Dictionary = changed.get(id, {})
	said[key] = value
	changed[id] = said
	return tidy(changed)


## How big a part of size `size` is drawn when it is asked to be `scale`
## times its size, on a screen of size `screen`: as asked, unless that is
## bigger than the screen, when it is as big as fits.
static func fitting_scale(size: Vector2, scale: float, screen: Vector2) -> float:
	var most := scale
	if size.x > 0.0:
		most = minf(most, screen.x / size.x)
	if size.y > 0.0:
		most = minf(most, screen.y / size.y)
	return most


## Where a part that is usually at `home` goes on a screen of size `screen`
## when it is as `how` (see part) says: moved, scaled about the corner of the
## screen it is nearest, and then brought back wholly onto the screen.
static func rect_for(home: Rect2, how: Dictionary, screen: Vector2) -> Rect2:
	var size: Vector2 = home.size * fitting_scale(home.size, how[SCALE], screen)
	# Scaled toward the edges it sits by, so that a part in a corner stays
	# in its corner.
	var centre := home.get_center()
	var hold := Vector2(_hold(centre.x, screen.x), _hold(centre.y, screen.y))
	var at := home.position + (home.size - size) * hold + Vector2(how[X] * screen.x, how[Y] * screen.y)
	at.x = clampf(at.x, 0.0, maxf(screen.x - size.x, 0.0))
	at.y = clampf(at.y, 0.0, maxf(screen.y - size.y, 0.0))
	return Rect2(at, size)


# Which side of itself a part scales about, along one way of a screen
# `span` across, when its middle is at `centre`: 0 its near side, 1 its
# far side, a half if it is in the middle.
static func _hold(centre: float, span: float) -> float:
	if absf(centre - span / 2.0) < span * 0.1:
		return 0.5
	return 1.0 if centre > span / 2.0 else 0.0
