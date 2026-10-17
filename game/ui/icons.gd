class_name Icons
## Small pictures for the interface: one for each weapon, each pickup and
## the things a player does. Each is a few flat shapes, drawn here in code
## into a picture the first time it is asked for, so that the game needs no
## picture files and an icon can be had at any size and in any colour.
##
## A shape is a ring of points in a square from 0 to 1, x across and y down.
## Where two shapes of an icon overlap there is a hole, which is how a ring
## or a cut-out is made.

## How big an icon is drawn unless asked otherwise, in pixels.
const SIZE := 48
## The things a player does that have an icon.
const ACTIONS := ["play", "practice", "learn", "customise", "edit", "settings", "credits", "back", "talk", "mute", "ping", "chat"]
## The medals that have one (see Medals).
const MEDALS := ["medal_double", "medal_triple", "medal_streak", "medal_long", "medal_head", "medal_comeback", "medal_revenge"]
## The pickups that have one.
const PICKUPS := ["health", "ammo", "armour", "power", "grenade", "knife"]

static var _made := {}


## Every icon there is, by name.
static func names() -> Array:
	return Weapons.ids() + PICKUPS + ACTIONS + MEDALS


## Whether there is an icon called `name`.
static func has(name: String) -> bool:
	return not _shapes(name).is_empty()


## The icon called `name`, `size` pixels across in `colour`, or null if
## there is none.
static func texture(name: String, size := SIZE, colour := Color.WHITE) -> ImageTexture:
	var key := [name, size, colour]
	if _made.has(key):
		return _made[key]
	var shapes := _shapes(name)
	if shapes.is_empty():
		return null
	var image := Image.create(size, size, false, Image.FORMAT_RGBA8)
	# Each pixel is looked at in nine places, so that edges are smooth.
	for y in size:
		for x in size:
			var covered := 0
			for sub_y in 3:
				for sub_x in 3:
					var at := Vector2((x + (sub_x + 0.5) / 3.0) / size, (y + (sub_y + 0.5) / 3.0) / size)
					covered += 1 if covers(shapes, at) else 0
			image.set_pixel(x, y, Color(colour, colour.a * covered / 9.0))
	_made[key] = ImageTexture.create_from_image(image)
	return _made[key]


## Whether the point `at` is inside the icon made of `shapes`: inside an odd
## number of them.
static func covers(shapes: Array, at: Vector2) -> bool:
	var inside := 0
	for shape: PackedVector2Array in shapes:
		inside += 1 if Geometry2D.is_point_in_polygon(at, shape) else 0
	return inside % 2 == 1


## How much of the square the icon called `name` fills, from 0 to 1.
static func cover(name: String) -> float:
	var shapes := _shapes(name)
	var covered := 0
	for y in 24:
		for x in 24:
			covered += 1 if covers(shapes, Vector2((x + 0.5) / 24.0, (y + 0.5) / 24.0)) else 0
	return covered / 576.0


static func _rect(x: float, y: float, wide: float, tall: float) -> PackedVector2Array:
	return PackedVector2Array([Vector2(x, y), Vector2(x + wide, y), Vector2(x + wide, y + tall), Vector2(x, y + tall)])


static func _round(x: float, y: float, radius: float, sides := 20) -> PackedVector2Array:
	var points := PackedVector2Array()
	for index in sides:
		points.append(Vector2(x, y) + Vector2.from_angle(TAU * index / sides) * radius)
	return points


static func _points(flat: Array) -> PackedVector2Array:
	var points := PackedVector2Array()
	for index in range(0, flat.size(), 2):
		points.append(Vector2(flat[index], flat[index + 1]))
	return points


# A gun seen from the side, pointing right: a barrel, a body, a grip, and
# for some a stock, a magazine and something on top.
static func _gun(barrel: float, thick: float, stock: bool, magazine: float, top := 0.0) -> Array:
	var shapes := [_rect(0.3, 0.42, barrel, thick * 0.6), _rect(0.16, 0.4, 0.34, thick), _points([0.2, 0.4 + thick, 0.34, 0.4 + thick, 0.3, 0.78, 0.16, 0.78])]
	if stock:
		shapes.append(_points([0.02, 0.4, 0.17, 0.4, 0.17, 0.4 + thick, 0.02, 0.62]))
	if magazine > 0.0:
		shapes.append(_rect(0.38, 0.41 + thick, 0.1, magazine))
	if top > 0.0:
		shapes.append(_rect(0.24, 0.4 - top, 0.22, top - 0.02))
	return shapes


static func _shapes(name: String) -> Array:
	match name:
		"pistol":
			return _gun(0.4, 0.16, false, 0.0)
		"pistols":
			return [_rect(0.08, 0.2, 0.5, 0.11), _points([0.1, 0.31, 0.24, 0.31, 0.2, 0.52, 0.08, 0.52]),
				_rect(0.4, 0.52, 0.5, 0.11), _points([0.42, 0.63, 0.56, 0.63, 0.52, 0.86, 0.4, 0.86])]
		"burratas":
			# The pair, each with the round of the cheese at the back.
			return [_rect(0.14, 0.2, 0.44, 0.11), _rect(0.06, 0.17, 0.1, 0.17), _points([0.1, 0.31, 0.24, 0.31, 0.2, 0.52, 0.08, 0.52]),
				_rect(0.46, 0.52, 0.44, 0.11), _rect(0.38, 0.49, 0.1, 0.17), _points([0.42, 0.63, 0.56, 0.63, 0.52, 0.86, 0.4, 0.86])]
		"blaster":
			return _gun(0.62, 0.17, true, 0.2)
		"carbine":
			return _gun(0.5, 0.16, true, 0.14, 0.08)
		"shotgun":
			return _gun(0.68, 0.14, true, 0.0) + [_rect(0.5, 0.56, 0.26, 0.07)]
		"grater":
			return _gun(0.44, 0.2, false, 0.3) + [_rect(0.74, 0.39, 0.1, 0.18)]
		"rifle":
			return _gun(0.7, 0.13, true, 0.0, 0.12)
		"lmg":
			return _gun(0.66, 0.2, true, 0.0) + [_rect(0.36, 0.6, 0.22, 0.2), _points([0.8, 0.54, 0.86, 0.54, 0.92, 0.8, 0.86, 0.8])]
		"launcher":
			return [_rect(0.06, 0.34, 0.86, 0.22), _rect(0.84, 0.28, 0.1, 0.34), _points([0.3, 0.56, 0.44, 0.56, 0.4, 0.84, 0.28, 0.84]), _rect(0.5, 0.56, 0.08, 0.12)]
		"bow":
			return [_points([0.3, 0.06, 0.4, 0.06, 0.72, 0.5, 0.4, 0.94, 0.3, 0.94, 0.6, 0.5]), _rect(0.3, 0.06, 0.03, 0.88), _rect(0.12, 0.48, 0.74, 0.04),
				_points([0.84, 0.42, 0.96, 0.5, 0.84, 0.58])]
		"grenade":
			return [_round(0.5, 0.6, 0.3), _rect(0.42, 0.16, 0.16, 0.16), _points([0.58, 0.16, 0.8, 0.16, 0.8, 0.4, 0.74, 0.4, 0.74, 0.22, 0.58, 0.22])]
		"knife":
			return [_points([0.38, 0.5, 0.92, 0.3, 0.96, 0.36, 0.44, 0.62]), _points([0.06, 0.74, 0.34, 0.5, 0.44, 0.62, 0.16, 0.86]), _rect(0.3, 0.44, 0.06, 0.26)]
		"health":
			# A wedge of cheese with a cross on it.
			return [_points([0.08, 0.78, 0.92, 0.78, 0.92, 0.36, 0.5, 0.2]), _rect(0.56, 0.4, 0.08, 0.3), _rect(0.45, 0.51, 0.3, 0.08)]
		"ammo":
			return [_points([0.14, 0.9, 0.14, 0.4, 0.22, 0.14, 0.3, 0.4, 0.3, 0.9]), _points([0.42, 0.9, 0.42, 0.4, 0.5, 0.14, 0.58, 0.4, 0.58, 0.9]),
				_points([0.7, 0.9, 0.7, 0.4, 0.78, 0.14, 0.86, 0.4, 0.86, 0.9])]
		"armour":
			return [_points([0.5, 0.06, 0.88, 0.2, 0.84, 0.6, 0.5, 0.94, 0.16, 0.6, 0.12, 0.2]), _points([0.5, 0.2, 0.74, 0.29, 0.71, 0.55, 0.5, 0.78, 0.29, 0.55, 0.26, 0.29])]
		"power":
			return [_points([0.58, 0.04, 0.2, 0.56, 0.46, 0.56, 0.38, 0.96, 0.8, 0.42, 0.54, 0.42])]
		"play":
			return [_points([0.24, 0.12, 0.86, 0.5, 0.24, 0.88])]
		"practice":
			return [_round(0.5, 0.5, 0.44), _round(0.5, 0.5, 0.32), _round(0.5, 0.5, 0.2), _round(0.5, 0.5, 0.08)]
		"learn":
			# An open book.
			return [_points([0.06, 0.22, 0.46, 0.3, 0.46, 0.86, 0.06, 0.78]), _points([0.54, 0.3, 0.94, 0.22, 0.94, 0.78, 0.54, 0.86])]
		"customise":
			# A hat.
			return [_points([0.3, 0.6, 0.34, 0.2, 0.66, 0.2, 0.7, 0.6]), _points([0.06, 0.6, 0.94, 0.6, 0.86, 0.76, 0.14, 0.76])]
		"edit":
			# Blocks stacked.
			return [_rect(0.08, 0.54, 0.38, 0.38), _rect(0.54, 0.54, 0.38, 0.38), _rect(0.3, 0.1, 0.38, 0.38)]
		"settings":
			# Three sliders.
			return [_rect(0.08, 0.2, 0.84, 0.06), _rect(0.08, 0.47, 0.84, 0.06), _rect(0.08, 0.74, 0.84, 0.06),
				_round(0.3, 0.23, 0.1), _round(0.68, 0.5, 0.1), _round(0.42, 0.77, 0.1)]
		"credits":
			# A star.
			var star := PackedVector2Array()
			for point in 10:
				star.append(Vector2(0.5, 0.52) + Vector2.from_angle(TAU * point / 10.0 - PI / 2.0) * (0.46 if point % 2 == 0 else 0.2))
			return [star]
		"back":
			return [_points([0.08, 0.5, 0.5, 0.12, 0.5, 0.36, 0.92, 0.36, 0.92, 0.64, 0.5, 0.64, 0.5, 0.88])]
		"talk":
			return [_points([0.08, 0.38, 0.28, 0.38, 0.52, 0.16, 0.52, 0.84, 0.28, 0.62, 0.08, 0.62]), _points([0.62, 0.3, 0.7, 0.3, 0.78, 0.5, 0.7, 0.7, 0.62, 0.7, 0.69, 0.5]),
				_points([0.78, 0.18, 0.86, 0.18, 0.96, 0.5, 0.86, 0.82, 0.78, 0.82, 0.87, 0.5])]
		"mute":
			return [_points([0.08, 0.38, 0.28, 0.38, 0.52, 0.16, 0.52, 0.84, 0.28, 0.62, 0.08, 0.62]), _points([0.6, 0.32, 0.67, 0.25, 0.95, 0.63, 0.88, 0.7]),
				_points([0.88, 0.25, 0.95, 0.32, 0.67, 0.7, 0.6, 0.63])]
		"ping":
			# A pin dropped on a spot.
			return [_points([0.5, 0.96, 0.22, 0.44, 0.3, 0.18, 0.5, 0.06, 0.7, 0.18, 0.78, 0.44]), _round(0.5, 0.34, 0.12)]
		"medal_double":
			return [_round(0.34, 0.5, 0.26), _round(0.66, 0.5, 0.26)]
		"medal_triple":
			return [_round(0.5, 0.3, 0.22), _round(0.26, 0.68, 0.22), _round(0.74, 0.68, 0.22)]
		"medal_streak":
			# A flame.
			return [_points([0.5, 0.04, 0.72, 0.36, 0.84, 0.62, 0.72, 0.86, 0.5, 0.96, 0.28, 0.86, 0.16, 0.62, 0.32, 0.5, 0.38, 0.28]), _points([0.5, 0.52, 0.62, 0.7, 0.5, 0.86, 0.38, 0.7])]
		"medal_long":
			# A sight: a ring with a cross through it.
			return [_round(0.5, 0.5, 0.4), _round(0.5, 0.5, 0.3), _rect(0.47, 0.04, 0.06, 0.92), _rect(0.04, 0.47, 0.92, 0.06)]
		"medal_head":
			# A wedge with a spot between its eyes.
			return [_points([0.14, 0.9, 0.86, 0.9, 0.7, 0.1, 0.3, 0.1]), _round(0.5, 0.4, 0.09), _round(0.34, 0.56, 0.05), _round(0.66, 0.56, 0.05)]
		"medal_comeback":
			return [_points([0.5, 0.06, 0.9, 0.5, 0.66, 0.5, 0.66, 0.94, 0.34, 0.94, 0.34, 0.5, 0.1, 0.5])]
		"medal_revenge":
			# Two blades crossed.
			return [_points([0.1, 0.16, 0.22, 0.1, 0.9, 0.8, 0.82, 0.9]), _points([0.9, 0.16, 0.78, 0.1, 0.1, 0.8, 0.18, 0.9])]
		"chat":
			return [_points([0.08, 0.14, 0.92, 0.14, 0.92, 0.68, 0.44, 0.68, 0.22, 0.9, 0.24, 0.68, 0.08, 0.68])]
	return []
