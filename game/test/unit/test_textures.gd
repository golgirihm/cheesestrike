extends GutTest
## What surfaces are made of, to look at.

const WORLD := preload("res://world/world.tscn")
## The most the game's own files may come to, which is what a player
## downloads: textures made in code add nothing to it.
const BUDGET := 3 * 1024 * 1024
## The most any one picture or sound file in the project may be.
const BIGGEST_ASSET := 256 * 1024


func test_every_kind_has_a_picture_that_darkens_gently_and_repeats() -> void:
	for kind: String in Textures.KINDS:
		assert_true(Textures.SPAN.has(kind), kind)
		assert_true(Textures.ROUGH.has(kind), kind)
		var picture := Textures.picture(kind)
		assert_eq(picture.get_width(), Textures.SIZE, kind)
		assert_eq(picture.get_height(), Textures.SIZE, kind)
		assert_same(Textures.picture(kind), picture, "%s is made once" % kind)
		var lightest := 0.0
		var darkest := 1.0
		var total := 0.0
		for y in Textures.SIZE:
			for x in Textures.SIZE:
				var shade := Textures.shade_at(kind, x, y)
				lightest = maxf(lightest, shade)
				darkest = minf(darkest, shade)
				total += shade
				# The same at the far edge as just before the near one.
				assert_eq(Textures.shade_at(kind, x + Textures.SIZE, y), shade, "%s repeats across" % kind) if x == 0 or y == 0 else null
		assert_gt(lightest, 0.9, "%s leaves most of a colour alone" % kind)
		assert_gt(darkest, 0.5, "%s is never near black" % kind)
		assert_gt(lightest - darkest, 0.04, "%s isn't flat" % kind)
		assert_gt(total / (Textures.SIZE * Textures.SIZE), 0.78, "%s keeps a thing its colour" % kind)


func test_no_two_kinds_look_the_same() -> void:
	var seen := {}
	for kind: String in Textures.KINDS:
		var row := []
		for x in 24:
			row.append(snappedf(Textures.shade_at(kind, x * 2, x + 5), 0.01))
		seen[row] = kind
	assert_eq(seen.size(), Textures.KINDS.size())


func test_a_material_keeps_its_colour_and_is_made_once() -> void:
	var red := Textures.material(Color(0.8, 0.2, 0.2), Textures.WOOD)
	assert_eq(red.albedo_color, Color(0.8, 0.2, 0.2))
	assert_same(red.albedo_texture, Textures.picture(Textures.WOOD))
	assert_true(red.uv1_triplanar, "laid on by where it is, not by how the shape was made")
	assert_true(red.uv1_world_triplanar)
	assert_almost_eq(red.uv1_scale.x, 1.0 / Textures.SPAN[Textures.WOOD], 0.0001)
	assert_same(Textures.material(Color(0.8, 0.2, 0.2), Textures.WOOD), red)
	assert_ne(Textures.material(Color(0.8, 0.2, 0.2), Textures.ROCK), red)
	assert_eq(Textures.material(Color.WHITE, "moon dust").get_meta("surface"), Textures.PLASTER, "an unknown kind is plaster")


func test_what_a_piece_is_made_of_goes_by_its_name_or_what_it_says() -> void:
	var piece := Node3D.new()
	autofree(piece)
	for named: Array in [["Block3", Textures.CHEESE], ["MesaNorth", Textures.ROCK], ["RampNorthEast", Textures.WOOD], ["TankSouth", Textures.METAL], ["Loaf", Textures.BREAD], ["Whatever", Textures.PLASTER]]:
		piece.name = named[0]
		assert_eq(Textures.kind_for(piece, "meadow"), named[1], named[0])
	piece.name = "Floor"
	assert_eq(Textures.kind_for(piece, "meadow"), Textures.DIRT)
	assert_eq(Textures.kind_for(piece, "city"), Textures.ASPHALT)
	piece.set_meta("surface", Textures.COBBLE)
	assert_eq(Textures.kind_for(piece, "city"), Textures.COBBLE, "what it says for itself comes first")
	piece.set_meta("surface", "moon dust")
	assert_eq(Textures.kind_for(piece, "city"), Textures.ASPHALT, "unless that is nothing known")


func test_every_solid_piece_of_every_arena_is_made_of_something() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var floors := {}
	for id: String in Maps.ORDER + [Maps.RANGE]:
		world.load_map(id)
		for body: Node3D in world.map.get_node("Geometry").get_children():
			var drawn := body.get_node_or_null("MeshInstance3D") as MeshInstance3D
			if drawn == null:
				continue
			var material := drawn.material_override as StandardMaterial3D
			assert_not_null(material, "%s: %s" % [id, body.name])
			if String(body.name).begins_with("Building"):
				continue
			assert_not_null(material.albedo_texture, "%s: %s is a flat colour" % [id, body.name])
			assert_true(Textures.KINDS.has(String(material.get_meta("surface", ""))), "%s: %s" % [id, body.name])
			if body.name == "Floor":
				floors[id] = material.get_meta("surface")
		for block: Node3D in world.cheese_blocks():
			var cheese := block.get_node("MeshInstance3D").material_override as StandardMaterial3D
			assert_eq(cheese.get_meta("surface"), Textures.CHEESE)
			assert_true(Scenery.is_yellow(cheese.albedo_color), "%s: cheese is still yellow" % id)
	assert_gte(floors.values().reduce(func(seen: Dictionary, kind: String) -> Dictionary:
		seen[kind] = true
		return seen, {}).size(), 5, "the arenas' floors aren't all alike")


func test_shots_still_knock_dust_of_a_things_own_colour_off_it() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	world.load_map("city")
	var geometry: Node = world.map.get_node("Geometry")
	assert_true(Scenery.is_yellow(Surfaces.dust(geometry.get_node("Block1"))))
	assert_ne(Surfaces.dust(geometry.get_node("Floor")), Surfaces.DUST, "the floor's colour, though it has a picture on it")
	assert_eq(Surfaces.dust(geometry.get_node("Building1")), Surfaces.DUST)


func test_the_game_is_small_enough_to_download() -> void:
	var total := 0
	var biggest := 0
	var biggest_name := ""
	var folders := ["res://"]
	while not folders.is_empty():
		var folder: String = folders.pop_back()
		var dir := DirAccess.open(folder)
		if dir == null:
			continue
		for sub in dir.get_directories():
			# Not what only tests and the editor use.
			if not sub.begins_with(".") and not ["addons", "test"].has(sub):
				folders.append(folder.path_join(sub))
		for file in dir.get_files():
			var path := folder.path_join(file)
			var size := FileAccess.get_file_as_bytes(path).size()
			total += size
			if ["png", "jpg", "jpeg", "webp", "svg", "wav", "ogg", "mp3", "glb", "gltf"].has(file.get_extension().to_lower()) and size > biggest:
				biggest = size
				biggest_name = path
	gut.p("the game's own files come to %d KB" % (total / 1024))
	assert_gt(total, 100 * 1024, "the count found the game")
	assert_lt(total, BUDGET)
	assert_lt(biggest, BIGGEST_ASSET, biggest_name)
