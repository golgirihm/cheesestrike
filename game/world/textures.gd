class_name Textures
## What surfaces are made of, to look at: earth, rock, wood grain, asphalt,
## tiles, cobbles, brushed metal, plaster, bread and the cheese itself. Each
## is a small picture of light and shade, made in code the first time it is
## wanted, that repeats without a seam and is laid over a surface's own
## colour. So nothing is added to what the game has to download, and a thing
## keeps the colour it had.
##
## A picture is laid on by where a surface is in the arena, not by how the
## shape was made, so it runs unbroken across things that touch and is the
## same size on a pebble as on a wall.

const DIRT := "dirt"
const ROCK := "rock"
const WOOD := "wood"
const ASPHALT := "asphalt"
const TILE := "tile"
const COBBLE := "cobble"
const METAL := "metal"
const PLASTER := "plaster"
const BREAD := "bread"
const CHEESE := "cheese"
const KINDS := [DIRT, ROCK, WOOD, ASPHALT, TILE, COBBLE, METAL, PLASTER, BREAD, CHEESE]
## Pixels along each side of a picture.
const SIZE := 64
## Metres of surface one copy of each picture covers.
const SPAN := {DIRT: 5.0, ROCK: 6.0, WOOD: 3.0, ASPHALT: 3.0, TILE: 4.0, COBBLE: 4.0, METAL: 4.0, PLASTER: 6.0, BREAD: 2.5, CHEESE: 4.0}
## How rough each is to the light.
const ROUGH := {DIRT: 0.95, ROCK: 0.9, WOOD: 0.75, ASPHALT: 0.9, TILE: 0.45, COBBLE: 0.85, METAL: 0.35, PLASTER: 0.85, BREAD: 0.9, CHEESE: 0.6}
## What the solid pieces of an arena are made of, by how their names begin;
## anything else is plaster. A piece can say otherwise with its own
## `surface`.
const BY_NAME := {
	"Block": CHEESE, "Mesa": ROCK, "Ridge": ROCK, "Pillar": ROCK, "Arch": ROCK, "Ledge": ROCK, "Rock": ROCK, "Fountain": ROCK, "Clock": ROCK,
	"Ramp": WOOD, "Board": WOOD, "Shelf": WOOD, "Crate": WOOD, "Cart": WOOD, "Stall": WOOD, "Step": BREAD, "Loaf": BREAD, "Cover": BREAD,
	"Lobby": PLASTER, "Upper": PLASTER, "Tank": METAL, "Toaster": METAL, "Kettle": METAL, "Gantry": METAL, "Jar": TILE, "Butter": TILE,
}
## What the floor of an arena is made of, by the land round it, unless the
## floor says otherwise.
const FLOORS := {"meadow": DIRT, "canyon": DIRT, "city": ASPHALT}

static var _pictures := {}
static var _materials := {}


## What `body`, a solid piece of an arena whose land is `scenery`, is made of.
static func kind_for(body: Node, scenery: String) -> String:
	var said := String(body.get_meta("surface", ""))
	if KINDS.has(said):
		return said
	var name := String(body.name)
	if name == "Floor":
		return FLOORS.get(scenery, DIRT)
	for start: String in BY_NAME:
		if name.begins_with(start):
			return BY_NAME[start]
	return PLASTER


## A material of `colour` made of `kind`, the same one each time.
static func material(colour: Color, kind: String) -> StandardMaterial3D:
	if not KINDS.has(kind):
		kind = PLASTER
	var key := [colour, kind]
	if not _materials.has(key):
		var made := StandardMaterial3D.new()
		made.albedo_color = colour
		made.albedo_texture = picture(kind)
		made.uv1_triplanar = true
		made.uv1_world_triplanar = true
		made.uv1_scale = Vector3.ONE / SPAN[kind]
		made.roughness = ROUGH[kind]
		made.set_meta("surface", kind)
		_materials[key] = made
	return _materials[key]


## The picture for `kind`: shades from a little under white down, so that
## laid over a colour it darkens it here and there and leaves it otherwise.
static func picture(kind: String) -> ImageTexture:
	if not _pictures.has(kind):
		var image := Image.create(SIZE, SIZE, true, Image.FORMAT_L8)
		for y in SIZE:
			for x in SIZE:
				var shade := clampf(shade_at(kind, x, y), 0.0, 1.0)
				image.set_pixel(x, y, Color(shade, shade, shade))
		image.generate_mipmaps()
		_pictures[kind] = ImageTexture.create_from_image(image)
	return _pictures[kind]


## How light the picture for `kind` is at pixel (x, y): 1 leaves the
## surface's colour alone.
static func shade_at(kind: String, x: int, y: int) -> float:
	match kind:
		DIRT:
			return 0.84 + 0.14 * _cloud(x, y, 8) + 0.04 * _speck(x, y)
		ROCK:
			# Blotches, with a few dark cracks where two of them meet.
			var blotch := _cloud(x, y, 16)
			var crack := absf(_cloud(x + 31, y + 17, 16) - 0.5)
			return 0.8 + 0.18 * blotch - (0.16 if crack < 0.025 else 0.0)
		WOOD:
			# Grain along one way, wandering a little, with darker lines.
			var wander := _cloud(x, y, 16) * 5.0
			var grain := 0.5 + 0.5 * sin((y + wander) * TAU / 8.0)
			var plank := 0.12 if x % 32 == 0 else 0.0
			return 0.82 + 0.14 * grain - plank + 0.03 * _speck(x, y)
		ASPHALT:
			return 0.86 + 0.14 * _speck(x, y) * _speck(y + 7, x + 3)
		TILE:
			# Square tiles with a dark joint, each a hair different.
			var joint := x % 32 == 0 or y % 32 == 0
			return 0.72 if joint else 0.93 + 0.06 * _hash(x / 32, y / 32)
		COBBLE:
			# Rounded stones: light in their middles, dark where they meet.
			var cell := 16
			var best := 9.0
			for cy in range(-1, 2):
				for cx in range(-1, 2):
					var gx := floori(float(x) / cell) + cx
					var gy := floori(float(y) / cell) + cy
					# The stones come round at the picture's edge too.
					var wx := posmod(gx, SIZE / cell)
					var wy := posmod(gy, SIZE / cell)
					var px := (gx + 0.2 + 0.6 * _hash(wx, wy)) * cell
					var py := (gy + 0.2 + 0.6 * _hash(wy + 9, wx + 4)) * cell
					best = minf(best, Vector2(x - px, y - py).length() / cell)
			return 0.98 - 0.3 * smoothstep(0.3, 0.62, best)
		METAL:
			# Brushed: fine streaks along one way.
			return 0.88 + 0.1 * _hash(0, y) + 0.03 * _speck(x, y)
		BREAD:
			var pore := _speck(x, y) * _speck(x + 5, y + 11)
			return 0.9 + 0.08 * _cloud(x, y, 8) - (0.18 if pore > 0.82 else 0.0)
		CHEESE:
			return 0.93 + 0.07 * _cloud(x, y, 16)
	return 0.92 + 0.07 * _cloud(x, y, 16)


# A number from 0 to 1 that is always the same for the same place, and
# comes round at the picture's edge.
static func _hash(x: int, y: int) -> float:
	var n := posmod(x, SIZE) * 374761393 + posmod(y, SIZE) * 668265263
	n = (n ^ (n >> 13)) * 1274126177
	return float((n ^ (n >> 16)) & 0xffff) / 65535.0


static func _speck(x: int, y: int) -> float:
	return _hash(x * 7 + 3, y * 13 + 5)


# Soft blotches `cell` pixels across, from 0 to 1, that come round at the
# picture's edge.
static func _cloud(x: int, y: int, cell: int) -> float:
	var gx := floori(float(x) / cell)
	var gy := floori(float(y) / cell)
	var fx := smoothstep(0.0, 1.0, float(posmod(x, cell)) / cell)
	var fy := smoothstep(0.0, 1.0, float(posmod(y, cell)) / cell)
	var across := SIZE / cell
	var a := _hash(posmod(gx, across) * cell, posmod(gy, across) * cell)
	var b := _hash(posmod(gx + 1, across) * cell, posmod(gy, across) * cell)
	var c := _hash(posmod(gx, across) * cell, posmod(gy + 1, across) * cell)
	var d := _hash(posmod(gx + 1, across) * cell, posmod(gy + 1, across) * cell)
	return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy)
