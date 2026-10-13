class_name ArenaCode
## An arena a player has laid out themselves, as a short code that can be
## kept, hosted and shared in a link, and the building of an arena from one.
##
## An arena is a square floor of one of a few sizes with pieces on it, each
## on a grid: blocks of cheese (which stack two high), walls, crates, pads
## holding a pickup, and jump pads. Everything else an arena must have is
## worked out from those: where sixteen players start, round the edge and
## clear of the pieces; the places the modes need; and names for its parts.
##
## A code is a few bytes for each piece, written in letters and digits that
## are safe in a link. Reading one checks every part of it, as a code may
## come from anyone.

const VERSION := 1
## The floor sizes to choose from, in metres along each side.
const SIZES := [32, 44, 56]
## The pieces, by the number that stands for each in a code.
const BLOCK := 0
const WALL := 1
const CRATE := 2
const HEALTH := 3
const AMMO := 4
const ARMOUR := 5
const POWER := 6
const LAUNCHER := 7
const JUMP := 8
const KINDS := [BLOCK, WALL, CRATE, HEALTH, AMMO, ARMOUR, POWER, LAUNCHER, JUMP]
const NAMES := {BLOCK: "Cheese block", WALL: "Wall", CRATE: "Crate", HEALTH: "Health wedge", AMMO: "Ammunition", ARMOUR: "Rind armour",
	POWER: "Power-up", LAUNCHER: "Gouda Launcher", JUMP: "Jump pad"}
## What a pad of each kind holds (see Pickups).
const PICKUPS := {HEALTH: "health", AMMO: "ammo", ARMOUR: "armour", POWER: "power", LAUNCHER: "launcher"}
## How far across (x, z) and how tall each solid piece is, before turning.
const SOLID := {BLOCK: Vector3(2, 2, 2), WALL: Vector3(6, 4, 1), CRATE: Vector3(3, 3, 3)}
## Metres between the lines of the grid pieces sit on.
const GRID := 1.0
## The most pieces an arena can have, and the most of some kinds.
const MAX_PIECES := 120
const MOST := {POWER: 1, LAUNCHER: 2, ARMOUR: 2, JUMP: 4}
## How fast a jump pad throws, up and along the way it faces.
const JUMP_UP := 13.0
const JUMP_ALONG := 9.0
## The longest code that will be read.
const MAX_LENGTH := 700
## The query parameter of a link that holds an arena's code.
const PARAM := "arena"


## A new arena with nothing on it: {"size": metres across, "pieces": []}.
## Each piece is {"kind", "x", "z", "turn": quarter turns, "level": 0 on
## the ground or 1 on top of a block}.
static func blank(size := 44) -> Dictionary:
	return {"size": size if SIZES.has(size) else 44, "pieces": []}


## The space a solid piece fills, or one the size of a pad for the rest.
static func footprint(piece: Dictionary) -> Rect2:
	var size: Vector3 = SOLID.get(piece.kind, Vector3(1.6, 0.2, 1.6))
	var across := Vector2(size.x, size.z) if piece.turn % 2 == 0 else Vector2(size.z, size.x)
	return Rect2(Vector2(piece.x, piece.z) - across / 2.0, across)


## Whether `piece` can be put into `arena`: a known kind, on the floor and
## clear of its edge, not through anything already there, and not one too
## many. A block on the second level needs a block under it.
static func can_place(arena: Dictionary, piece: Dictionary) -> bool:
	if not KINDS.has(piece.get("kind")) or arena.pieces.size() >= MAX_PIECES:
		return false
	var half: float = arena.size / 2.0
	var room := footprint(piece)
	if room.position.x < -half + 0.5 or room.position.y < -half + 0.5 or room.end.x > half - 0.5 or room.end.y > half - 0.5:
		return false
	if MOST.has(piece.kind) and arena.pieces.filter(func(other: Dictionary) -> bool: return other.kind == piece.kind).size() >= MOST[piece.kind]:
		return false
	# Nothing goes through anything on its own level, and only a block can
	# be on the second, exactly on top of a block on the first.
	var rests := false
	for other: Dictionary in arena.pieces:
		if other.level == piece.level and footprint(other).grow(-0.05).intersects(room):
			return false
		if other.kind == BLOCK and other.level == 0 and other.x == piece.x and other.z == piece.z:
			rests = true
	return piece.level == 0 or (piece.kind == BLOCK and rests)


## Puts `piece` into `arena` if it can go there. Returns whether it did.
static func place(arena: Dictionary, piece: Dictionary) -> bool:
	var whole := {"kind": piece.get("kind"), "x": float(piece.get("x", 0)), "z": float(piece.get("z", 0)), "turn": posmod(int(piece.get("turn", 0)), 4), "level": clampi(int(piece.get("level", 0)), 0, 1)}
	whole.x = snappedf(whole.x, GRID)
	whole.z = snappedf(whole.z, GRID)
	if not can_place(arena, whole):
		return false
	arena.pieces.append(whole)
	return true


## Takes the piece at (x, z) out of `arena`: the topmost there. Returns
## whether there was one.
static func remove_at(arena: Dictionary, x: float, z: float) -> bool:
	var found := -1
	for index in arena.pieces.size():
		var piece: Dictionary = arena.pieces[index]
		if footprint(piece).has_point(Vector2(x, z)) and (found == -1 or piece.level > arena.pieces[found].level):
			found = index
	if found == -1:
		return false
	arena.pieces.remove_at(found)
	return true


## `arena` as a code.
static func encode(arena: Dictionary) -> String:
	var bytes := PackedByteArray([VERSION, maxi(SIZES.find(arena.size), 0), mini(arena.pieces.size(), MAX_PIECES)])
	for piece: Dictionary in arena.pieces.slice(0, MAX_PIECES):
		bytes.append(piece.kind | (piece.turn << 4) | (piece.level << 6))
		bytes.append(int(piece.x) + 128)
		bytes.append(int(piece.z) + 128)
	# Letters and digits that pass through a link unchanged.
	return Marshalls.raw_to_base64(bytes).replace("+", "-").replace("/", "_").replace("=", "")


## The arena `code` describes, with anything that can't be placed left out;
## {} if it isn't a code at all.
static func decode(code: Variant) -> Dictionary:
	if not code is String or code.is_empty() or code.length() > MAX_LENGTH:
		return {}
	for character: String in code:
		if not (character.is_valid_int() or (character >= "a" and character <= "z") or (character >= "A" and character <= "Z") or character == "-" or character == "_"):
			return {}
	# No whole number of bytes is written in one letter over a multiple of four.
	if code.length() % 4 == 1:
		return {}
	var padded: String = code.replace("-", "+").replace("_", "/")
	while padded.length() % 4 != 0:
		padded += "="
	var bytes := Marshalls.base64_to_raw(padded)
	if bytes.size() < 3 or bytes[0] != VERSION or bytes[1] >= SIZES.size():
		return {}
	var arena := blank(SIZES[bytes[1]])
	var count := mini(bytes[2], (bytes.size() - 3) / 3)
	for index in count:
		var at := 3 + index * 3
		place(arena, {"kind": bytes[at] & 15, "turn": (bytes[at] >> 4) & 3, "level": (bytes[at] >> 6) & 1, "x": bytes[at + 1] - 128, "z": bytes[at + 2] - 128})
	return arena


## The link that gives the arena `code` to whoever opens it, from the page
## at `page_url`: its other parameters are kept, but not a session to join.
static func link(page_url: String, code: String) -> String:
	var address := page_url.get_slice("#", 0)
	var kept: Array[String] = []
	if address.contains("?"):
		for part in address.get_slice("?", 1).split("&", false):
			if part.get_slice("=", 0) != PARAM and part.get_slice("=", 0) != Invite.PARAM:
				kept.append(part)
	kept.append("%s=%s" % [PARAM, code])
	return "%s?%s" % [address.get_slice("?", 0), "&".join(kept)]


## The arena code in a page's query string, or "" if there is none or it
## isn't one.
static func code_in(query: String) -> String:
	for part in query.trim_prefix("?").split("&", false):
		if part.get_slice("=", 0) == PARAM and part.contains("="):
			var code := part.get_slice("=", 1).uri_decode()
			return code if not decode(code).is_empty() else ""
	return ""


## A page's query string without its arena code.
static func without_code(query: String) -> String:
	var kept: Array[String] = []
	for part in query.trim_prefix("?").split("&", false):
		if part.get_slice("=", 0) != PARAM:
			kept.append(part)
	return ("?" + "&".join(kept)) if not kept.is_empty() else ""


## Whether (x, z) is clear of every piece of `arena` by `margin`.
static func clear_at(arena: Dictionary, x: float, z: float, margin: float) -> bool:
	for piece: Dictionary in arena.pieces:
		if footprint(piece).grow(margin).has_point(Vector2(x, z)):
			return false
	return true


## Where sixteen players start: spread round the arena just inside its
## edge, each clear of the pieces, the first two in opposite corners.
static func spawns(arena: Dictionary) -> Array[Vector3]:
	var reach: float = arena.size / 2.0 - 3.0
	var wanted := MatchSettings.MAX_PLAYERS
	var found: Array[Vector3] = []
	# Rings in from the edge, until there are enough.
	var ring := reach
	while found.size() < wanted and ring > 1.0:
		# A multiple of four, so that each spot has one straight across.
		var round := int(ring * 8.0 / 3.2 / 4.0) * 4
		for step in round:
			# From one corner, with the opposite corner next.
			var along := (step / 2 if step % 2 == 0 else round / 2 + step / 2) % round
			var at := _on_square(ring, float(along) / round)
			var apart := found.all(func(other: Vector3) -> bool: return other.distance_to(at) > BodyShape.RADIUS * 2.0 + 0.6)
			if found.size() < wanted and apart and clear_at(arena, at.x, at.z, BodyShape.RADIUS + 0.3):
				found.append(at)
		ring -= 3.2
	return found


# The point a share `along` of the way round a square `reach` from the
# middle, starting from its north-west corner.
static func _on_square(reach: float, along: float) -> Vector3:
	var side := int(along * 4.0) % 4
	var part := fmod(along * 4.0, 1.0) * 2.0 * reach - reach
	match side:
		0:
			return Vector3(part, 0.1, -reach)
		1:
			return Vector3(reach, 0.1, part)
		2:
			return Vector3(-part, 0.1, reach)
	return Vector3(-reach, 0.1, -part)


## The spot nearest to (x, z) that is clear of the pieces, for something
## the arena must have somewhere about there.
static func free_near(arena: Dictionary, x: float, z: float) -> Vector3:
	var reach: float = arena.size / 2.0 - 3.0
	for out in range(0, int(arena.size)):
		for turn in maxi(out * 4, 1):
			var angle := TAU * turn / maxi(out * 4, 1)
			var at := Vector3(clampf(x + cos(angle) * out, -reach, reach), 0.0, clampf(z + sin(angle) * out, -reach, reach))
			if clear_at(arena, at.x, at.z, BodyShape.RADIUS + 0.3):
				return at
	return Vector3(x, 0, z)


## Builds the arena as a scene like any other: its floor, edge and pieces,
## where players start, its pads, and the places the modes and names need.
static func build(arena: Dictionary) -> Node3D:
	var map := Node3D.new()
	map.name = "Custom"
	map.set_meta("scenery", "meadow")
	var geometry := Node3D.new()
	geometry.name = "Geometry"
	map.add_child(geometry)
	var size: float = arena.size
	geometry.add_child(_box("Floor", Vector3(0, -0.5, 0), Vector3(size, 1, size), Color(0.45, 0.31, 0.22)))
	geometry.add_child(_box("WallNorth", Vector3(0, 999, -size / 2.0 - 2.0), Vector3(size + 8.0, 2000, 4), Color.TRANSPARENT))
	geometry.add_child(_box("WallSouth", Vector3(0, 999, size / 2.0 + 2.0), Vector3(size + 8.0, 2000, 4), Color.TRANSPARENT))
	geometry.add_child(_box("WallEast", Vector3(size / 2.0 + 2.0, 999, 0), Vector3(4, 2000, size), Color.TRANSPARENT))
	geometry.add_child(_box("WallWest", Vector3(-size / 2.0 - 2.0, 999, 0), Vector3(4, 2000, size), Color.TRANSPARENT))
	var pickups := _holder(map, "Pickups")
	var jumps := _holder(map, "JumpPads")
	var counts := {}
	for piece: Dictionary in arena.pieces:
		counts[piece.kind] = counts.get(piece.kind, 0) + 1
		var number: int = counts[piece.kind]
		if SOLID.has(piece.kind):
			var solid: Vector3 = SOLID[piece.kind]
			var turned := solid if piece.turn % 2 == 0 else Vector3(solid.z, solid.y, solid.x)
			var name: String = {BLOCK: "Block", WALL: "CoverWall", CRATE: "Crate"}[piece.kind]
			var colour: Color = {BLOCK: Color(1, 0.82, 0.22), WALL: Color(0.72, 0.5, 0.3), CRATE: Color(0.62, 0.44, 0.28)}[piece.kind]
			geometry.add_child(_box("%s%d" % [name, number], Vector3(piece.x, solid.y / 2.0 + piece.level * 2.0, piece.z), turned, colour))
		elif piece.kind == JUMP:
			var way := Vector3(0, 0, -1).rotated(Vector3.UP, -piece.turn * PI / 2.0)
			var pad := _marker(jumps, "Pad%d" % number, Vector3(piece.x, 0, piece.z))
			pad.set_meta("launch", Vector3(way.x * JUMP_ALONG, JUMP_UP, way.z * JUMP_ALONG))
		else:
			_marker(pickups, "%s%d" % [String(PICKUPS[piece.kind]).capitalize(), number], Vector3(piece.x, 0, piece.z)).set_meta("kind", PICKUPS[piece.kind])
	var starts := _holder(map, "SpawnPoints")
	var spots := spawns(arena)
	for index in spots.size():
		_marker(starts, "Spawn%d" % (index + 1), spots[index])
	var quarter := size / 4.0
	var objectives := _holder(map, "Objectives")
	_marker(objectives, "BaseCheddar", free_near(arena, -size / 2.0 + 4.0, 0))
	_marker(objectives, "BaseBrie", free_near(arena, size / 2.0 - 4.0, 0))
	var middles := [Vector2.ZERO, Vector2(-quarter, -quarter), Vector2(quarter, quarter), Vector2(-quarter, quarter), Vector2(quarter, -quarter)]
	for index in middles.size():
		_marker(objectives, "Hill%d" % (index + 1), free_near(arena, middles[index].x, middles[index].y))
	for index in 3:
		_marker(objectives, "Point%s" % "ABC"[index], free_near(arena, (index - 1) * quarter, 0))
	var places := _holder(map, "Places")
	var named := {"The Middle": Vector2.ZERO, "North Side": Vector2(0, -quarter * 1.4), "South Side": Vector2(0, quarter * 1.4), "East Side": Vector2(quarter * 1.4, 0), "West Side": Vector2(-quarter * 1.4, 0)}
	for label: String in named:
		_marker(places, "Place%d" % (places.get_child_count() + 1), Vector3(named[label].x, 0, named[label].y)).set_meta("label", label)
	var camera := Camera3D.new()
	camera.name = "MenuCamera"
	camera.transform = Transform3D(Basis.from_euler(Vector3(-0.57, 0, 0)), Vector3(0, size * 0.42, size * 0.62))
	map.add_child(camera)
	return map


static func _holder(map: Node3D, name: String) -> Node3D:
	var holder := Node3D.new()
	holder.name = name
	map.add_child(holder)
	return holder


static func _marker(holder: Node3D, name: String, at: Vector3) -> Marker3D:
	var marker := Marker3D.new()
	marker.name = name
	marker.position = at
	holder.add_child(marker)
	return marker


# A solid box as the arenas' scenes have them: drawn, unless it has no colour.
static func _box(name: String, at: Vector3, size: Vector3, colour: Color) -> StaticBody3D:
	var body := StaticBody3D.new()
	body.name = name
	body.position = at
	if colour.a > 0.0:
		var drawn := MeshInstance3D.new()
		drawn.name = "MeshInstance3D"
		var mesh := BoxMesh.new()
		mesh.size = size
		var material := StandardMaterial3D.new()
		material.albedo_color = colour
		mesh.material = material
		drawn.mesh = mesh
		body.add_child(drawn)
	var shape := CollisionShape3D.new()
	shape.name = "CollisionShape3D"
	var box := BoxShape3D.new()
	box.size = size
	shape.shape = box
	body.add_child(shape)
	return body
