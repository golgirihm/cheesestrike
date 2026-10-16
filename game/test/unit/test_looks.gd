extends GutTest
## What players choose their characters to look like, and that none of it
## changes what can be hit.

const CHARACTER := preload("res://player/cheese_character.gd")
const WORLD := preload("res://world/world.tscn")

var character: Node3D


func before_each() -> void:
	character = CHARACTER.new()
	add_child_autofree(character)


func after_each() -> void:
	for key: String in Looks.SETTINGS:
		Settings.set_value(key, Looks.SETTINGS[key][0][0])


func _every_look() -> Array:
	var looks := []
	for cheese: String in Looks.CHEESES:
		for hat: String in Looks.HATS:
			for held: String in Looks.HELD:
				looks.append(Looks.pack(cheese, hat, held))
	return looks


func test_a_look_survives_being_packed_into_a_number() -> void:
	for cheese: String in Looks.CHEESES:
		for hat: String in Looks.HATS:
			for held: String in Looks.HELD:
				assert_eq(Looks.unpack(Looks.pack(cheese, hat, held)), {"finish": "plain", "title": "none", "level": 0, "cheese": cheese, "hat": hat, "held": held})
	assert_eq(Looks.pack("swiss", "none", "none"), 0, "the plain look is nothing at all")


func test_every_choice_has_a_name() -> void:
	for key: String in Looks.SETTINGS:
		var choice: Array = Looks.SETTINGS[key]
		for id: String in choice[0]:
			assert_true(choice[1].has(id), "%s: %s" % [key, id])
		assert_true(Settings.CHOICES.has(key), key)
		assert_eq(Settings.CHOICES[key], choice[0])


func test_a_look_from_another_machine_is_put_right() -> void:
	assert_eq(Looks.clean("fancy"), 0)
	assert_eq(Looks.clean(null), 0)
	assert_eq(Looks.clean(3.5), 0)
	assert_eq(Looks.unpack(Looks.clean(0xFFF)), {"finish": "plain", "title": "none", "level": 0, "cheese": "swiss", "hat": "none", "held": "none"}, "choices that don't exist become the plain ones")
	var wild := Looks.unpack(Looks.clean(-1))
	assert_eq([wild.cheese, wild.hat, wild.held, wild.title], ["swiss", "none", "none", "none"])
	assert_lte(wild.level, Looks.TOP_LEVEL, "and a level no higher than there is")
	var real := Looks.pack("blue", "crown", "flag")
	assert_eq(Looks.clean(real), real)
	assert_eq(Looks.clean(real | 1 << 27), real, "anything extra is dropped")
	assert_eq(Looks.unpack(99999).keys(), ["finish", "title", "level", "cheese", "hat", "held"])


func test_the_chosen_look_comes_from_the_settings() -> void:
	Settings.set_value("look_cheese", "brie")
	Settings.set_value("look_hat", "chef")
	Settings.set_value("look_held", "balloon")
	assert_eq(Looks.unpack(Looks.chosen()), {"finish": "plain", "title": "none", "level": Progress.level(), "cheese": "brie", "hat": "chef", "held": "balloon"})
	Settings.set_value("look_hat", "sombrero")
	assert_push_error("can't be sombrero")
	assert_eq(Settings.get_value("look_hat"), "chef", "a hat that doesn't exist isn't taken")


func test_each_kind_of_cheese_wears_its_own_camouflage() -> void:
	var camo := character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with("Camo"))
	assert_gt(camo.size(), 0)
	character.set_colour(Color.BLUE)
	var colours := {}
	for cheese: String in Looks.CHEESES:
		character.wear(Looks.pack(cheese, "none", "none"))
		assert_eq(character.worn().cheese, cheese)
		assert_eq(camo.all(func(blotch: Node3D) -> bool: return blotch.visible), cheese != "swiss", "%s: camouflage" % cheese)
		assert_true(camo.all(func(blotch: Node3D) -> bool: return blotch.visible == camo[0].visible), "%s: all of it or none" % cheese)
		colours[cheese] = character._camo_material.albedo_color
	assert_lt(colours.cheddar.v, Color.BLUE.v, "a cheddar's is a darker shade of the uniform")
	assert_gt(colours.brie.v, 0.9, "a brie's is pale, whatever colour the uniform")
	assert_eq(colours.blue, CHARACTER.VEIN, "a blue's is the blue of its veins")


func test_a_hat_sits_on_the_helmet_and_one_thing_is_held() -> void:
	var top := BodyShape.HEAD_CENTRE.y + BodyShape.HEAD_SIZE.y / 2.0
	for hat: String in Looks.HATS:
		character.wear(Looks.pack("swiss", hat, "none"))
		var worn := character.get_node_or_null("Hat")
		if hat == "none":
			assert_null(worn)
			continue
		assert_not_null(worn, hat)
		assert_between(worn.position.y, top - 0.1, top, "%s: on the helmet" % hat)
		assert_gt(worn.find_children("*", "MeshInstance3D", true, false).size(), 0, hat)
	for held: String in Looks.HELD:
		character.wear(Looks.pack("swiss", "none", held))
		var carried := character.get_node_or_null("Held")
		assert_eq(carried == null, held == "none", held)
	character.wear(Looks.pack("swiss", "top", "flag"))
	character.wear(Looks.pack("swiss", "crown", "flag"))
	await wait_process_frames(2)
	assert_eq(character.get_children().filter(func(child: Node) -> bool: return child.name.begins_with("Hat")).size(), 1, "one hat at a time")


func test_no_look_changes_what_can_be_hit_or_leaves_the_players_own_space() -> void:
	var parts: Array = ["Torso", "Head", "Leg1", "Leg2"].map(func(part: String) -> Node3D: return character.get_node(part))
	var before := parts.map(_drawn_bounds)
	for look: int in _every_look():
		character.wear(look)
		assert_eq(parts.map(_drawn_bounds), before, "what is hit is the same")
		var bounds := AABB()
		var first := true
		for mesh: MeshInstance3D in character.find_children("*", "MeshInstance3D", true, false):
			if not mesh.is_visible_in_tree():
				continue
			var box: AABB = mesh.global_transform * mesh.get_aabb()
			bounds = box if first else bounds.merge(box)
			first = false
		var named := str(Looks.unpack(look))
		assert_gte(bounds.position.y, -0.01, named)
		assert_lte(bounds.end.y, BodyShape.HEIGHT, "%s: within the player's height" % named)
		assert_lte(maxf(absf(bounds.position.x), absf(bounds.end.x)), BodyShape.RADIUS + 0.05, "%s: within their width" % named)
		assert_lte(maxf(absf(bounds.position.z), absf(bounds.end.z)), BodyShape.RADIUS + 0.05, named)


# Where the meshes under `part` are drawn, all together.
func _drawn_bounds(part: Node3D) -> AABB:
	var meshes: Array = part.find_children("*", "MeshInstance3D", true, false)
	if part is MeshInstance3D:
		meshes.append(part)
	var bounds := AABB()
	for index in meshes.size():
		var mesh: MeshInstance3D = meshes[index]
		var box: AABB = mesh.global_transform * mesh.get_aabb()
		bounds = box if index == 0 else bounds.merge(box)
	return bounds


func test_a_hat_comes_down_with_a_crouch() -> void:
	character.wear(Looks.pack("swiss", "top", "balloon"))
	var stood: float = character.get_node("Hat").position.y
	var held: float = character.get_node("Held").position.y
	character.set_crouch(1.0)
	assert_almost_eq(character.get_node("Hat").position.y, stood - BodyShape.CROUCH_DROP, 0.0001)
	assert_almost_eq(character.get_node("Held").position.y, held - BodyShape.CROUCH_DROP, 0.0001)
	character.wear(Looks.pack("swiss", "crown", "none"))
	assert_almost_eq(character.get_node("Hat").position.y, stood - BodyShape.CROUCH_DROP, 0.0001, "a hat put on while crouched is on the head, not above it")


func test_a_look_survives_the_trip_with_the_rest_of_the_record() -> void:
	var state := MatchState.new()
	state.add_player(1).look = Looks.pack("blue", "cowboy", "lollipop")
	var copy := MatchState.from_dict(state.to_dict())
	assert_eq(Looks.unpack(copy.player(1).look), {"finish": "plain", "title": "none", "level": 0, "cheese": "blue", "hat": "cowboy", "held": "lollipop"})
	var numbers := PackedInt32Array()
	numbers.resize(PlayerRecord._COUNT)
	numbers.fill(0)
	numbers[PlayerRecord._LOOK] = 0x7FFFFFFF
	assert_eq(PlayerRecord.from_packed(["X", numbers]).look, Looks.clean(0x7FFFFFFF), "and nonsense is put right on arrival")


func test_the_host_takes_a_players_look_before_or_after_they_join() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var match_: Match = world.match_
	match_.start()
	var early := Looks.pack("cheddar", "party", "none")
	match_._wear(5, early)
	assert_null(match_.state.player(5), "not here yet")
	match_.add_player(5)
	assert_eq(match_.state.player(5).look, early, "worn from the moment they arrive")
	watch_signals(match_)
	var later := Looks.pack("brie", "crown", "flag")
	match_._wear(5, later)
	assert_eq(match_.state.player(5).look, later)
	assert_signal_emitted(match_, "state_changed", "everyone is told")
	match_._wear(5, "a trilby")
	assert_eq(match_.state.player(5).look, 0, "nonsense is the plain look")


func test_everyone_sees_what_a_player_wears() -> void:
	var world: Node3D = WORLD.instantiate()
	add_child_autofree(world)
	var match_: Match = world.match_
	match_.start()
	match_.add_player(2)
	var other: Node3D = world._create_player([2, Vector3.ZERO])
	world.players.add_child(other)
	assert_eq(other.body.worn().hat, "none")
	match_._wear(2, Looks.pack("blue", "chef", "balloon"))
	assert_eq(other.body.worn(), {"finish": "plain", "title": "none", "level": 0, "cheese": "blue", "hat": "chef", "held": "balloon"})
	assert_not_null(other.body.get_node_or_null("Hat"))


func test_the_settings_say_what_is_chosen_and_step_through_the_choices() -> void:
	const MENU := preload("res://ui/settings_menu.gd")
	assert_eq(MENU.look_text("look_hat"), "Hat: None")
	Settings.set_value("look_hat", "crown")
	assert_eq(MENU.look_text("look_hat"), "Hat: Crown")
	assert_eq(MENU.look_text("look_cheese"), "Cheese: Swiss")
	assert_eq(MENU.next_choice(Looks.HATS, "none"), "party")
	assert_eq(MENU.next_choice(Looks.HATS, Looks.HATS[Looks.HATS.size() - 1]), "none", "and round again")
	var menu: Control = MENU.new()
	add_child_autofree(menu)
	menu.refresh()
	var button: Button = menu.find_child("Setting_look_cheese", true, false)
	assert_eq(button.text, "Cheese: Swiss")
	button.pressed.emit()
	assert_eq(Settings.get_value("look_cheese"), "cheddar")
